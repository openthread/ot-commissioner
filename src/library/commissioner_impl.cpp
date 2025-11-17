/*
 *    Copyright (c) 2019, The OpenThread Commissioner Authors.
 *    All rights reserved.
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *    3. Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *    POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   The file implements the Commissioner interface.
 */

#include "library/commissioner_impl.hpp"

#include <sys/types.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "commissioner/commissioner.hpp"
#include "commissioner/defines.hpp"
#include "commissioner/error.hpp"
#include "commissioner/network_data.hpp"
#include "commissioner/network_diag_data.hpp"
#include "common/address.hpp"
#include "common/error_macros.hpp"
#include "common/logging.hpp"
#include "common/time.hpp"
#include "common/utils.hpp"
#include "event2/event.h"
#include "library/coap.hpp"
#include "library/commissioner_impl_internal.hpp"
#include "library/dtls.hpp"
#include "library/joiner_session.hpp"
#include "library/openthread/bloom_filter.hpp"
#include "library/openthread/pbkdf2_cmac.hpp"
#include "library/openthread/sha256.hpp"
#include "library/timer.hpp"
#include "library/tlv.hpp"
#include "library/uri.hpp"

#define CCM_NOT_IMPLEMENTED "CCM features not implemented"

namespace ot {

namespace commissioner {

static constexpr uint16_t kLeaderAloc16     = 0xFC00;
static constexpr uint16_t kPrimaryBbrAloc16 = 0xFC38;

static constexpr uint32_t kMinKeepAliveInterval = 30;
static constexpr uint32_t kMaxKeepAliveInterval = 45;

Error Commissioner::GeneratePSKc(ByteArray         &aPSKc,
                                 const std::string &aPassphrase,
                                 const std::string &aNetworkName,
                                 const ByteArray   &aExtendedPanId)
{
    Error             error;
    const std::string saltPrefix = "Thread";
    ByteArray         salt;

    VerifyOrExit((aPassphrase.size() >= kMinCommissionerCredentialLength) &&
                     (aPassphrase.size() <= kMaxCommissionerCredentialLength),
                 error = ERROR_INVALID_ARGS("passphrase length={} exceeds range [{}, {}]", aPassphrase.size(),
                                            kMinCommissionerCredentialLength, kMaxCommissionerCredentialLength));
    VerifyOrExit(aNetworkName.size() <= kMaxNetworkNameLength,
                 error = ERROR_INVALID_ARGS("network name length={} > {}", aNetworkName.size(), kMaxNetworkNameLength));
    VerifyOrExit(
        aExtendedPanId.size() == kExtendedPanIdLength,
        error = ERROR_INVALID_ARGS("extended PAN ID length={} != {}", aExtendedPanId.size(), kExtendedPanIdLength));

    salt.insert(salt.end(), saltPrefix.begin(), saltPrefix.end());
    salt.insert(salt.end(), aExtendedPanId.begin(), aExtendedPanId.end());
    salt.insert(salt.end(), aNetworkName.begin(), aNetworkName.end());

    aPSKc.resize(kMaxPSKcLength);
    otPbkdf2Cmac(reinterpret_cast<const uint8_t *>(aPassphrase.data()), static_cast<uint16_t>(aPassphrase.size()),
                 salt.data(), static_cast<uint16_t>(salt.size()), 16384, static_cast<uint16_t>(aPSKc.size()),
                 aPSKc.data());

exit:
    return error;
}

ByteArray Commissioner::ComputeJoinerId(uint64_t aEui64)
{
    Sha256    sha256;
    uint8_t   hash[Sha256::kHashSize];
    ByteArray eui64;

    utils::Encode(eui64, aEui64);

    sha256.Start();
    sha256.Update(&eui64[0], eui64.size());
    sha256.Finish(hash);

    static_assert(sizeof(hash) >= kJoinerIdLength, "wrong crypto::Sha256::kHashSize value");

    ByteArray joinerId{hash, hash + kJoinerIdLength};
    joinerId[0] |= kLocalExternalAddrMask;
    return joinerId;
}

void Commissioner::AddJoiner(ByteArray &aSteeringData, const ByteArray &aJoinerId)
{
    if (aSteeringData.size() != kMaxSteeringDataLength)
    {
        aSteeringData.resize(kMaxSteeringDataLength);
        std::fill(aSteeringData.begin(), aSteeringData.end(), 0);
    }
    ComputeBloomFilter(aSteeringData, aJoinerId);
}

std::string Commissioner::GetVersion(void) { return OT_COMM_VERSION; }

CommissionerImpl::CommissionerImpl(CommissionerHandler &aHandler, struct event_base *aEventBase)
    : mState(State::kDisabled)
    , mSessionId(0)
    , mCommissionerHandler(aHandler)
    , mEventBase(aEventBase)
    , mKeepAliveTimer(mEventBase, [this](Timer &aTimer) { SendKeepAlive(aTimer); })
    , mBrClient(mEventBase)
    , mJoinerSessionTimer(mEventBase, [this](Timer &aTimer) { HandleJoinerSessionTimer(aTimer); })
    , mResourceUdpRx(uri::kUdpRx, [this](const coap::Request &aRequest) { mProxyClient.HandleUdpRx(aRequest); })
    , mResourceRlyRx(uri::kRelayRx, [this](const coap::Request &aRequest) { HandleRlyRx(aRequest); })
    , mProxyClient(*this, mBrClient)
#if OT_COMM_CONFIG_CCM_ENABLE
    , mTokenManager(mEventBase)
#endif
    , mResourceDatasetChanged(uri::kMgmtDatasetChanged,
                              [this](const coap::Request &aRequest) { HandleDatasetChanged(aRequest); })
    , mResourcePanIdConflict(uri::kMgmtPanidConflict,
                             [this](const coap::Request &aRequest) { HandlePanIdConflict(aRequest); })
    , mResourceEnergyReport(uri::kMgmtEdReport, [this](const coap::Request &aRequest) { HandleEnergyReport(aRequest); })
    , mResourceDiagAns(uri::kDiagGetAns, [this](const coap::Request &aRequest) { HandleDiagGetAnswer(aRequest); })
{
    SuccessOrDie(mBrClient.AddResource(mResourceUdpRx));
    SuccessOrDie(mBrClient.AddResource(mResourceRlyRx));
    SuccessOrDie(mProxyClient.AddResource(mResourceDatasetChanged));
    SuccessOrDie(mProxyClient.AddResource(mResourcePanIdConflict));
    SuccessOrDie(mProxyClient.AddResource(mResourceEnergyReport));
    SuccessOrDie(mProxyClient.AddResource(mResourceDiagAns));
}

Error CommissionerImpl::Init(const Config &aConfig)
{
    Error error;

    SuccessOrExit(error = ValidateConfig(aConfig));
    mConfig = aConfig;

    InitLogger(aConfig.mLogger);
    LoggingConfig();

    SuccessOrExit(error = mBrClient.Init(GetDtlsConfig(mConfig)));

#if OT_COMM_CONFIG_CCM_ENABLE
    if (IsCcmMode())
    {
        // It is not good to leave the token manager uninitialized in non-CCM mode.
        // TODO(wgtdkp): create TokenManager only in CCM Mode.
        SuccessOrExit(error = mTokenManager.Init(mConfig));
    }
#endif

exit:
    return error;
}

Error CommissionerImpl::ValidateConfig(const Config &aConfig)
{
    Error error;

    {
        tlv::Tlv commissionerIdTlv{tlv::Type::kCommissionerId, aConfig.mId};
        VerifyOrExit(!aConfig.mId.empty(), error = ERROR_INVALID_ARGS("commissioner ID is mandatory"));
        VerifyOrExit(commissionerIdTlv.IsValid(),
                     error = ERROR_INVALID_ARGS("{} is not a valid Commissioner ID", aConfig.mId));
    }

    VerifyOrExit(
        (aConfig.mKeepAliveInterval >= kMinKeepAliveInterval && aConfig.mKeepAliveInterval <= kMaxKeepAliveInterval),
        error = ERROR_INVALID_ARGS("keep-alive internal {} exceeds range [{}, {}]", aConfig.mKeepAliveInterval,
                                   kMinKeepAliveInterval, kMaxKeepAliveInterval));

    if (aConfig.mEnableCcm)
    {
        tlv::Tlv domainNameTlv{tlv::Type::kDomainName, aConfig.mDomainName};

#if !OT_COMM_CONFIG_CCM_ENABLE
        ExitNow(error = ERROR_INVALID_ARGS(CCM_NOT_IMPLEMENTED));
#endif

        VerifyOrExit(!aConfig.mDomainName.empty(), error = ERROR_INVALID_ARGS("missing Domain Name for CCM network"));
        VerifyOrExit(domainNameTlv.IsValid(),
                     error = ERROR_INVALID_ARGS("Domain Name is too long (length={})", aConfig.mDomainName.size()));
        VerifyOrExit(!aConfig.mPrivateKey.empty(),
                     error = ERROR_INVALID_ARGS("missing Private Key file for CCM network"));
        VerifyOrExit(!aConfig.mCertificate.empty(),
                     error = ERROR_INVALID_ARGS("missing Certificate file for CCM network"));
        VerifyOrExit(!aConfig.mTrustAnchor.empty(),
                     error = ERROR_INVALID_ARGS("missing Trust Anchor file for CCM network"));
    }
    else
    {
        // Should we also enable setting PSKc from passphrase?
        VerifyOrExit(!aConfig.mPSKc.empty(), error = ERROR_INVALID_ARGS("missing PSKc for non-CCM network"));
        VerifyOrExit(aConfig.mPSKc.size() <= kMaxPSKcLength,
                     error = ERROR_INVALID_ARGS("PSKc is too long (length={})", aConfig.mPSKc.size()));
    }

exit:
    return error;
}

void CommissionerImpl::LoggingConfig()
{
    LOG_INFO(LOG_REGION_CONFIG, "Id = {}", mConfig.mId);
    LOG_INFO(LOG_REGION_CONFIG, "enable CCM = {}", mConfig.mEnableCcm);
    LOG_INFO(LOG_REGION_CONFIG, "domain name = {}", mConfig.mDomainName);
    LOG_INFO(LOG_REGION_CONFIG, "keep alive interval = {}", mConfig.mKeepAliveInterval);
    LOG_INFO(LOG_REGION_CONFIG, "enable DTLS debug logging = {}", mConfig.mEnableDtlsDebugLogging);
    LOG_INFO(LOG_REGION_CONFIG, "maximum connection number = {}", mConfig.mMaxConnectionNum);

    // Do not logging credentials
}

const Config &CommissionerImpl::GetConfig() const { return mConfig; }

void CommissionerImpl::Petition(PetitionHandler aHandler, const std::string &aAddr, uint16_t aPort)
{
    Error error;

    auto onConnected = [&, aHandler](Error aError) {
        if (aError != ErrorCode::kNone)
        {
            aHandler(nullptr, aError);
        }
        else
        {
            LOG_DEBUG(LOG_REGION_MESHCOP, "DTLS connection to border agent succeed");
            this->mState = State::kConnected;
            SendPetition(aHandler);
        }
    };

    VerifyOrExit(!IsActive(), error = ERROR_INVALID_STATE("cannot petition when the commissioner is running"));
    LOG_DEBUG(LOG_REGION_MESHCOP, "starting petition: border agent = ({}, {})", aAddr, aPort);

    if (mBrClient.IsConnected())
    {
        SendPetition(aHandler);
    }
    else
    {
        Connect(onConnected, aAddr, aPort);
    }

exit:
    if (error != ErrorCode::kNone)
    {
        aHandler(nullptr, error);
    }
}

void CommissionerImpl::Resign(ErrorHandler aHandler)
{
    if (IsActive())
    {
        SendKeepAlive(mKeepAliveTimer, false);
    }

    if (mKeepAliveTimer.IsRunning())
    {
        mKeepAliveTimer.Stop();
    }

    aHandler(ERROR_NONE);
}

void CommissionerImpl::Connect(ErrorHandler aHandler, const std::string &aAddr, uint16_t aPort)
{
    auto onConnected = [&, aHandler](const DtlsSession &, Error aError) {
        if (aError == ErrorCode::kNone)
        {
            this->mState = State::kConnected;
        }
        aHandler(aError);
    };
    mBrClient.Connect(onConnected, aAddr, aPort);
}

void CommissionerImpl::Disconnect()
{
    mBrClient.Disconnect(ERROR_CANCELLED("the CoAPs client was disconnected"));
    mProxyClient.ClearMeshLocalPrefix();
    mState = State::kDisabled;
}

uint16_t CommissionerImpl::GetSessionId() const { return mSessionId; }

State CommissionerImpl::GetState() const { return mState; }

bool CommissionerImpl::IsActive() const { return GetState() == State::kActive; }

bool CommissionerImpl::IsCcmMode() const { return mConfig.mEnableCcm; }

const std::string &CommissionerImpl::GetDomainName() const { return mConfig.mDomainName; }

void CommissionerImpl::CancelRequests()
{
    mProxyClient.CancelRequests();
    mBrClient.CancelRequests();

#if OT_COMM_CONFIG_CCM_ENABLE
    if (IsCcmMode())
    {
        mTokenManager.CancelRequests();
    }
#endif
}

void CommissionerImpl::GetCommissionerDataset(Handler<CommissionerDataset> aHandler, uint16_t aDatasetFlags)
{
    Error         error;
    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};
    ByteArray     tlvTypes = GetCommissionerDatasetTlvs(aDatasetFlags);

    auto onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        Error               error;
        CommissionerDataset dataset;

        SuccessOrExit(error = aError);
        SuccessOrExit(error = CheckCoapResponseCode(*aResponse));

        SuccessOrExit(error = DecodeCommissionerDataset(dataset, *aResponse));

        aHandler(&dataset, error);

    exit:
        if (error != ErrorCode::kNone)
        {
            aHandler(nullptr, error);
        }
    };

    SuccessOrExit(error = request.SetUriPath(uri::kMgmtCommissionerGet));

    // If Get TLV is not present, get all Commissioner Dataset TLVs.
    if (!tlvTypes.empty())
    {
        SuccessOrExit(AppendTlv(request, {tlv::Type::kGet, tlvTypes}));
    }

    mProxyClient.SendRequest(request, onResponse, kLeaderAloc16, kDefaultMmPort);

    LOG_DEBUG(LOG_REGION_MGMT, "sent MGMT_COMMISSIONER_GET.req");

exit:
    if (error != ErrorCode::kNone)
    {
        aHandler(nullptr, error);
    }
}

void CommissionerImpl::SetCommissionerDataset(ErrorHandler aHandler, const CommissionerDataset &aDataset)
{
    Error         error;
    auto          dataset = aDataset;
    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};

    auto onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        aHandler(HandleStateResponse(aResponse, aError));
    };

    dataset.mPresentFlags &= ~CommissionerDataset::kSessionIdBit;
    dataset.mPresentFlags &= ~CommissionerDataset::kBorderAgentLocatorBit;
    VerifyOrExit(dataset.mPresentFlags != 0, error = ERROR_INVALID_ARGS("empty Commissioner Dataset"));

    // TODO(wgtdkp): verify if every joiner UDP port differs from each other (required by Thread).
    //               Otherwise, this request may fail.

    SuccessOrExit(error = request.SetUriPath(uri::kMgmtCommissionerSet));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kCommissionerSessionId, GetSessionId()}));
    SuccessOrExit(error = EncodeCommissionerDataset(request, dataset));

#if OT_COMM_CONFIG_CCM_ENABLE
    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }
#endif

    mProxyClient.SendRequest(request, onResponse, kLeaderAloc16, kDefaultMmPort);

    LOG_DEBUG(LOG_REGION_MGMT, "sent MGMT_COMMISSIONER_SET.req");

exit:
    if (error != ErrorCode::kNone)
    {
        aHandler(error);
    }
}

void CommissionerImpl::GetActiveDataset(Handler<ActiveOperationalDataset> aHandler, uint16_t aDatasetFlags)
{
    auto rawDatasetHandler = [aHandler](const ByteArray *aRawDataset, Error aError) {
        Error                    error;
        ActiveOperationalDataset dataset;

        SuccessOrExit(error = aError);
        SuccessOrExit(error = DecodeActiveOperationalDataset(dataset, *aRawDataset));

        aHandler(&dataset, error);
    exit:
        if (error != ErrorCode::kNone)
        {
            aHandler(nullptr, error);
        }
    };

    GetRawActiveDataset(rawDatasetHandler, aDatasetFlags);
}

void CommissionerImpl::GetRawActiveDataset(Handler<ByteArray> aHandler, uint16_t aDatasetFlags)
{
    Error         error;
    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};
    ByteArray     datasetList = GetActiveOperationalDatasetTlvs(aDatasetFlags);

    auto onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        Error error;

        SuccessOrExit(error = aError);
        SuccessOrExit(error = CheckCoapResponseCode(*aResponse));

        aHandler(&aResponse->GetPayload(), error);

    exit:
        if (error != ErrorCode::kNone)
        {
            aHandler(nullptr, error);
        }
    };

    VerifyOrExit(IsActiveOrConnected(),
                 error = ERROR_INVALID_STATE("the commissioner is not active or secure session is not connected"));
    SuccessOrExit(error = request.SetUriPath(uri::kMgmtActiveGet));
    if (!datasetList.empty())
    {
        SuccessOrExit(error = AppendTlv(request, {tlv::Type::kGet, datasetList}));
    }

#if OT_COMM_CONFIG_CCM_ENABLE
    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }
#endif

    // Send MGMT_ACTIVE_GET.req to the Border Agent but not the Leader,
    // because we don't possess the Mesh-Local Prefix before getting the
    // Active Operational Dataset.
    mBrClient.SendRequest(request, onResponse);

    LOG_DEBUG(LOG_REGION_MGMT, "sent MGMT_ACTIVE_GET.req");

exit:
    if (error != ErrorCode::kNone)
    {
        aHandler(nullptr, error);
    }
}

void CommissionerImpl::SetActiveDataset(ErrorHandler aHandler, const ActiveOperationalDataset &aActiveDataset)
{
    Error         error;
    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};

    auto onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        aHandler(HandleStateResponse(aResponse, aError));
    };

    VerifyOrExit(aActiveDataset.mPresentFlags & ActiveOperationalDataset::kActiveTimestampBit,
                 error = ERROR_INVALID_ARGS("Active Timestamp is mandatory for an Active Operational Dataset"));

    // TLVs affect connectivity are not allowed.
    VerifyOrExit((aActiveDataset.mPresentFlags & ActiveOperationalDataset::kChannelBit) == 0,
                 error = ERROR_INVALID_ARGS("Channel cannot be set with Active Operational Dataset, "
                                            "try setting with Pending Operational Dataset instead"));
    VerifyOrExit((aActiveDataset.mPresentFlags & ActiveOperationalDataset::kPanIdBit) == 0,
                 error = ERROR_INVALID_ARGS("PAN ID cannot be set with Active Operational Dataset, "
                                            "try setting with Pending Operational Dataset instead"));
    VerifyOrExit((aActiveDataset.mPresentFlags & ActiveOperationalDataset::kMeshLocalPrefixBit) == 0,
                 error = ERROR_INVALID_ARGS("Mesh-Local Prefix cannot be set with Active Operational Dataset, "
                                            "try setting with Pending Operational Dataset instead"));
    VerifyOrExit((aActiveDataset.mPresentFlags & ActiveOperationalDataset::kNetworkMasterKeyBit) == 0,
                 error = ERROR_INVALID_ARGS("Network Master Key cannot be set with Active Operational Dataset, "
                                            "try setting with Pending Operational Dataset instead"));

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));
    SuccessOrExit(error = request.SetUriPath(uri::kMgmtActiveSet));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kCommissionerSessionId, GetSessionId()}));
    SuccessOrExit(error = EncodeActiveOperationalDataset(request, aActiveDataset));

#if OT_COMM_CONFIG_CCM_ENABLE
    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }
#endif

    mProxyClient.SendRequest(request, onResponse, kLeaderAloc16, kDefaultMmPort);

    LOG_DEBUG(LOG_REGION_MGMT, "sent MGMT_ACTIVE_SET.req");

exit:
    if (error != ErrorCode::kNone)
    {
        aHandler(error);
    }
}

void CommissionerImpl::GetPendingDataset(Handler<PendingOperationalDataset> aHandler, uint16_t aDatasetFlags)
{
    Error         error;
    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};
    ByteArray     datasetList = GetPendingOperationalDatasetTlvs(aDatasetFlags);

    auto onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        Error                     error;
        PendingOperationalDataset dataset;

        SuccessOrExit(error = aError);
        SuccessOrExit(error = CheckCoapResponseCode(*aResponse));

        SuccessOrExit(error = DecodePendingOperationalDataset(dataset, *aResponse));
        if (dataset.mPresentFlags != 0)
        {
            VerifyOrExit(dataset.mPresentFlags & PendingOperationalDataset::kDelayTimerBit,
                         error = ERROR_BAD_FORMAT("Delay Timer is not included in MGMT_PENDING_GET.rsp"));
        }

        aHandler(&dataset, error);

    exit:
        if (error != ErrorCode::kNone)
        {
            aHandler(nullptr, error);
        }
    };

    VerifyOrExit(IsActiveOrConnected(),
                 error = ERROR_INVALID_STATE("the commissioner is not active or secure session is not connected"));
    SuccessOrExit(error = request.SetUriPath(uri::kMgmtPendingGet));
    if (!datasetList.empty())
    {
        SuccessOrExit(error = AppendTlv(request, {tlv::Type::kGet, datasetList}));
    }

#if OT_COMM_CONFIG_CCM_ENABLE
    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }
#endif

    mBrClient.SendRequest(request, onResponse);

    LOG_DEBUG(LOG_REGION_MGMT, "sent MGMT_PENDING_GET.req");

exit:
    if (error != ErrorCode::kNone)
    {
        aHandler(nullptr, error);
    }
}

void CommissionerImpl::CommandDiagGetQuery(ErrorHandler aHandler, const std::string &aAddr, uint64_t aDiagDataFlags)
{
    Error         error;
    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};
    auto          onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        Error error;

        SuccessOrExit(error = aError);
        SuccessOrExit(error = CheckCoapResponseCode(*aResponse));

    exit:
        aHandler(error);
    };

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("commissioner is not active"));
    SuccessOrExit(error = request.SetUriPath(uri::kDiagGetQuery));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kNetworkDiagTypeList, GetNetDiagTlvTypes(aDiagDataFlags),
                                              tlv::Scope::kNetworkDiag}));

#if OT_COMM_CONFIG_CCM_ENABLE
    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }
#endif

    LOG_INFO(LOG_REGION_MESHDIAG, "sending DIAG_GET.qry");
    if (aAddr.empty())
    {
        mProxyClient.SendRequest(request, onResponse, kLeaderAloc16, kDefaultMmPort);
    }
    else
    {
        Address dstAddr;
        SuccessOrExit(error = dstAddr.Set(aAddr));
        mProxyClient.SendRequest(request, onResponse, dstAddr, kDefaultMmPort);
    }
    LOG_DEBUG(LOG_REGION_MESHDIAG, "sent DIAG_GET.qry");

exit:
    if (error != ErrorCode::kNone)
    {
        aHandler(error);
    }
}

void CommissionerImpl::HandleDiagGetAnswer(const coap::Request &aRequest)
{
    Error       error;
    std::string peerAddr = aRequest.GetEndpoint()->GetPeerAddr().ToString();

    LOG_INFO(LOG_REGION_MESHDIAG, "received DIAG_GET.ans from {}", peerAddr);
    mProxyClient.SendEmptyChanged(aRequest);

    SuccessOrExit(error = internal::DecodeNetDiagData(mDiagAnsTlvs, aRequest.GetPayload()));
    LOG_INFO(LOG_REGION_MESHDIAG, "accepted DIAG_GET.ans data {}", utils::Hex(aRequest.GetPayload()));

    mCommissionerHandler.OnDiagGetAnswerMessage(peerAddr, mDiagAnsTlvs);

exit:
    if (error != ErrorCode::kNone)
    {
        LOG_ERROR(LOG_REGION_MESHDIAG, "failed to handle DIAG_GET.ans from {}", peerAddr);
    }
}

void CommissionerImpl::CommandDiagReset(ErrorHandler aHandler, const std::string &aAddr, uint64_t aDiagDataFlags)
{
    Error         error;
    Address       dstAddr;
    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};
    auto          onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        Error error;

        SuccessOrExit(error = aError);
        SuccessOrExit(error = CheckCoapResponseCode(*aResponse));

    exit:
        aHandler(error);
    };

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("commissioner is not active"));
    SuccessOrExit(error = request.SetUriPath(uri::kDiagRst));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kNetworkDiagTypeList, GetNetDiagTlvTypes(aDiagDataFlags),
                                              tlv::Scope::kNetworkDiag}));

#if OT_COMM_CONFIG_CCM_ENABLE
    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }
#endif

    LOG_INFO(LOG_REGION_MESHDIAG, "sending DIAG_RST.ntf");
    if (aAddr.empty())
    {
        mProxyClient.SendRequest(request, onResponse, kLeaderAloc16, kDefaultMmPort);
    }
    else
    {
        SuccessOrExit(error = dstAddr.Set(aAddr));
        mProxyClient.SendRequest(request, onResponse, dstAddr, kDefaultMmPort);
    }
    LOG_INFO(LOG_REGION_MESHDIAG, "sent DIAG_RST.ntf");

exit:
    if (error != ErrorCode::kNone)
    {
        aHandler(error);
    }
}

void CommissionerImpl::SetPendingDataset(ErrorHandler aHandler, const PendingOperationalDataset &aPendingDataset)
{
    Error         error;
    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};

    auto onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        aHandler(HandleStateResponse(aResponse, aError));
    };

    VerifyOrExit(aPendingDataset.mPresentFlags & PendingOperationalDataset::kActiveTimestampBit,
                 error = ERROR_INVALID_ARGS("Active Timestamp is mandatory for a Pending Operational Dataset"));

    VerifyOrExit(aPendingDataset.mPresentFlags & PendingOperationalDataset::kPendingTimestampBit,
                 error = ERROR_INVALID_ARGS("Pending Timestamp is mandatory for a Pending Operational Dataset"));
    VerifyOrExit(aPendingDataset.mPresentFlags & PendingOperationalDataset::kDelayTimerBit,
                 error = ERROR_INVALID_ARGS("Delay Timer is mandatory for a Pending Operational Dataset"));

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));
    SuccessOrExit(error = request.SetUriPath(uri::kMgmtPendingSet));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kCommissionerSessionId, GetSessionId()}));
    SuccessOrExit(error = EncodePendingOperationalDataset(request, aPendingDataset));

#if OT_COMM_CONFIG_CCM_ENABLE
    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }
#endif

    mProxyClient.SendRequest(request, onResponse, kLeaderAloc16, kDefaultMmPort);

    LOG_DEBUG(LOG_REGION_MGMT, "sent MGMT_PENDING_SET.req");

exit:
    if (error != ErrorCode::kNone)
    {
        aHandler(error);
    }
}

#if OT_COMM_CONFIG_CCM_ENABLE
void CommissionerImpl::SetBbrDataset(ErrorHandler aHandler, const BbrDataset &aDataset)
{
    Error error;

    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};

    auto onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        aHandler(HandleStateResponse(aResponse, aError));
    };

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));
    VerifyOrExit(IsCcmMode(), error = ERROR_INVALID_STATE("sending MGMT_BBR_SET.req is only valid in CCM mode"));
    VerifyOrExit((aDataset.mPresentFlags & BbrDataset::kRegistrarIpv6AddrBit) == 0,
                 error = ERROR_INVALID_ARGS("trying to set Registrar IPv6 Address which is read-only"));

    SuccessOrExit(error = request.SetUriPath(uri::kMgmtBbrSet));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kCommissionerSessionId, GetSessionId()}));
    SuccessOrExit(error = EncodeBbrDataset(request, aDataset));

    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }

    mProxyClient.SendRequest(request, onResponse, kLeaderAloc16, kDefaultMmPort);

    LOG_DEBUG(LOG_REGION_MGMT, "sent MGMT_BBR_SET.req");

exit:
    if (error != ErrorCode::kNone)
    {
        aHandler(error);
    }
}

void CommissionerImpl::GetBbrDataset(Handler<BbrDataset> aHandler, uint16_t aDatasetFlags)
{
    Error error;

    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};
    ByteArray     datasetList = GetBbrDatasetTlvs(aDatasetFlags);

    auto onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        Error      error;
        BbrDataset dataset;

        SuccessOrExit(error = aError);
        SuccessOrExit(error = CheckCoapResponseCode(*aResponse));

        SuccessOrExit(error = DecodeBbrDataset(dataset, *aResponse));

        aHandler(&dataset, error);

    exit:
        if (error != ErrorCode::kNone)
        {
            aHandler(nullptr, error);
        }
    };

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));
    VerifyOrExit(IsCcmMode(), error = ERROR_INVALID_STATE("sending MGMT_BBR_GET.req is only valid in CCM mode"));

    SuccessOrExit(error = request.SetUriPath(uri::kMgmtBbrGet));

    if (!datasetList.empty())
    {
        SuccessOrExit(error = AppendTlv(request, {tlv::Type::kGet, datasetList}));
    }

    mProxyClient.SendRequest(request, onResponse, kLeaderAloc16, kDefaultMmPort);

    LOG_DEBUG(LOG_REGION_MGMT, "sent MGMT_BBR_GET.req");

exit:
    if (error != ErrorCode::kNone)
    {
        aHandler(nullptr, error);
    }
}

void CommissionerImpl::SetSecurePendingDataset(ErrorHandler                     aHandler,
                                               uint32_t                         aMaxRetrievalTimer,
                                               const PendingOperationalDataset &aDataset)
{
    Error         error;
    Address       pbbrAddr;
    ByteArray     secureDissemination;
    std::string   uri;
    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};

    auto onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        aHandler(HandleStateResponse(aResponse, aError));
    };

    auto onMeshLocalPrefixResponse = [=](Error aError) {
        if (aError == ErrorCode::kNone)
        {
            SetSecurePendingDataset(aHandler, aMaxRetrievalTimer, aDataset);
        }
        else
        {
            aHandler(aError);
        }
    };

    // Delay timer is mandatory.
    VerifyOrExit(aDataset.mPresentFlags & PendingOperationalDataset::kDelayTimerBit,
                 error = ERROR_INVALID_ARGS("Delay Timer is mandatory for a Secure Pending Operational Dataset"));

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));
    VerifyOrExit(IsCcmMode(),
                 error = ERROR_INVALID_STATE("sending MGMT_SEC_PENDING_SET.req is only valid in CCM mode"));

    if (mProxyClient.GetMeshLocalPrefix().empty())
    {
        mProxyClient.FetchMeshLocalPrefix(onMeshLocalPrefixResponse);
        ExitNow();
    }

    SuccessOrExit(error = request.SetUriPath(uri::kMgmtSecPendingSet));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kCommissionerSessionId, GetSessionId()}));

    pbbrAddr = mProxyClient.GetAnycastLocator(kPrimaryBbrAloc16);
    uri      = "coaps://[" + pbbrAddr.ToString() + "]" + uri::kMgmtPendingGet;

    utils::Encode(secureDissemination, aDataset.mPendingTimestamp.Encode());
    utils::Encode(secureDissemination, aMaxRetrievalTimer);
    secureDissemination.insert(secureDissemination.end(), uri.begin(), uri.end());
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kSecureDissemination, secureDissemination}));

    SuccessOrExit(error = EncodePendingOperationalDataset(request, aDataset));

    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }

    mProxyClient.SendRequest(request, onResponse, pbbrAddr, kDefaultMmPort);

    LOG_DEBUG(LOG_REGION_MGMT, "sent MGMT_SEC_PENDING_SET.req");

exit:
    if (error != ErrorCode::kNone)
    {
        aHandler(error);
    }
}

void CommissionerImpl::CommandReenroll(ErrorHandler aHandler, const std::string &aDstAddr)
{
    Error error;

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));
    VerifyOrExit(IsCcmMode(), error = ERROR_INVALID_STATE("en-enroll a device is not in CCM Mode"));
    SendProxyMessage(aHandler, aDstAddr, uri::kMgmtReenroll);

exit:
    if (error != ERROR_NONE)
    {
        aHandler(error);
    }
}

void CommissionerImpl::CommandDomainReset(ErrorHandler aHandler, const std::string &aDstAddr)
{
    Error error;

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));
    VerifyOrExit(IsCcmMode(), error = ERROR_INVALID_STATE("resetting a device is not in CCM Mode"));
    SendProxyMessage(aHandler, aDstAddr, uri::kMgmtDomainReset);

exit:
    if (error != ERROR_NONE)
    {
        aHandler(error);
    }
}

void CommissionerImpl::CommandMigrate(ErrorHandler       aHandler,
                                      const std::string &aDstAddr,
                                      const std::string &aDstNetworkName)
{
    Error         error;
    Address       dstAddr;
    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};

    auto onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        aHandler(HandleStateResponse(aResponse, aError, /* aStateTlvIsMandatory */ false));
    };

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));
    VerifyOrExit(IsCcmMode(), error = ERROR_INVALID_STATE("Migrating a Device is only valid in CCM Mode"));

    SuccessOrExit(error = dstAddr.Set(aDstAddr));
    VerifyOrExit(aDstNetworkName.size() <= kMaxNetworkNameLength,
                 error =
                     ERROR_INVALID_ARGS("Network Name length={} > {}", aDstNetworkName.size(), kMaxNetworkNameLength));

    SuccessOrExit(error = request.SetUriPath(uri::kMgmtNetMigrate));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kCommissionerSessionId, GetSessionId()}));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kNetworkName, aDstNetworkName}));

    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }

    mProxyClient.SendRequest(request, onResponse, dstAddr, kDefaultMmPort);

    LOG_DEBUG(LOG_REGION_MGMT, "sent MGMT_NET_MIGRATE.req");

exit:
    if (error != ErrorCode::kNone)
    {
        aHandler(error);
    }
}

void CommissionerImpl::RequestToken(Handler<ByteArray> aHandler, const std::string &aAddr, uint16_t aPort)
{
    if (!IsCcmMode())
    {
        aHandler(nullptr, ERROR_INVALID_STATE("requesting COM_TOK is only valid in CCM Mode"));
    }
    else
    {
        mTokenManager.RequestToken(aHandler, aAddr, aPort);
    }
}

Error CommissionerImpl::SetToken(const ByteArray &aSignedToken)
{
    Error error;

    VerifyOrExit(IsCcmMode(), error = ERROR_INVALID_STATE("setting COM_TOK in only valid in CCM Mode"));
    error = mTokenManager.SetToken(aSignedToken, /* aAlwaysAccept */ true);

exit:
    return error;
}
#else
void CommissionerImpl::SetBbrDataset(ErrorHandler aHandler, const BbrDataset &aDataset)
{
    (void)aDataset;
    aHandler(ERROR_UNIMPLEMENTED(CCM_NOT_IMPLEMENTED));
}

void CommissionerImpl::GetBbrDataset(Handler<BbrDataset> aHandler, uint16_t aDatasetFlags)
{
    (void)aDatasetFlags;
    aHandler(nullptr, ERROR_UNIMPLEMENTED(CCM_NOT_IMPLEMENTED));
}

void CommissionerImpl::SetSecurePendingDataset(ErrorHandler                     aHandler,
                                               uint32_t                         aMaxRetrievalTimer,
                                               const PendingOperationalDataset &aDataset)
{
    (void)aMaxRetrievalTimer;
    (void)aDataset;
    aHandler(ERROR_UNIMPLEMENTED(CCM_NOT_IMPLEMENTED));
}

void CommissionerImpl::CommandReenroll(ErrorHandler aHandler, const std::string &aDstAddr)
{
    (void)aDstAddr;
    aHandler(ERROR_UNIMPLEMENTED(CCM_NOT_IMPLEMENTED));
}

void CommissionerImpl::CommandDomainReset(ErrorHandler aHandler, const std::string &aDstAddr)
{
    (void)aDstAddr;
    aHandler(ERROR_UNIMPLEMENTED(CCM_NOT_IMPLEMENTED));
}

void CommissionerImpl::CommandMigrate(ErrorHandler       aHandler,
                                      const std::string &aDstAddr,
                                      const std::string &aDstNetworkName)
{
    (void)aDstAddr;
    (void)aDstNetworkName;
    aHandler(ERROR_UNIMPLEMENTED(CCM_NOT_IMPLEMENTED));
}

void CommissionerImpl::RequestToken(Handler<ByteArray> aHandler, const std::string &aAddr, uint16_t aPort)
{
    (void)aAddr;
    (void)aPort;
    aHandler(nullptr, ERROR_UNIMPLEMENTED(CCM_NOT_IMPLEMENTED));
}

Error CommissionerImpl::SetToken(const ByteArray &aSignedToken)
{
    (void)aSignedToken;
    return ERROR_UNIMPLEMENTED(CCM_NOT_IMPLEMENTED);
}
#endif // OT_COMM_CONFIG_CCM_ENABLE

void CommissionerImpl::RegisterMulticastListener(Handler<uint8_t>                aHandler,
                                                 const std::vector<std::string> &aMulticastAddrList,
                                                 uint32_t                        aTimeout)
{
    Error     error;
    Address   multicastAddr;
    ByteArray rawAddresses;

    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};

    auto onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        Error       error;
        tlv::TlvPtr statusTlv = nullptr;
        uint8_t     status;

        SuccessOrExit(error = aError);

#if OT_COMM_CONFIG_REFERENCE_DEVICE_ENABLE
        LOG_INFO(LOG_REGION_THCI, "received MLR.rsp: {}", utils::Hex(aResponse->GetPayload()));
#endif

        SuccessOrExit(error = CheckCoapResponseCode(*aResponse));

        statusTlv = GetTlv(tlv::Type::kThreadStatus, *aResponse, tlv::Scope::kThread);
        VerifyOrExit(statusTlv != nullptr, error = ERROR_BAD_FORMAT("no valid State TLV found in response"));

        status = statusTlv->GetValueAsUint8();
        aHandler(&status, error);

    exit:
        if (error != ErrorCode::kNone)
        {
            aHandler(nullptr, error);
        }
    };

    VerifyOrExit(!aMulticastAddrList.empty(), error = ERROR_INVALID_ARGS("Multicast Address List cannot be empty"));

    for (const auto &addr : aMulticastAddrList)
    {
        SuccessOrExit(error = multicastAddr.Set(addr));
        VerifyOrExit(multicastAddr.IsIpv6() && multicastAddr.IsMulticast(),
                     error = ERROR_INVALID_ARGS("{} is not a valid IPv6 multicast address", multicastAddr.ToString()));
        rawAddresses.insert(rawAddresses.end(), multicastAddr.GetRaw().begin(), multicastAddr.GetRaw().end());
    }

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));
    SuccessOrExit(error = request.SetUriPath(uri::kMlr));
    SuccessOrExit(
        error = AppendTlv(request, {tlv::Type::kThreadCommissionerSessionId, GetSessionId(), tlv::Scope::kThread}));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kThreadTimeout, aTimeout, tlv::Scope::kThread}));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kThreadIpv6Addresses, rawAddresses, tlv::Scope::kThread}));

#if OT_COMM_CONFIG_CCM_ENABLE
    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }
#endif

    mProxyClient.SendRequest(request, onResponse, kPrimaryBbrAloc16, kDefaultMmPort);

    LOG_DEBUG(LOG_REGION_MGMT, "sent MLR.req");

exit:
    if (error != ErrorCode::kNone)
    {
        aHandler(nullptr, error);
    }
}

void CommissionerImpl::AnnounceBegin(ErrorHandler       aHandler,
                                     uint32_t           aChannelMask,
                                     uint8_t            aCount,
                                     uint16_t           aPeriod,
                                     const std::string &aDstAddr)
{
    Error         error;
    Address       dstAddr;
    ByteArray     channelMask;
    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};

    auto onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        Error error;

        SuccessOrExit(error = aError);
        SuccessOrExit(error = CheckCoapResponseCode(*aResponse));

    exit:
        aHandler(error);
    };

    SuccessOrExit(error = dstAddr.Set(aDstAddr));

    if (dstAddr.IsMulticast())
    {
        request.SetType(coap::Type::kNonConfirmable);
    }

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));
    SuccessOrExit(error = request.SetUriPath(uri::kMgmtAnnounceBegin));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kCommissionerSessionId, GetSessionId()}));
    SuccessOrExit(error = MakeChannelMask(channelMask, aChannelMask));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kChannelMask, channelMask}));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kCount, aCount}));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kPeriod, aPeriod}));

#if OT_COMM_CONFIG_CCM_ENABLE
    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }
#endif

    mProxyClient.SendRequest(request, onResponse, dstAddr, kDefaultMmPort);

exit:
    if (error != ErrorCode::kNone || request.IsNonConfirmable())
    {
        aHandler(error);
    }
}

void CommissionerImpl::PanIdQuery(ErrorHandler       aHandler,
                                  uint32_t           aChannelMask,
                                  uint16_t           aPanId,
                                  const std::string &aDstAddr)
{
    Error         error;
    Address       dstAddr;
    ByteArray     channelMask;
    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};

    auto onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        Error error;

        SuccessOrExit(error = aError);
        SuccessOrExit(error = CheckCoapResponseCode(*aResponse));

    exit:
        aHandler(error);
    };

    SuccessOrExit(error = dstAddr.Set(aDstAddr));

    if (dstAddr.IsMulticast())
    {
        request.SetType(coap::Type::kNonConfirmable);
    }

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));
    SuccessOrExit(error = request.SetUriPath(uri::kMgmtPanidQuery));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kCommissionerSessionId, GetSessionId()}));
    SuccessOrExit(error = MakeChannelMask(channelMask, aChannelMask));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kChannelMask, channelMask}));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kPanId, aPanId}));

#if OT_COMM_CONFIG_CCM_ENABLE
    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }
#endif

    mProxyClient.SendRequest(request, onResponse, dstAddr, kDefaultMmPort);

exit:
    if (error != ErrorCode::kNone || request.IsNonConfirmable())
    {
        aHandler(error);
    }
}

void CommissionerImpl::EnergyScan(ErrorHandler       aHandler,
                                  uint32_t           aChannelMask,
                                  uint8_t            aCount,
                                  uint16_t           aPeriod,
                                  uint16_t           aScanDuration,
                                  const std::string &aDstAddr)
{
    Error         error;
    Address       dstAddr;
    ByteArray     channelMask;
    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};

    auto onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        Error error;

        SuccessOrExit(error = aError);
        SuccessOrExit(error = CheckCoapResponseCode(*aResponse));

    exit:
        aHandler(error);
    };

    SuccessOrExit(error = dstAddr.Set(aDstAddr));

    if (dstAddr.IsMulticast())
    {
        request.SetType(coap::Type::kNonConfirmable);
    }

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));
    SuccessOrExit(error = request.SetUriPath(uri::kMgmtEdScan));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kCommissionerSessionId, GetSessionId()}));
    SuccessOrExit(error = MakeChannelMask(channelMask, aChannelMask));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kChannelMask, channelMask}));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kCount, aCount}));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kPeriod, aPeriod}));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kScanDuration, aScanDuration}));

#if OT_COMM_CONFIG_CCM_ENABLE
    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }
#endif

    mProxyClient.SendRequest(request, onResponse, dstAddr, kDefaultMmPort);

exit:
    if (error != ErrorCode::kNone || request.IsNonConfirmable())
    {
        aHandler(error);
    }
}

void CommissionerImpl::SendPetition(PetitionHandler aHandler)
{
    Error         error;
    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};

    auto onResponse = [this, aHandler](const coap::Response *aResponse, Error aError) {
        Error       error;
        tlv::TlvSet tlvSet;
        tlv::TlvPtr stateTlv          = nullptr;
        tlv::TlvPtr sessionIdTlv      = nullptr;
        tlv::TlvPtr commissionerIdTlv = nullptr;
        std::string existingCommissionerId;

        SuccessOrExit(error = aError);
        SuccessOrExit(error = CheckCoapResponseCode(*aResponse));

        SuccessOrExit(error = GetTlvSet(tlvSet, *aResponse));

        stateTlv = tlvSet[tlv::Type::kState];
        VerifyOrExit(stateTlv != nullptr, error = ERROR_BAD_FORMAT("no valid State TLV found in response"));
        if (stateTlv->GetValueAsInt8() != tlv::kStateAccept)
        {
            commissionerIdTlv = tlvSet[tlv::Type::kCommissionerId];
            if (commissionerIdTlv != nullptr && commissionerIdTlv->IsValid())
            {
                existingCommissionerId = commissionerIdTlv->GetValueAsString();
            }
            ExitNow(error = ERROR_REJECTED("petition was rejected"));
        }

        sessionIdTlv = tlvSet[tlv::Type::kCommissionerSessionId];
        VerifyOrExit(sessionIdTlv != nullptr,
                     error = ERROR_BAD_FORMAT("no valid Commissioner Session TLV found in response"));

        mSessionId = sessionIdTlv->GetValueAsUint16();
        mState     = State::kActive;
        mKeepAliveTimer.Start(GetKeepAliveInterval());

        LOG_INFO(LOG_REGION_MESHCOP, "petition succeed, start keep-alive timer with {} seconds",
                 GetKeepAliveInterval().count() / 1000);

    exit:
        if (error != ErrorCode::kNone)
        {
            mState = State::kDisabled;
        }
        aHandler(existingCommissionerId.empty() ? nullptr : &existingCommissionerId, error);
    };

    VerifyOrExit(mState == State::kConnected,
                 error = ERROR_INVALID_STATE("the commissioner is not started, petitioning, or already active"));
    SuccessOrExit(error = request.SetUriPath(uri::kPetitioning));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kCommissionerId, mConfig.mId}));

#if OT_COMM_CONFIG_CCM_ENABLE
    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }
#endif

    mState = State::kPetitioning;

    mBrClient.SendRequest(request, onResponse);

    LOG_DEBUG(LOG_REGION_MESHCOP, "sent petition request");

exit:
    if (error != ErrorCode::kNone)
    {
        aHandler(nullptr, error);
    }
}

void CommissionerImpl::SendKeepAlive(Timer &, bool aKeepAlive)
{
    Error         error;
    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};
    auto          state = (aKeepAlive ? tlv::kStateAccept : tlv::kStateReject);

    auto onResponse = [this, aKeepAlive](const coap::Response *aResponse, Error aError) {
        Error error            = HandleStateResponse(aResponse, aError);
        bool  shouldDisconnect = true;

        if (aKeepAlive && error == ErrorCode::kNone)
        {
            mKeepAliveTimer.Start(GetKeepAliveInterval());
            LOG_INFO(LOG_REGION_MESHCOP, "keep alive message accepted, keep-alive timer restarted");
            shouldDisconnect = false;
        }
        else if (!aKeepAlive)
        {
            if (aError == ErrorCode::kNone && error.GetCode() == ErrorCode::kRejected)
            {
                error = ERROR_NONE;
            }
            LOG_INFO(LOG_REGION_MESHCOP, "keep alive reject message sent, disconnecting commissioner");
        }
        else
        {
            LOG_WARN(LOG_REGION_MESHCOP, "keep alive message rejected: {}", error.ToString());
            mKeepAliveTimer.Stop();
        }

        mCommissionerHandler.OnKeepAliveResponse(error);

        if (shouldDisconnect)
        {
            Disconnect();
        }
    };

    VerifyOrExit(IsActive(),
                 error = ERROR_INVALID_STATE("cannot send keep-alive message the commissioner is not active"));

    SuccessOrExit(error = request.SetUriPath(uri::kKeepAlive));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kState, state}));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kCommissionerSessionId, GetSessionId()}));

#if OT_COMM_CONFIG_CCM_ENABLE
    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request, tlv::Scope::kMeshCoP, /* aAppendToken */ false));
    }
#endif

    mBrClient.SendRequest(request, onResponse);

    LOG_DEBUG(LOG_REGION_MESHCOP, "sent keep alive message: keepAlive={}", aKeepAlive);

exit:
    if (error != ErrorCode::kNone)
    {
        LOG_WARN(LOG_REGION_MESHCOP, "sending keep alive message failed: {}", error.ToString());
        Disconnect();
    }
}

#if OT_COMM_CONFIG_CCM_ENABLE
Error CommissionerImpl::SignRequest(coap::Request &aRequest, tlv::Scope aScope, bool aAppendToken)
{
    Error     error;
    ByteArray signature;

    ASSERT(IsCcmMode());

    SuccessOrExit(error = mTokenManager.SignMessage(signature, aRequest));

    if (aAppendToken)
    {
        SuccessOrExit(error = AppendTlv(aRequest, {tlv::Type::kCommissionerToken, mTokenManager.GetToken(), aScope}));
    }
    SuccessOrExit(error = AppendTlv(aRequest, {tlv::Type::kCommissionerSignature, signature, aScope}));

exit:
    return error;
}
#endif // OT_COMM_CONFIG_CCM_ENABLE

Error AppendTlv(coap::Message &aMessage, const tlv::Tlv &aTlv)
{
    Error     error;
    ByteArray buf;

    VerifyOrExit(aTlv.IsValid(),
                 error = ERROR_INVALID_ARGS("the tlv(type={}) is in bad format", utils::to_underlying(aTlv.GetType())));

    aTlv.Serialize(buf);
    aMessage.Append(buf);

exit:
    return error;
}

Error GetTlvSet(tlv::TlvSet &aTlvSet, const coap::Message &aMessage, tlv::Scope aScope)
{
    return tlv::GetTlvSet(aTlvSet, aMessage.GetPayload(), aScope);
}

tlv::TlvPtr GetTlv(tlv::Type aTlvType, const coap::Message &aMessage, tlv::Scope aScope)
{
    return tlv::GetTlv(aTlvType, aMessage.GetPayload(), aScope);
}

Error CommissionerImpl::CheckCoapResponseCode(const coap::Response &aResponse)
{
    Error error = ERROR_NONE;

    VerifyOrExit(aResponse.GetCode() == coap::Code::kChanged,
                 error = ERROR_COAP_ERROR("request for {} failed: {}", aResponse.GetRequestUri(),
                                          coap::CodeToString(aResponse.GetCode())));

exit:
    return error;
}

Error CommissionerImpl::HandleStateResponse(const coap::Response *aResponse, Error aError, bool aStateTlvIsMandatory)
{
    Error       error;
    tlv::TlvPtr stateTlv = nullptr;

    SuccessOrExit(error = aError);
    SuccessOrExit(error = CheckCoapResponseCode(*aResponse));

    stateTlv = GetTlv(tlv::Type::kState, *aResponse);
    VerifyOrExit((stateTlv != nullptr || !aStateTlvIsMandatory),
                 error = ERROR_BAD_FORMAT("no valid State TLV found in response"));
    if (stateTlv != nullptr)
    {
        VerifyOrExit(stateTlv->GetValueAsInt8() == tlv::kStateAccept,
                     error = ERROR_REJECTED("the request was rejected by peer"));
    }

exit:
    return error;
}

static void inline EncodeTlvType(ByteArray &aBuf, tlv::Type aTlvType)
{
    aBuf.emplace_back(utils::to_underlying(aTlvType));
}

ByteArray CommissionerImpl::GetActiveOperationalDatasetTlvs(uint16_t aDatasetFlags)
{
    ByteArray tlvTypes;

    if (aDatasetFlags & ActiveOperationalDataset::kActiveTimestampBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kActiveTimestamp);
    }
    if (aDatasetFlags & ActiveOperationalDataset::kChannelBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kChannel);
    }
    if (aDatasetFlags & ActiveOperationalDataset::kChannelMaskBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kChannelMask);
    }
    if (aDatasetFlags & ActiveOperationalDataset::kExtendedPanIdBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kExtendedPanId);
    }
    if (aDatasetFlags & ActiveOperationalDataset::kMeshLocalPrefixBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkMeshLocalPrefix);
    }
    if (aDatasetFlags & ActiveOperationalDataset::kNetworkMasterKeyBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkMasterKey);
    }
    if (aDatasetFlags & ActiveOperationalDataset::kNetworkNameBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkName);
    }
    if (aDatasetFlags & ActiveOperationalDataset::kPanIdBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kPanId);
    }
    if (aDatasetFlags & ActiveOperationalDataset::kPSKcBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kPSKc);
    }
    if (aDatasetFlags & ActiveOperationalDataset::kSecurityPolicyBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kSecurityPolicy);
    }

    return tlvTypes;
}

ByteArray CommissionerImpl::GetPendingOperationalDatasetTlvs(uint16_t aDatasetFlags)
{
    auto tlvTypes = GetActiveOperationalDatasetTlvs(aDatasetFlags);

    if (aDatasetFlags & PendingOperationalDataset::kDelayTimerBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kDelayTimer);
    }
    if (aDatasetFlags & PendingOperationalDataset::kPendingTimestampBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kPendingTimestamp);
    }

    return tlvTypes;
}

Error CommissionerImpl::DecodeActiveOperationalDataset(ActiveOperationalDataset &aDataset, const ByteArray &aPayload)
{
    Error                    error;
    tlv::TlvSet              tlvSet;
    ActiveOperationalDataset dataset;

    // Clear all data fields
    dataset.mPresentFlags = 0;

    SuccessOrExit(error = tlv::GetTlvSet(tlvSet, aPayload));

    if (auto activeTimeStamp = tlvSet[tlv::Type::kActiveTimestamp])
    {
        uint64_t value;
        value                    = utils::Decode<uint64_t>(activeTimeStamp->GetValue());
        dataset.mActiveTimestamp = Timestamp::Decode(value);
        dataset.mPresentFlags |= ActiveOperationalDataset::kActiveTimestampBit;
    }

    if (auto channel = tlvSet[tlv::Type::kChannel])
    {
        const ByteArray &value   = channel->GetValue();
        dataset.mChannel.mPage   = value[0];
        dataset.mChannel.mNumber = utils::Decode<uint16_t>(value.data() + 1, value.size() - 1);
        dataset.mPresentFlags |= ActiveOperationalDataset::kChannelBit;
    }

    if (auto channelMask = tlvSet[tlv::Type::kChannelMask])
    {
        SuccessOrExit(DecodeChannelMask(dataset.mChannelMask, channelMask->GetValue()));
        dataset.mPresentFlags |= ActiveOperationalDataset::kChannelMaskBit;
    }

    if (auto extendedPanId = tlvSet[tlv::Type::kExtendedPanId])
    {
        dataset.mExtendedPanId = extendedPanId->GetValue();
        dataset.mPresentFlags |= ActiveOperationalDataset::kExtendedPanIdBit;
    }

    if (auto meshLocalPrefix = tlvSet[tlv::Type::kNetworkMeshLocalPrefix])
    {
        dataset.mMeshLocalPrefix = meshLocalPrefix->GetValue();
        dataset.mPresentFlags |= ActiveOperationalDataset::kMeshLocalPrefixBit;
    }

    if (auto networkMasterKey = tlvSet[tlv::Type::kNetworkMasterKey])
    {
        dataset.mNetworkMasterKey = networkMasterKey->GetValue();
        dataset.mPresentFlags |= ActiveOperationalDataset::kNetworkMasterKeyBit;
    }

    if (auto networkName = tlvSet[tlv::Type::kNetworkName])
    {
        dataset.mNetworkName = networkName->GetValueAsString();
        dataset.mPresentFlags |= ActiveOperationalDataset::kNetworkNameBit;
    }

    if (auto panId = tlvSet[tlv::Type::kPanId])
    {
        dataset.mPanId = utils::Decode<uint16_t>(panId->GetValue());
        dataset.mPresentFlags |= ActiveOperationalDataset::kPanIdBit;
    }

    if (auto pskc = tlvSet[tlv::Type::kPSKc])
    {
        dataset.mPSKc = pskc->GetValue();
        dataset.mPresentFlags |= ActiveOperationalDataset::kPSKcBit;
    }

    if (auto securityPolicy = tlvSet[tlv::Type::kSecurityPolicy])
    {
        auto &value                           = securityPolicy->GetValue();
        dataset.mSecurityPolicy.mRotationTime = utils::Decode<uint16_t>(value);
        dataset.mSecurityPolicy.mFlags        = {value.begin() + sizeof(uint16_t), value.end()};
        dataset.mPresentFlags |= ActiveOperationalDataset::kSecurityPolicyBit;
    }

    aDataset = dataset;

exit:
    return error;
}

Error CommissionerImpl::DecodePendingOperationalDataset(PendingOperationalDataset &aDataset,
                                                        const coap::Response      &aResponse)
{
    Error                     error;
    tlv::TlvSet               tlvSet;
    PendingOperationalDataset dataset;

    // Clear all data fields
    dataset.mPresentFlags = 0;

    SuccessOrExit(error = DecodeActiveOperationalDataset(dataset, aResponse.GetPayload()));
    SuccessOrExit(error = GetTlvSet(tlvSet, aResponse));

    if (auto delayTimer = tlvSet[tlv::Type::kDelayTimer])
    {
        dataset.mDelayTimer = utils::Decode<uint32_t>(delayTimer->GetValue());
        dataset.mPresentFlags |= PendingOperationalDataset::kDelayTimerBit;
    }

    if (auto pendingTimestamp = tlvSet[tlv::Type::kPendingTimestamp])
    {
        uint64_t value;
        value                     = utils::Decode<uint64_t>(pendingTimestamp->GetValue());
        dataset.mPendingTimestamp = Timestamp::Decode(value);
        dataset.mPresentFlags |= PendingOperationalDataset::kPendingTimestampBit;
    }

    aDataset = dataset;

exit:
    return error;
}

Error CommissionerImpl::DecodeChannelMask(ChannelMask &aChannelMask, const ByteArray &aBuf)
{
    Error       error;
    ChannelMask channelMask;
    size_t      offset = 0;
    size_t      length = aBuf.size();

    while (offset < length)
    {
        ChannelMaskEntry entry;
        uint8_t          entryLength;
        VerifyOrExit(offset + 2 <= length, error = ERROR_BAD_FORMAT("premature end of Channel Mask Entry"));

        entry.mPage = aBuf[offset++];
        entryLength = aBuf[offset++];

        VerifyOrExit(offset + entryLength <= length, error = ERROR_BAD_FORMAT("premature end of Channel Mask Entry"));
        entry.mMasks = {aBuf.begin() + offset, aBuf.begin() + offset + entryLength};
        channelMask.emplace_back(entry);

        offset += entryLength;
    }

    ASSERT(offset == length);

    aChannelMask = channelMask;

exit:
    return error;
}

Error CommissionerImpl::EncodeActiveOperationalDataset(coap::Request                  &aRequest,
                                                       const ActiveOperationalDataset &aDataset)
{
    Error error;

    if (aDataset.mPresentFlags & ActiveOperationalDataset::kActiveTimestampBit)
    {
        SuccessOrExit(error = AppendTlv(aRequest, {tlv::Type::kActiveTimestamp, aDataset.mActiveTimestamp.Encode()}));
    }

    if (aDataset.mPresentFlags & ActiveOperationalDataset::kChannelBit)
    {
        ByteArray value;
        utils::Encode(value, aDataset.mChannel.mPage);
        utils::Encode(value, aDataset.mChannel.mNumber);
        SuccessOrExit(error = AppendTlv(aRequest, {tlv::Type::kChannel, value}));
    }

    if (aDataset.mPresentFlags & ActiveOperationalDataset::kChannelMaskBit)
    {
        ByteArray value;
        SuccessOrExit(error = EncodeChannelMask(value, aDataset.mChannelMask));
        SuccessOrExit(error = AppendTlv(aRequest, {tlv::Type::kChannelMask, value}));
    }

    if (aDataset.mPresentFlags & ActiveOperationalDataset::kExtendedPanIdBit)
    {
        SuccessOrExit(error = AppendTlv(aRequest, {tlv::Type::kExtendedPanId, aDataset.mExtendedPanId}));
    }

    if (aDataset.mPresentFlags & ActiveOperationalDataset::kMeshLocalPrefixBit)
    {
        SuccessOrExit(error = AppendTlv(aRequest, {tlv::Type::kNetworkMeshLocalPrefix, aDataset.mMeshLocalPrefix}));
    }

    if (aDataset.mPresentFlags & ActiveOperationalDataset::kNetworkMasterKeyBit)
    {
        SuccessOrExit(error = AppendTlv(aRequest, {tlv::Type::kNetworkMasterKey, aDataset.mNetworkMasterKey}));
    }

    if (aDataset.mPresentFlags & ActiveOperationalDataset::kNetworkNameBit)
    {
        SuccessOrExit(error = AppendTlv(aRequest, {tlv::Type::kNetworkName, aDataset.mNetworkName}));
    }

    if (aDataset.mPresentFlags & ActiveOperationalDataset::kPanIdBit)
    {
        SuccessOrExit(error = AppendTlv(aRequest, {tlv::Type::kPanId, aDataset.mPanId}));
    }

    if (aDataset.mPresentFlags & ActiveOperationalDataset::kPSKcBit)
    {
        SuccessOrExit(error = AppendTlv(aRequest, {tlv::Type::kPSKc, aDataset.mPSKc}));
    }

    if (aDataset.mPresentFlags & ActiveOperationalDataset::kSecurityPolicyBit)
    {
        ByteArray value;
        utils::Encode(value, aDataset.mSecurityPolicy.mRotationTime);
        value.insert(value.end(), aDataset.mSecurityPolicy.mFlags.begin(), aDataset.mSecurityPolicy.mFlags.end());
        SuccessOrExit(error = AppendTlv(aRequest, {tlv::Type::kSecurityPolicy, value}));
    }

exit:
    return error;
}

Error CommissionerImpl::EncodePendingOperationalDataset(coap::Request                   &aRequest,
                                                        const PendingOperationalDataset &aDataset)
{
    Error error;

    SuccessOrExit(error = EncodeActiveOperationalDataset(aRequest, aDataset));

    if (aDataset.mPresentFlags & PendingOperationalDataset::kDelayTimerBit)
    {
        SuccessOrExit(error = AppendTlv(aRequest, {tlv::Type::kDelayTimer, aDataset.mDelayTimer}));
    }

    if (aDataset.mPresentFlags & PendingOperationalDataset::kPendingTimestampBit)
    {
        SuccessOrExit(error = AppendTlv(aRequest, {tlv::Type::kPendingTimestamp, aDataset.mPendingTimestamp.Encode()}));
    }

exit:
    return error;
}

Error CommissionerImpl::EncodeChannelMask(ByteArray &aBuf, const ChannelMask &aChannelMask)
{
    Error error;

    for (const auto &entry : aChannelMask)
    {
        VerifyOrExit(entry.mMasks.size() < tlv::kEscapeLength,
                     error = ERROR_INVALID_ARGS("Channel Mask list is tool long (>={})", tlv::kEscapeLength));

        utils::Encode(aBuf, entry.mPage);
        utils::Encode(aBuf, static_cast<uint8_t>(entry.mMasks.size()));
        aBuf.insert(aBuf.end(), entry.mMasks.begin(), entry.mMasks.end());
    }

exit:
    return error;
}

#if OT_COMM_CONFIG_CCM_ENABLE
Error CommissionerImpl::DecodeBbrDataset(BbrDataset &aDataset, const coap::Response &aResponse)
{
    Error       error;
    tlv::TlvSet tlvSet;
    BbrDataset  dataset;

    SuccessOrExit(error = GetTlvSet(tlvSet, aResponse));

    if (auto triHostname = tlvSet[tlv::Type::kTriHostname])
    {
        dataset.mTriHostname = triHostname->GetValueAsString();
        dataset.mPresentFlags |= BbrDataset::kTriHostnameBit;
    }

    if (auto registrarHostname = tlvSet[tlv::Type::kRegistrarHostname])
    {
        dataset.mRegistrarHostname = registrarHostname->GetValueAsString();
        dataset.mPresentFlags |= BbrDataset::kRegistrarHostnameBit;
    }

    if (auto registrarIpv6Addr = tlvSet[tlv::Type::kRegistrarIpv6Address])
    {
        Address addr;

        SuccessOrExit(error = addr.Set(registrarIpv6Addr->GetValue()));

        dataset.mRegistrarIpv6Addr = addr.ToString();
        dataset.mPresentFlags |= BbrDataset::kRegistrarIpv6AddrBit;
    }

    aDataset = dataset;

exit:
    return error;
}

Error CommissionerImpl::EncodeBbrDataset(coap::Request &aRequest, const BbrDataset &aDataset)
{
    Error error;

    if (aDataset.mPresentFlags & BbrDataset::kTriHostnameBit)
    {
        SuccessOrExit(error = AppendTlv(aRequest, {tlv::Type::kTriHostname, aDataset.mTriHostname}));
    }

    if (aDataset.mPresentFlags & BbrDataset::kRegistrarHostnameBit)
    {
        SuccessOrExit(error = AppendTlv(aRequest, {tlv::Type::kRegistrarHostname, aDataset.mRegistrarHostname}));
    }

    if (aDataset.mPresentFlags & BbrDataset::kRegistrarIpv6AddrBit)
    {
        Address addr;

        SuccessOrExit(error = addr.Set(aDataset.mRegistrarIpv6Addr));
        SuccessOrExit(error = AppendTlv(aRequest, {tlv::Type::kRegistrarIpv6Address, addr.GetRaw()}));
    }

exit:
    return error;
}

ByteArray CommissionerImpl::GetBbrDatasetTlvs(uint16_t aDatasetFlags)
{
    ByteArray tlvTypes;

    if (aDatasetFlags & BbrDataset::kTriHostnameBit)
    {
        tlvTypes.emplace_back(utils::to_underlying(tlv::Type::kTriHostname));
    }

    if (aDatasetFlags & BbrDataset::kRegistrarHostnameBit)
    {
        tlvTypes.emplace_back(utils::to_underlying(tlv::Type::kRegistrarHostname));
    }

    if (aDatasetFlags & BbrDataset::kRegistrarIpv6AddrBit)
    {
        tlvTypes.emplace_back(utils::to_underlying(tlv::Type::kRegistrarIpv6Address));
    }
    return tlvTypes;
}
#endif // OT_COMM_CONFIG_CCM_ENABLE

Error CommissionerImpl::DecodeCommissionerDataset(CommissionerDataset &aDataset, const coap::Response &aResponse)
{
    Error               error;
    tlv::TlvSet         tlvSet;
    CommissionerDataset dataset;

    SuccessOrExit(error = GetTlvSet(tlvSet, aResponse));

    if (auto sessionId = tlvSet[tlv::Type::kCommissionerSessionId])
    {
        dataset.mSessionId = sessionId->GetValueAsUint16();
        dataset.mPresentFlags |= CommissionerDataset::kSessionIdBit;
    }

    if (auto borderAgentLocator = tlvSet[tlv::Type::kBorderAgentLocator])
    {
        dataset.mBorderAgentLocator = borderAgentLocator->GetValueAsUint16();
        dataset.mPresentFlags |= CommissionerDataset::kBorderAgentLocatorBit;
    }

    if (auto steeringData = tlvSet[tlv::Type::kSteeringData])
    {
        dataset.mSteeringData = steeringData->GetValue();
        dataset.mPresentFlags |= CommissionerDataset::kSteeringDataBit;
    }

    if (auto aeSteeringData = tlvSet[tlv::Type::kAeSteeringData])
    {
        dataset.mAeSteeringData = aeSteeringData->GetValue();
        dataset.mPresentFlags |= CommissionerDataset::kAeSteeringDataBit;
    }

    if (auto nmkpSteeringData = tlvSet[tlv::Type::kNmkpSteeringData])
    {
        dataset.mNmkpSteeringData = nmkpSteeringData->GetValue();
        dataset.mPresentFlags |= CommissionerDataset::kNmkpSteeringDataBit;
    }

    if (auto joinerUdpPort = tlvSet[tlv::Type::kJoinerUdpPort])
    {
        dataset.mJoinerUdpPort = joinerUdpPort->GetValueAsUint16();
        dataset.mPresentFlags |= CommissionerDataset::kJoinerUdpPortBit;
    }

    if (auto aeUdpPort = tlvSet[tlv::Type::kAeUdpPort])
    {
        dataset.mAeUdpPort = aeUdpPort->GetValueAsUint16();
        dataset.mPresentFlags |= CommissionerDataset::kAeUdpPortBit;
    }

    if (auto nmkpUdpPort = tlvSet[tlv::Type::kNmkpUdpPort])
    {
        dataset.mNmkpUdpPort = nmkpUdpPort->GetValueAsUint16();
        dataset.mPresentFlags |= CommissionerDataset::kNmkpUdpPortBit;
    }

    aDataset = dataset;

exit:
    return error;
}

Error CommissionerImpl::EncodeCommissionerDataset(coap::Request &aRequest, const CommissionerDataset &aDataset)
{
    Error error;

    if (aDataset.mPresentFlags & CommissionerDataset::kSessionIdBit)
    {
        SuccessOrExit(error = AppendTlv(aRequest, {tlv::Type::kCommissionerSessionId, aDataset.mSessionId}));
    }

    if (aDataset.mPresentFlags & CommissionerDataset::kBorderAgentLocatorBit)
    {
        SuccessOrExit(error = AppendTlv(aRequest, {tlv::Type::kBorderAgentLocator, aDataset.mBorderAgentLocator}));
    }

    if (aDataset.mPresentFlags & CommissionerDataset::kSteeringDataBit)
    {
        SuccessOrExit(error = AppendTlv(aRequest, {tlv::Type::kSteeringData, aDataset.mSteeringData}));
    }

    if (aDataset.mPresentFlags & CommissionerDataset::kAeSteeringDataBit)
    {
        SuccessOrExit(error = AppendTlv(aRequest, {tlv::Type::kAeSteeringData, aDataset.mAeSteeringData}));
    }

    if (aDataset.mPresentFlags & CommissionerDataset::kNmkpSteeringDataBit)
    {
        SuccessOrExit(error = AppendTlv(aRequest, {tlv::Type::kNmkpSteeringData, aDataset.mNmkpSteeringData}));
    }

    if (aDataset.mPresentFlags & CommissionerDataset::kJoinerUdpPortBit)
    {
        SuccessOrExit(error = AppendTlv(aRequest, {tlv::Type::kJoinerUdpPort, aDataset.mJoinerUdpPort}));
    }

    if (aDataset.mPresentFlags & CommissionerDataset::kAeUdpPortBit)
    {
        SuccessOrExit(error = AppendTlv(aRequest, {tlv::Type::kAeUdpPort, aDataset.mAeUdpPort}));
    }

    if (aDataset.mPresentFlags & CommissionerDataset::kNmkpUdpPortBit)
    {
        SuccessOrExit(error = AppendTlv(aRequest, {tlv::Type::kNmkpUdpPort, aDataset.mNmkpUdpPort}));
    }

exit:
    return error;
}

ByteArray CommissionerImpl::GetCommissionerDatasetTlvs(uint16_t aDatasetFlags)
{
    ByteArray tlvTypes;

    if (aDatasetFlags & CommissionerDataset::kSessionIdBit)
    {
        tlvTypes.emplace_back(utils::to_underlying(tlv::Type::kCommissionerSessionId));
    }

    if (aDatasetFlags & CommissionerDataset::kBorderAgentLocatorBit)
    {
        tlvTypes.emplace_back(utils::to_underlying(tlv::Type::kBorderAgentLocator));
    }

    if (aDatasetFlags & CommissionerDataset::kSteeringDataBit)
    {
        tlvTypes.emplace_back(utils::to_underlying(tlv::Type::kSteeringData));
    }

    if (aDatasetFlags & CommissionerDataset::kAeSteeringDataBit)
    {
        tlvTypes.emplace_back(utils::to_underlying(tlv::Type::kAeSteeringData));
    }

    if (aDatasetFlags & CommissionerDataset::kNmkpSteeringDataBit)
    {
        tlvTypes.emplace_back(utils::to_underlying(tlv::Type::kNmkpSteeringData));
    }

    if (aDatasetFlags & CommissionerDataset::kJoinerUdpPortBit)
    {
        tlvTypes.emplace_back(utils::to_underlying(tlv::Type::kJoinerUdpPort));
    }

    if (aDatasetFlags & CommissionerDataset::kAeUdpPortBit)
    {
        tlvTypes.emplace_back(utils::to_underlying(tlv::Type::kAeUdpPort));
    }

    if (aDatasetFlags & CommissionerDataset::kNmkpUdpPortBit)
    {
        tlvTypes.emplace_back(utils::to_underlying(tlv::Type::kNmkpUdpPort));
    }
    return tlvTypes;
}

ByteArray CommissionerImpl::GetNetDiagTlvTypes(uint64_t aDiagDataFlags)
{
    ByteArray tlvTypes;

    if (aDiagDataFlags & NetDiagData::kExtMacAddrBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkDiagExtMacAddress);
    }

    if (aDiagDataFlags & NetDiagData::kMacAddrBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkDiagMacAddress);
    }

    if (aDiagDataFlags & NetDiagData::kModeBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkDiagMode);
    }

    if (aDiagDataFlags & NetDiagData::kRoute64Bit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkDiagRoute64);
    }

    if (aDiagDataFlags & NetDiagData::kLeaderDataBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkDiagLeaderData);
    }

    if (aDiagDataFlags & NetDiagData::kAddrsBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkDiagIpv6Address);
    }

    if (aDiagDataFlags & NetDiagData::kMacCountersBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkDiagMacCounters);
    }

    if (aDiagDataFlags & NetDiagData::kChildTableBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkDiagChildTable);
    }

    if (aDiagDataFlags & NetDiagData::kEui64Bit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkDiagEui64);
    }

    if (aDiagDataFlags & NetDiagData::kChildIpv6AddrsInfoListBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkDiagChildIpv6Address);
    }

    if (aDiagDataFlags & NetDiagData::kNetworkDataBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkDiagNetworkData);
    }

    if (aDiagDataFlags & NetDiagData::kTimeoutBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkDiagTimeout);
    }

    if (aDiagDataFlags & NetDiagData::kConnectivityBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkDiagConnectivity);
    }

    if (aDiagDataFlags & NetDiagData::kBatteryLevelBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkDiagBatteryLevel);
    }

    if (aDiagDataFlags & NetDiagData::kSupplyVoltageBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkDiagSupplyVoltage);
    }

    if (aDiagDataFlags & NetDiagData::kChannelPagesBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkDiagChannelPages);
    }

    if (aDiagDataFlags & NetDiagData::kTypeListBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkDiagTypeList);
    }

    if (aDiagDataFlags & NetDiagData::kMaxChildTimeoutBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkDiagMaxChildTimeout);
    }

    if (aDiagDataFlags & NetDiagData::kVersionBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkDiagVersion);
    }

    if (aDiagDataFlags & NetDiagData::kVendorNameBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkDiagVendorName);
    }

    if (aDiagDataFlags & NetDiagData::kVendorModelBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkDiagVendorModel);
    }

    if (aDiagDataFlags & NetDiagData::kVendorSWVersionBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkDiagVendorSWVersion);
    }

    if (aDiagDataFlags & NetDiagData::kThreadStackVersionBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkDiagThreadStackVersion);
    }

    if (aDiagDataFlags & NetDiagData::kChildBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkDiagChild);
    }

    if (aDiagDataFlags & NetDiagData::kRouterNeighborBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkDiagRouterNeighbor);
    }

    if (aDiagDataFlags & NetDiagData::kMleCountersBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkDiagMleCounters);
    }

    if (aDiagDataFlags & NetDiagData::kVendorAppURLBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkDiagVendorAppURL);
    }

    if (aDiagDataFlags & NetDiagData::kNonPreferredChannelsMaskBit)
    {
        EncodeTlvType(tlvTypes, tlv::Type::kNetworkDiagNonPreferredChannelsMask);
    }

    return tlvTypes;
}

Error internal::DecodeNetDiagData(NetDiagData &aNetDiagData, const ByteArray &aPayload)
{
    Error        error;
    tlv::TlvSet  tlvSet;
    tlv::TlvList tlvList;
    NetDiagData  diagData;

    SuccessOrExit(error = tlv::GetTlvSet(tlvSet, aPayload, tlv::Scope::kNetworkDiag));

    if (auto extMacAddr = tlvSet[tlv::Type::kNetworkDiagExtMacAddress])
    {
        const ByteArray &value = extMacAddr->GetValue();
        diagData.mExtMacAddr   = value;
        diagData.mPresentFlags |= NetDiagData::kExtMacAddrBit;
    }

    if (auto macAddr = tlvSet[tlv::Type::kNetworkDiagMacAddress])
    {
        uint16_t value;
        value             = utils::Decode<uint16_t>(macAddr->GetValue());
        diagData.mMacAddr = value;
        diagData.mPresentFlags |= NetDiagData::kMacAddrBit;
    }

    if (auto mode = tlvSet[tlv::Type::kNetworkDiagMode])
    {
        SuccessOrExit(error = internal::DecodeModeData(diagData.mMode, mode->GetValue()));
        diagData.mPresentFlags |= NetDiagData::kModeBit;
    }

    if (auto route64 = tlvSet[tlv::Type::kNetworkDiagRoute64])
    {
        const ByteArray &value = route64->GetValue();
        SuccessOrExit(error = DecodeRoute64(diagData.mRoute64, value));
        diagData.mPresentFlags |= NetDiagData::kRoute64Bit;
    }

    if (auto leaderData = tlvSet[tlv::Type::kNetworkDiagLeaderData])
    {
        const ByteArray &value = leaderData->GetValue();
        SuccessOrExit(error = DecodeLeaderData(diagData.mLeaderData, value));
        diagData.mPresentFlags |= NetDiagData::kLeaderDataBit;
    }

    if (auto ipv6Addrs = tlvSet[tlv::Type::kNetworkDiagIpv6Address])
    {
        const ByteArray &value = ipv6Addrs->GetValue();
        SuccessOrExit(error = DecodeIpv6AddressList(diagData.mAddrs, value));
        diagData.mPresentFlags |= NetDiagData::kAddrsBit;
    }

    if (auto macCounters = tlvSet[tlv::Type::kNetworkDiagMacCounters])
    {
        const ByteArray &value = macCounters->GetValue();
        SuccessOrExit(error = DecodeMacCounters(diagData.mMacCounters, value));
        diagData.mPresentFlags |= NetDiagData::kMacCountersBit;
    }

    if (auto childTable = tlvSet[tlv::Type::kNetworkDiagChildTable])
    {
        const ByteArray &value = childTable->GetValue();
        SuccessOrExit(error = DecodeChildTable(diagData.mChildTable, value));
        diagData.mPresentFlags |= NetDiagData::kChildTableBit;
    }

    if (auto eui64 = tlvSet[tlv::Type::kNetworkDiagEui64])
    {
        const ByteArray &value = eui64->GetValue();
        diagData.mEui64        = value;
        diagData.mPresentFlags |= NetDiagData::kEui64Bit;
    }

    SuccessOrExit(error = tlv::GetTlvListByType(tlvList, aPayload, tlv::Type::kNetworkDiagChildIpv6Address,
                                                tlv::Scope::kNetworkDiag));

    if (tlvList.size() > 0)
    {
        for (const auto &tlv : tlvList)
        {
            SuccessOrExit(error = DecodeChildIpv6AddressList(diagData.mChildIpv6AddrsInfoList, tlv.GetValue()));
        }
        diagData.mPresentFlags |= NetDiagData::kChildIpv6AddrsInfoListBit;
    }

    if (auto networkData = tlvSet[tlv::Type::kNetworkDiagNetworkData])
    {
        const ByteArray &value = networkData->GetValue();
        SuccessOrExit(error = DecodeNetworkData(diagData.mNetworkData, value));
        diagData.mPresentFlags |= NetDiagData::kNetworkDataBit;
    }

    if (auto timeout = tlvSet[tlv::Type::kNetworkDiagTimeout])
    {
        uint32_t value;
        value             = utils::Decode<uint32_t>(timeout->GetValue());
        diagData.mTimeout = value;
        diagData.mPresentFlags |= NetDiagData::kTimeoutBit;
    }

    if (auto connectivity = tlvSet[tlv::Type::kNetworkDiagConnectivity])
    {
        SuccessOrExit(error = DecodeConnectivity(diagData.mConnectivity, connectivity->GetValue()));
        diagData.mPresentFlags |= NetDiagData::kConnectivityBit;
    }

    if (auto batteryLevel = tlvSet[tlv::Type::kNetworkDiagBatteryLevel])
    {
        uint8_t value;
        value                  = utils::Decode<uint8_t>(batteryLevel->GetValue());
        diagData.mBatteryLevel = value;
        diagData.mPresentFlags |= NetDiagData::kBatteryLevelBit;
    }

    if (auto supplyVoltage = tlvSet[tlv::Type::kNetworkDiagSupplyVoltage])
    {
        uint16_t value;
        value                   = utils::Decode<uint16_t>(supplyVoltage->GetValue());
        diagData.mSupplyVoltage = value;
        diagData.mPresentFlags |= NetDiagData::kSupplyVoltageBit;
    }

    if (auto channelPages = tlvSet[tlv::Type::kNetworkDiagChannelPages])
    {
        const ByteArray &value = channelPages->GetValue();
        diagData.mChannelPages = value;
        diagData.mPresentFlags |= NetDiagData::kChannelPagesBit;
    }

    if (auto typeList = tlvSet[tlv::Type::kNetworkDiagTypeList])
    {
        const ByteArray &value = typeList->GetValue();
        diagData.mTypeList     = value;
        diagData.mPresentFlags |= NetDiagData::kTypeListBit;
    }

    if (auto maxChildTimeout = tlvSet[tlv::Type::kNetworkDiagMaxChildTimeout])
    {
        uint32_t value;
        value                     = utils::Decode<uint32_t>(maxChildTimeout->GetValue());
        diagData.mMaxChildTimeout = value;
        diagData.mPresentFlags |= NetDiagData::kMaxChildTimeoutBit;
    }

    if (auto version = tlvSet[tlv::Type::kNetworkDiagVersion])
    {
        uint16_t value;
        value             = utils::Decode<uint16_t>(version->GetValue());
        diagData.mVersion = value;
        diagData.mPresentFlags |= NetDiagData::kVersionBit;
    }

    if (auto vendorName = tlvSet[tlv::Type::kNetworkDiagVendorName])
    {
        diagData.mVendorName = vendorName->GetValueAsString();
        diagData.mPresentFlags |= NetDiagData::kVendorNameBit;
    }

    if (auto vendorModel = tlvSet[tlv::Type::kNetworkDiagVendorModel])
    {
        diagData.mVendorModel = vendorModel->GetValueAsString();
        diagData.mPresentFlags |= NetDiagData::kVendorModelBit;
    }

    if (auto vendorSWVersion = tlvSet[tlv::Type::kNetworkDiagVendorSWVersion])
    {
        diagData.mVendorSWVersion = vendorSWVersion->GetValueAsString();
        diagData.mPresentFlags |= NetDiagData::kVendorSWVersionBit;
    }

    if (auto threadStackVersion = tlvSet[tlv::Type::kNetworkDiagThreadStackVersion])
    {
        diagData.mThreadStackVersion = threadStackVersion->GetValueAsString();
        diagData.mPresentFlags |= NetDiagData::kThreadStackVersionBit;
    }

    SuccessOrExit(error =
                      tlv::GetTlvListByType(tlvList, aPayload, tlv::Type::kNetworkDiagChild, tlv::Scope::kNetworkDiag));

    if (tlvList.size() > 0)
    {
        for (const auto &tlv : tlvList)
        {
            SuccessOrExit(error = DecodeChild(diagData.mChild, tlv.GetValue()));
        }
        diagData.mPresentFlags |= NetDiagData::kChildBit;
    }

    SuccessOrExit(error = tlv::GetTlvListByType(tlvList, aPayload, tlv::Type::kNetworkDiagRouterNeighbor,
                                                tlv::Scope::kNetworkDiag));

    if (tlvList.size() > 0)
    {
        for (const auto &tlv : tlvList)
        {
            SuccessOrExit(error = DecodeRouterNeighbor(diagData.mRouterNeighbor, tlv.GetValue()));
        }
        diagData.mPresentFlags |= NetDiagData::kRouterNeighborBit;
    }

    if (auto mleCounters = tlvSet[tlv::Type::kNetworkDiagMleCounters])
    {
        SuccessOrExit(error = internal::DecodeMleCounters(diagData.mMleCounters, mleCounters->GetValue()));
        diagData.mPresentFlags |= NetDiagData::kMleCountersBit;
    }

    if (auto vendorAppURL = tlvSet[tlv::Type::kNetworkDiagVendorAppURL])
    {
        diagData.mVendorAppURL = vendorAppURL->GetValueAsString();
        diagData.mPresentFlags |= NetDiagData::kVendorAppURLBit;
    }

    if (auto channelsMask = tlvSet[tlv::Type::kNetworkDiagNonPreferredChannelsMask])
    {
        SuccessOrExit(error = internal::DecodeNonPreferredChannelsMask(diagData.mNonPreferredChannelsMask,
                                                                       channelsMask->GetValue()));
        diagData.mPresentFlags |= NetDiagData::kNonPreferredChannelsMaskBit;
    }

    aNetDiagData = diagData;

exit:
    return error;
}

Error internal::DecodeNetworkData(NetworkData &aNetworkData, const ByteArray &aBuf)
{
    Error        error;
    tlv::TlvList tlvList;

    SuccessOrExit(error =
                      tlv::GetTlvListByType(tlvList, aBuf, tlv::Type::kNetworkDataPrefix, tlv::Scope::kNetworkData));
    if (tlvList.size() > 0)
    {
        for (const auto &tlv : tlvList)
        {
            PrefixEntry prefixEntry;
            SuccessOrExit(error = DecodePrefixEntry(prefixEntry, tlv.GetValue()));
            aNetworkData.mPrefixList.emplace_back(prefixEntry);
        }
    }

exit:
    return error;
}

Error internal::DecodePrefixEntry(PrefixEntry &aPrefixEntry, const ByteArray &aBuf)
{
    Error       error;
    size_t      length = aBuf.size();
    uint8_t     offset = 0;
    ByteArray   subTlv;
    tlv::TlvSet tlvSet;

    VerifyOrExit(length >= kPrefixBytes, error = ERROR_BAD_FORMAT("premature end of Prefix"));
    offset += kPrefixBytes;
    aPrefixEntry.mDomainId     = aBuf[0];
    aPrefixEntry.mPrefixLength = (aBuf[1] + 7) >> 3;
    offset += aPrefixEntry.mPrefixLength;
    VerifyOrExit(length >= offset, error = ERROR_BAD_FORMAT("premature end of Prefix"));
    aPrefixEntry.mPrefix = {aBuf.begin() + kPrefixBytes, aBuf.begin() + kPrefixBytes + aPrefixEntry.mPrefixLength};

    if (length > offset)
    {
        subTlv = {aBuf.begin() + offset, aBuf.end()};

        // Get the 6LowPan context
        SuccessOrExit(error = tlv::GetTlvSet(tlvSet, subTlv, tlv::Scope::kNetworkData));
        if (auto context = tlvSet[tlv::Type::kNetworkData6LowPanContext])
        {
            const ByteArray &value = context->GetValue();
            SuccessOrExit(error = DecodeContext(aPrefixEntry.mSixLowPanContext, value));
        }

        // Get the HasRoute
        SuccessOrExit(error = tlv::GetTlvSet(tlvSet, subTlv, tlv::Scope::kNetworkData));
        if (auto hasRoute = tlvSet[tlv::Type::kNetworkDataHasRoute])
        {
            const ByteArray &value = hasRoute->GetValue();
            SuccessOrExit(error = DecodeHasRoute(aPrefixEntry.mHasRouteList, value));
        }

        // Get the BorderRouter
        SuccessOrExit(error = tlv::GetTlvSet(tlvSet, subTlv, tlv::Scope::kNetworkData));
        if (auto borderRouter = tlvSet[tlv::Type::kNetworkDataBorderRouter])
        {
            const ByteArray &value = borderRouter->GetValue();
            SuccessOrExit(error = DecodeBorderRouter(aPrefixEntry.mBorderRouterList, value));
        }
    }

exit:
    return error;
}

Error internal::DecodeHasRoute(std::vector<HasRouteEntry> &aHasRouteList, const ByteArray &aBuf)
{
    Error         error;
    size_t        length = aBuf.size();
    uint8_t       offset = 0;
    HasRouteEntry hasRouteEntry;

    VerifyOrExit((length % kHasRouteBytes == 0), error = ERROR_BAD_FORMAT("incorrect size of HasRoute"));
    while (offset < length)
    {
        hasRouteEntry.mRloc16 = utils::Decode<uint16_t>(aBuf.data() + offset, kRloc16Bytes);
        offset += kRloc16Bytes;
        hasRouteEntry.mIsNat64          = (aBuf[offset] >> 5) & 0x01;
        hasRouteEntry.mRouterPreference = (aBuf[offset] >> 6) & 0x03;
        offset += 1;
        aHasRouteList.emplace_back(hasRouteEntry);
    }

exit:
    return error;
}

Error internal::DecodeBorderRouter(std::vector<BorderRouterEntry> &aBorderRouterList, const ByteArray &aBuf)
{
    Error             error;
    size_t            length = aBuf.size();
    uint8_t           offset = 0;
    BorderRouterEntry borderRouterEntry;

    VerifyOrExit((length % kBorderRouterBytes == 0), error = ERROR_BAD_FORMAT("incorrect size of BorderRouter"));

    while (offset < length)
    {
        borderRouterEntry.mRloc16 = utils::Decode<uint16_t>(aBuf.data() + offset, kRloc16Bytes);
        offset += kRloc16Bytes;
        borderRouterEntry.mPrefixPreference = (aBuf[offset] >> 6) & 0x03;
        borderRouterEntry.mIsPreferred      = (aBuf[offset] >> 5) & 0x01;
        borderRouterEntry.mIsSlaac          = (aBuf[offset] >> 4) & 0x01;
        borderRouterEntry.mIsDhcp           = (aBuf[offset] >> 3) & 0x01;
        borderRouterEntry.mIsConfigure      = (aBuf[offset] >> 2) & 0x01;
        borderRouterEntry.mIsDefaultRoute   = (aBuf[offset] >> 1) & 0x01;
        borderRouterEntry.mIsOnMesh         = (aBuf[offset] >> 0) & 0x01;
        offset += 1;
        borderRouterEntry.mIsNdDns = (aBuf[offset] >> 7) & 0x01;
        borderRouterEntry.mIsDp    = (aBuf[offset] >> 6) & 0x01;
        offset += 1;
        aBorderRouterList.emplace_back(borderRouterEntry);
    }

exit:
    return error;
}

Error internal::DecodeContext(SixLowPanContext &aSixLowPanContext, const ByteArray &aBuf)
{
    Error  error;
    size_t length = aBuf.size();

    VerifyOrExit(length == kSixLowPanContextBytes, error = ERROR_BAD_FORMAT("incorrect size of Context"));
    aSixLowPanContext.mIsCompress    = (aBuf[0] >> 4) & 0x01;
    aSixLowPanContext.mContextId     = aBuf[0] & 0x0F;
    aSixLowPanContext.mContextLength = aBuf[1];

exit:
    return error;
}

Error internal::DecodeIpv6AddressList(std::vector<std::string> &aAddrs, const ByteArray &aBuf)
{
    Error  error;
    size_t length = aBuf.size();
    size_t offset = 0;
    while (offset < length)
    {
        VerifyOrExit(offset + kIpv6AddressBytes <= length, error = ERROR_BAD_FORMAT("premature end of IPv6 Address"));
        Address          addr;
        const ByteArray &value = {aBuf.data() + offset, aBuf.data() + offset + kIpv6AddressBytes};
        SuccessOrExit(error = addr.Set(value));
        aAddrs.emplace_back(addr.ToString());
        offset += kIpv6AddressBytes;
    }

exit:
    return error;
}

Error internal::DecodeChildIpv6AddressList(std::vector<ChildIpv6AddrInfo> &aChildIpv6AddressInfoList,
                                           const ByteArray                &aBuf)
{
    Error             error;
    size_t            length = aBuf.size();
    ChildIpv6AddrInfo childIpv6AddrsInfo;

    VerifyOrExit((length % kIpv6AddressBytes) == kRloc16Bytes,
                 error = ERROR_BAD_FORMAT("premature end of Child IPv6 Address"));
    childIpv6AddrsInfo.mRloc16  = utils::Decode<uint16_t>(aBuf.data(), kRloc16Bytes);
    childIpv6AddrsInfo.mChildId = childIpv6AddrsInfo.mRloc16 & 0x1FF;

    SuccessOrExit(error = DecodeIpv6AddressList(childIpv6AddrsInfo.mAddrs, {aBuf.begin() + kRloc16Bytes, aBuf.end()}));
    aChildIpv6AddressInfoList.emplace_back(childIpv6AddrsInfo);

exit:
    return error;
}

Error internal::DecodeModeData(ModeData &aModeData, const ByteArray &aBuf)
{
    Error  error;
    size_t length = aBuf.size();
    VerifyOrExit(length == 1, error = ERROR_BAD_FORMAT("invalid Mode value length {}, expect 1", length));
    aModeData.mIsRxOnWhenIdleMode          = (aBuf[0] & 0x01) != 0;
    aModeData.mIsMtd                       = (aBuf[0] & 0x02) == 0;
    aModeData.mIsStableNetworkDataRequired = (aBuf[0] & 0x04) != 0;

exit:
    return error;
}

/**
 * Decode Child Table TLV
 *
 * @param aChildTable The Child Table to be decoded, which is a vector of ChildTableEntry.
 *                    The ChildTableEntry is a struct of timeout, incoming link quality, child id,
 *                    and mode data which is composed of 3 bytes
 *                    Timeout is 2^(Timeout-4) seconds.
 *                    ILQ is a value between 0 and 3.
 *                    Child ID is a 9-bit value.
 * +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
 * +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * |       Timeout     |  ILQ  ｜           Child ID                |            Mode              |
 * +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * @param aBuf The buffer to be decoded
 * @return Error code
 */
Error internal::DecodeChildTable(std::vector<ChildTableEntry> &aChildTable, const ByteArray &aBuf)
{
    Error  error;
    size_t length = aBuf.size();
    size_t offset = 0;

    while (offset < length)
    {
        ChildTableEntry entry;
        VerifyOrExit(offset + kChildTableEntryBytes <= length,
                     error = ERROR_BAD_FORMAT("premature end of Child Table"));
        entry.mTimeout             = 1 << (((aBuf[offset] & 0xF8) >> 3) - 4);
        entry.mIncomingLinkQuality = (aBuf[offset] & 0x06) >> 1;
        entry.mChildId             = ((aBuf[offset] & 0x01) << 9) | aBuf[offset + 1];
        SuccessOrExit(error = DecodeModeData(entry.mModeData, {aBuf.begin() + offset + 2, aBuf.begin() + offset + 3}));
        aChildTable.emplace_back(entry);
        offset += kChildTableEntryBytes;
    }

exit:
    return error;
}

Error internal::DecodeLeaderData(LeaderData &aLeaderData, const ByteArray &aBuf)
{
    Error  error;
    size_t length = aBuf.size();
    VerifyOrExit(length == kLeaderDataBytes, error = ERROR_BAD_FORMAT("incorrect size of LeaderData"));
    aLeaderData.mPartitionId       = utils::Decode<uint32_t>(aBuf.data(), 4);
    aLeaderData.mWeighting         = aBuf[4];
    aLeaderData.mDataVersion       = aBuf[5];
    aLeaderData.mStableDataVersion = aBuf[6];
    aLeaderData.mRouterId          = aBuf[7];

exit:
    return error;
}

Error internal::DecodeRoute64(Route64 &aRoute64, const ByteArray &aBuf)
{
    Error     error;
    size_t    length = aBuf.size();
    size_t    offset = 0;
    ByteArray routerIdList;

    VerifyOrExit(length >= kRouterIdMaskBytes + 1, error = ERROR_BAD_FORMAT("incorrect size of Route64"));
    aRoute64.mIdSequence = aBuf[offset++];
    aRoute64.mMask       = {aBuf.begin() + offset, aBuf.begin() + offset + kRouterIdMaskBytes};
    offset += kRouterIdMaskBytes;

    routerIdList = ExtractRouterIds(aRoute64.mMask);
    VerifyOrExit((length - offset) == routerIdList.size(), error = ERROR_BAD_FORMAT("incorrect size of RouteData"));
    while (offset < length)
    {
        RouteDataEntry entry;
        entry.mRouterId = routerIdList[offset - kRouterIdMaskBytes - 1];
        DecodeRouteDataEntry(entry, aBuf[offset]);
        aRoute64.mRouteData.emplace_back(entry);
        offset++;
    }

exit:
    return error;
}

void internal::DecodeRouteDataEntry(RouteDataEntry &aRouteDataEntry, uint8_t aBuf)
{
    aRouteDataEntry.mOutgoingLinkQuality = (aBuf >> 6) & 0x03;
    aRouteDataEntry.mIncomingLinkQuality = (aBuf >> 4) & 0x03;
    aRouteDataEntry.mRouteCost           = aBuf & 0x0F;
}

ByteArray internal::ExtractRouterIds(const ByteArray &aMask)
{
    ByteArray routerIdList;

    for (size_t i = 0; i < kRouterIdMaskBytes * 8; i++)
    {
        if ((aMask[i / 8] & (0x80 >> (i % 8))) != 0)
        {
            routerIdList.push_back(i);
        }
    }

    return routerIdList;
}

Error internal::DecodeMacCounters(MacCounters &aMacCounters, const ByteArray &aBuf)
{
    Error  error;
    size_t length = aBuf.size();
    VerifyOrExit(length == kMacCountersBytes, error = ERROR_BAD_FORMAT("incorrect size of MacCounters"));
    aMacCounters.mIfInUnknownProtos  = utils::Decode<uint32_t>(aBuf.data(), 4);
    aMacCounters.mIfInErrors         = utils::Decode<uint32_t>(aBuf.data() + 4, 4);
    aMacCounters.mIfOutErrors        = utils::Decode<uint32_t>(aBuf.data() + 8, 4);
    aMacCounters.mIfInUcastPkts      = utils::Decode<uint32_t>(aBuf.data() + 12, 4);
    aMacCounters.mIfInBroadcastPkts  = utils::Decode<uint32_t>(aBuf.data() + 16, 4);
    aMacCounters.mIfInDiscards       = utils::Decode<uint32_t>(aBuf.data() + 20, 4);
    aMacCounters.mIfOutUcastPkts     = utils::Decode<uint32_t>(aBuf.data() + 24, 4);
    aMacCounters.mIfOutBroadcastPkts = utils::Decode<uint32_t>(aBuf.data() + 28, 4);
    aMacCounters.mIfOutDiscards      = utils::Decode<uint32_t>(aBuf.data() + 32, 4);

exit:
    return error;
}

/**
 * @brief Decodes a Connectivity TLV into a Connectivity struct.
 *
 * The Connectivity TLV provides information about a device's connection quality and
 * its relationship with neighboring devices. The TLV has a minimum length of 7 bytes,
 * with two additional optional fields.
 *
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |0|1|2|3|4|5|6|7|8|9|1|1|2|3|4|5|6|7|8|9|2|1|2|3|4|5|6|7|8|9|3|1|
 * | | | | | | | | | | |0| | | | | | | | | |0| | | | | | | | | |0| |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |PP |Reserved   |Link Quality 3 |Link Quality 2 |Link Quality 1 |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |Leader Cost    |ID Sequence    |Active Routers |Rx-off Child   |
 * |               |               |               |Buffer Size    |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |Rx-off Child   |Rx-off Child   |
 * |Buffer Size    |Datagram Count |
 * +---+---+---+---+---+---+---+---+
 *
 * @param[out] aConnectivity  A reference to the Connectivity struct to be populated.
 * @param[in]  aBuf         The raw byte array from the TLV value field.
 *
 * @returns An Error object indicating the success or failure of the decoding.
 */
Error internal::DecodeConnectivity(Connectivity &aConnectivity, const ByteArray &aBuf)
{
    Error          error;
    size_t         length = aBuf.size();
    const uint8_t *cur    = aBuf.data();
    const uint8_t *end    = cur + aBuf.size();
    uint8_t        byte0;
    int8_t         pp;

    // A valid Connectivity TLV must have a minimum length of 7 bytes.
    VerifyOrExit(length >= 7, error = {ErrorCode::kBadFormat, "invalid connectivity tlv length"});

    // Byte 0: Parent Priority and Reserved bits
    byte0 = *cur++;
    pp    = byte0 & 0x03;
    if (pp == 0b11)
    {
        pp = -1;
    }
    else if (pp == 0b10)
    {
        pp = -2;
    } // Reserved value
    aConnectivity.mParentPriority = pp;
    aConnectivity.mLinkQuality3   = *cur++;
    aConnectivity.mLinkQuality2   = *cur++;
    aConnectivity.mLinkQuality1   = *cur++;
    aConnectivity.mLeaderCost     = *cur++;
    aConnectivity.mIdSequence     = *cur++;
    aConnectivity.mActiveRouters  = *cur++;

    // If the size is 10, decode the optional fields.
    if (length >= 10)
    {
        aConnectivity.mPresentFlags |= Connectivity::kRxOffChildBufferSizeBit;
        aConnectivity.mRxOffChildBufferSize = utils::Decode<uint16_t>(cur, 2);
        cur += 2;

        aConnectivity.mPresentFlags |= Connectivity::kRxOffChildDatagramCountBit;
        aConnectivity.mRxOffChildDatagramCount = *cur++;
    }

    // Ensure we have consumed all bytes of the TLV.
    if (cur != end)
    {
        LOG_WARN(LOG_REGION_MESHDIAG, "malformed connectivity tlv, {} trailing bytes", std::distance(cur, end));
    }

exit:
    return error;
}

Error internal::DecodeChild(std::vector<Child> &aChild, const ByteArray &aBuf)
{
    Error   error;
    Child   childInfo;
    uint8_t flags;

    VerifyOrExit(aBuf.size() >= kChildBytes, error = {ErrorCode::kBadFormat, "invalid child tlv length"});

    // Flags (1 byte at offset 0)
    flags                         = aBuf[0];
    childInfo.mIsRxOnWhenIdle     = (flags & 0x80) != 0;
    childInfo.mIsDeviceTypeMtd    = (flags & 0x40) != 0;
    childInfo.mHasNetworkData     = (flags & 0x20) != 0;
    childInfo.mSupportsCsl        = (flags & 0x10) != 0;
    childInfo.mSupportsErrorRates = (flags & 0x08) != 0;

    // Decode all other fields into childInfo
    childInfo.mRloc16 = utils::Decode<uint16_t>(aBuf.data() + 1, 2);
    childInfo.mExtAddress.assign(aBuf.begin() + 3, aBuf.begin() + 11);
    childInfo.mThreadVersion       = utils::Decode<uint16_t>(aBuf.data() + 11, 2);
    childInfo.mTimeout             = utils::Decode<uint32_t>(aBuf.data() + 13, 4);
    childInfo.mAge                 = utils::Decode<uint32_t>(aBuf.data() + 17, 4);
    childInfo.mConnectionTime      = utils::Decode<uint32_t>(aBuf.data() + 21, 4);
    childInfo.mSupervisionInterval = utils::Decode<uint16_t>(aBuf.data() + 25, 2);
    childInfo.mLinkMargin          = aBuf[27];
    childInfo.mAverageRssi         = static_cast<int8_t>(aBuf[28]);
    childInfo.mLastRssi            = static_cast<int8_t>(aBuf[29]);
    childInfo.mFrameErrorRate      = utils::Decode<uint16_t>(aBuf.data() + 30, 2);
    childInfo.mMessageErrorRate    = utils::Decode<uint16_t>(aBuf.data() + 32, 2);
    childInfo.mQueuedMessageCount  = utils::Decode<uint16_t>(aBuf.data() + 34, 2);
    childInfo.mCslPeriod           = utils::Decode<uint16_t>(aBuf.data() + 36, 2);
    childInfo.mCslTimeout          = utils::Decode<uint32_t>(aBuf.data() + 38, 4);
    childInfo.mCslChannel          = aBuf[42];

    aChild.emplace_back(childInfo);

exit:
    return error;
}

Error internal::DecodeRouterNeighbor(std::vector<RouterNeighbor> &aRouterNeighbor, const ByteArray &aBuf)
{
    Error          error;
    RouterNeighbor neighborInfo;
    uint8_t        flags;

    VerifyOrExit(aBuf.size() >= kRouterNeighborBytes,
                 error = {ErrorCode::kBadFormat, "invalid router neighbor tlv length"});

    flags                            = aBuf[0];
    neighborInfo.mSupportsErrorRates = (flags & 0x80) != 0;

    neighborInfo.mRloc16 = utils::Decode<uint16_t>(aBuf.data() + 1, 2);
    neighborInfo.mExtAddress.assign(aBuf.begin() + 3, aBuf.begin() + 11);
    neighborInfo.mThreadVersion    = utils::Decode<uint16_t>(aBuf.data() + 11, 2);
    neighborInfo.mConnectionTime   = utils::Decode<uint32_t>(aBuf.data() + 13, 4);
    neighborInfo.mLinkMargin       = aBuf[17];
    neighborInfo.mAverageRssi      = static_cast<int8_t>(aBuf[18]);
    neighborInfo.mLastRssi         = static_cast<int8_t>(aBuf[19]);
    neighborInfo.mFrameErrorRate   = utils::Decode<uint16_t>(aBuf.data() + 20, 2);
    neighborInfo.mMessageErrorRate = utils::Decode<uint16_t>(aBuf.data() + 22, 2);

    aRouterNeighbor.push_back(neighborInfo);

exit:
    return error;
}

Error internal::DecodeMleCounters(MleCounters &aCounters, const ByteArray &aBuf)
{
    Error error;

    VerifyOrExit(aBuf.size() >= kMleCountersBytes, error = {ErrorCode::kBadFormat, "invalid mle counters tlv length"});

    aCounters.mRadioDisabledCounter                 = utils::Decode<uint16_t>(aBuf.data() + 0, 2);
    aCounters.mDetachedRoleCounter                  = utils::Decode<uint16_t>(aBuf.data() + 2, 2);
    aCounters.mChildRoleCounter                     = utils::Decode<uint16_t>(aBuf.data() + 4, 2);
    aCounters.mRouterRoleCounter                    = utils::Decode<uint16_t>(aBuf.data() + 6, 2);
    aCounters.mLeaderRoleCounter                    = utils::Decode<uint16_t>(aBuf.data() + 8, 2);
    aCounters.mAttachAttemptsCounter                = utils::Decode<uint16_t>(aBuf.data() + 10, 2);
    aCounters.mPartitionIdChangesCounter            = utils::Decode<uint16_t>(aBuf.data() + 12, 2);
    aCounters.mBetterPartitionAttachAttemptsCounter = utils::Decode<uint16_t>(aBuf.data() + 14, 2);
    aCounters.mNewParentCounter                     = utils::Decode<uint16_t>(aBuf.data() + 16, 2);
    aCounters.mTotalTrackingTime                    = utils::Decode<uint64_t>(aBuf.data() + 18, 8);
    aCounters.mRadioDisabledTime                    = utils::Decode<uint64_t>(aBuf.data() + 26, 8);
    aCounters.mDetachedRoleTime                     = utils::Decode<uint64_t>(aBuf.data() + 34, 8);
    aCounters.mChildRoleTime                        = utils::Decode<uint64_t>(aBuf.data() + 42, 8);
    aCounters.mRouterRoleTime                       = utils::Decode<uint64_t>(aBuf.data() + 50, 8);
    aCounters.mLeaderRoleTime                       = utils::Decode<uint64_t>(aBuf.data() + 58, 8);

exit:
    return error;
}

Error internal::DecodeNonPreferredChannelsMask(ChannelMask &aChannelMask, const ByteArray &aBuf)
{
    Error       error;
    ChannelMask channelMask;
    size_t      offset = 0;
    size_t      length = aBuf.size();

    while (offset < length)
    {
        ChannelMaskEntry entry;
        uint8_t          entryLength;
        VerifyOrExit(offset + 2 <= length, error = ERROR_BAD_FORMAT("premature end of Channel Mask Entry"));

        entry.mPage = aBuf[offset++];
        entryLength = aBuf[offset++];

        VerifyOrExit(offset + entryLength <= length, error = ERROR_BAD_FORMAT("premature end of Channel Mask Entry"));
        entry.mMasks = {aBuf.begin() + offset, aBuf.begin() + offset + entryLength};
        channelMask.emplace_back(entry);

        offset += entryLength;
    }

    VerifyOrExit(offset == length, error = ERROR_BAD_FORMAT("trailing bytes in Channel Mask TLV"));

    aChannelMask = channelMask;

exit:
    return error;
}

void CommissionerImpl::SendProxyMessage(ErrorHandler aHandler, const std::string &aDstAddr, const std::string &aUriPath)
{
    Error         error;
    Address       dstAddr;
    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};

    auto onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        aHandler(HandleStateResponse(aResponse, aError, /* aStateTlvIsMandatory */ false));
    };

    SuccessOrExit(error = dstAddr.Set(aDstAddr));

    SuccessOrExit(error = request.SetUriPath(aUriPath));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kCommissionerSessionId, GetSessionId()}));

#if OT_COMM_CONFIG_CCM_ENABLE
    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }
#endif

    mProxyClient.SendRequest(request, onResponse, dstAddr, kDefaultMmPort);

exit:
    if (error != ErrorCode::kNone)
    {
        aHandler(error);
    }
}

void CommissionerImpl::HandleDatasetChanged(const coap::Request &aRequest)
{
    LOG_INFO(LOG_REGION_MGMT, "received MGMT_DATASET_CHANGED.ntf from {}",
             aRequest.GetEndpoint()->GetPeerAddr().ToString());

    mProxyClient.SendEmptyChanged(aRequest);

    // Clear the cached Mesh-Local prefix so that the UDP Proxy client
    // will request for the new Mesh-Local prefix before sending next
    // UDP_TX.ntf message.
    mProxyClient.ClearMeshLocalPrefix();

    mCommissionerHandler.OnDatasetChanged();
}

void CommissionerImpl::HandlePanIdConflict(const coap::Request &aRequest)
{
    Error       error;
    tlv::TlvSet tlvSet;
    tlv::TlvPtr channelMaskTlv;
    tlv::TlvPtr panIdTlv;
    ChannelMask channelMask;
    uint16_t    panId;
    std::string peerAddr = aRequest.GetEndpoint()->GetPeerAddr().ToString();

    LOG_INFO(LOG_REGION_MGMT, "received MGMT_PANID_CONFLICT.ans from {}", peerAddr);

    mProxyClient.SendEmptyChanged(aRequest);

    SuccessOrExit(error = GetTlvSet(tlvSet, aRequest));
    VerifyOrExit((channelMaskTlv = tlvSet[tlv::Type::kChannelMask]) != nullptr,
                 error = ERROR_BAD_FORMAT("no valid Channel Mask TLV in MGMT_PANID_CONFLICT.ans"));
    VerifyOrExit((panIdTlv = tlvSet[tlv::Type::kPanId]) != nullptr,
                 error = ERROR_BAD_FORMAT("no valid PAN ID TLV in MGMT_PANID_CONFLICT.ans"));

    SuccessOrExit(error = DecodeChannelMask(channelMask, channelMaskTlv->GetValue()));
    panId = panIdTlv->GetValueAsUint16();

    mCommissionerHandler.OnPanIdConflict(peerAddr, channelMask, panId);

exit:
    if (error != ErrorCode::kNone)
    {
        LOG_WARN(LOG_REGION_MGMT, "handle MGMT_PANID_CONFLICT.ans from {} failed: {}", peerAddr, error.ToString());
    }
}

void CommissionerImpl::HandleEnergyReport(const coap::Request &aRequest)
{
    Error       error;
    tlv::TlvSet tlvSet;
    ChannelMask channelMask;
    ByteArray   energyList;
    std::string peerAddr = aRequest.GetEndpoint()->GetPeerAddr().ToString();

    LOG_INFO(LOG_REGION_MGMT, "received MGMT_ED_REPORT.ans from {}", peerAddr);

    mProxyClient.SendEmptyChanged(aRequest);

    SuccessOrExit(error = GetTlvSet(tlvSet, aRequest));
    if (auto channelMaskTlv = tlvSet[tlv::Type::kChannelMask])
    {
        SuccessOrExit(error = DecodeChannelMask(channelMask, channelMaskTlv->GetValue()));
    }
    if (auto eneryListTlv = tlvSet[tlv::Type::kEnergyList])
    {
        energyList = eneryListTlv->GetValue();
    }

    mCommissionerHandler.OnEnergyReport(peerAddr, channelMask, energyList);

exit:
    if (error != ErrorCode::kNone)
    {
        LOG_WARN(LOG_REGION_MGMT, "handle MGMT_ED_REPORT.ans from {} failed: {}", peerAddr, error.ToString());
    }
}

Error CommissionerImpl::MakeChannelMask(ByteArray &aBuf, uint32_t aChannelMask)
{
    Error            error;
    ChannelMaskEntry entry;

    if (kRadio915Mhz)
    {
        if (aChannelMask & kRadio915MhzOqpskChannelMask)
        {
            entry.mPage = kRadioChannelPage2;
            utils::Encode(entry.mMasks, kRadio915MhzOqpskChannelMask);
        }
    }
    else
    {
        if (aChannelMask & kRadio2P4GhzOqpskChannelMask)
        {
            entry.mPage = kRadioChannelPage0;
            utils::Encode(entry.mMasks, kRadio2P4GhzOqpskChannelMask);
        }
    }

    VerifyOrExit(!entry.mMasks.empty(), error = ERROR_INVALID_ARGS("no valid Channel Masks provided"));
    SuccessOrDie(EncodeChannelMask(aBuf, {entry}));

exit:
    return error;
}

void CommissionerImpl::HandleRlyRx(const coap::Request &aRlyRx)
{
    Error       error;
    tlv::TlvSet tlvSet;
    tlv::TlvPtr tlv;

    std::string joinerPSKd;
    uint16_t    joinerUdpPort;
    uint16_t    joinerRouterLocator;
    ByteArray   joinerIid;
    ByteArray   joinerId;
    ByteArray   dtlsRecords;

    SuccessOrExit(error = GetTlvSet(tlvSet, aRlyRx));

    VerifyOrExit((tlv = tlvSet[tlv::Type::kJoinerUdpPort]) != nullptr,
                 error = ERROR_BAD_FORMAT("no valid Joiner UDP Port TLV found"));
    joinerUdpPort = tlv->GetValueAsUint16();

    VerifyOrExit((tlv = tlvSet[tlv::Type::kJoinerRouterLocator]) != nullptr,
                 error = ERROR_BAD_FORMAT("no valid Joiner Router Locator TLV found"));
    joinerRouterLocator = tlv->GetValueAsUint16();

    VerifyOrExit((tlv = tlvSet[tlv::Type::kJoinerIID]) != nullptr,
                 error = ERROR_BAD_FORMAT("no valid Joiner IID TLV found"));
    joinerIid = tlv->GetValue();

    VerifyOrExit((tlv = tlvSet[tlv::Type::kJoinerDtlsEncapsulation]) != nullptr,
                 error = ERROR_BAD_FORMAT("no valid Joiner DTLS Encapsulation TLV found"));
    dtlsRecords = tlv->GetValue();

    joinerId = joinerIid;
    joinerId[0] ^= kLocalExternalAddrMask;
    LOG_DEBUG(LOG_REGION_JOINER_SESSION,
              "received RLY_RX.ntf: joinerID={}, joinerRouterLocator={}, joinerPort={} length={}", utils::Hex(joinerId),
              joinerRouterLocator, joinerUdpPort, dtlsRecords.size());

    {
        auto it = mJoinerSessions.find(joinerId);
        if (it != mJoinerSessions.end() && it->second.Disabled())
        {
            LOG_DEBUG(LOG_REGION_JOINER_SESSION, "received RLY_RX.ntf, but joiner is disabled");
            mJoinerSessions.erase(it);
            it = mJoinerSessions.end();
        }

        if (it == mJoinerSessions.end())
        {
            Address localAddr;

            joinerPSKd = mCommissionerHandler.OnJoinerRequest(joinerId);
            if (joinerPSKd.empty() && !mConfig.mProxyMode)
            {
                LOG_INFO(LOG_REGION_JOINER_SESSION, "joiner(ID={}) is disabled", utils::Hex(joinerId));
                ExitNow(error = ERROR_REJECTED("joiner(ID={}) is disabled", utils::Hex(joinerId)));
            }

            SuccessOrExit(error = mBrClient.GetLocalAddr(localAddr));
            it = mJoinerSessions
                     .emplace(std::piecewise_construct, std::forward_as_tuple(joinerId),
                              std::forward_as_tuple(*this, joinerId, joinerPSKd, joinerUdpPort, joinerRouterLocator,
                                                    aRlyRx.GetEndpoint()->GetPeerAddr(),
                                                    aRlyRx.GetEndpoint()->GetPeerPort(), localAddr,
                                                    kListeningJoinerPort))
                     .first;
            auto &session = it->second;

            std::string peerAddr = session.GetPeerAddr().ToString();

            LOG_DEBUG(LOG_REGION_JOINER_SESSION, "received a new joiner(ID={}) DTLS connection from [{}]:{}",
                      utils::Hex(joinerId), peerAddr, session.GetPeerPort());

            session.Start();

            LOG_INFO(LOG_REGION_JOINER_SESSION, "joiner session timer started, expiration-time={}",
                     TimePointToString(session.GetExpirationTime()));
            mJoinerSessionTimer.Start(session.GetExpirationTime());
        }

        ASSERT(it != mJoinerSessions.end());
        auto &session = it->second;
        session.RecvJoinerDtlsRecords(dtlsRecords, joinerUdpPort);
    }

exit:
    if (error != ErrorCode::kNone)
    {
        LOG_ERROR(LOG_REGION_JOINER_SESSION, "failed to handle RLY_RX.ntf message: {}", error.ToString());
    }
}

void CommissionerImpl::HandleJoinerSessionTimer(Timer &aTimer)
{
    TimePoint nextShot;
    bool      hasNextShot = false;
    auto      now         = Clock::now();

    LOG_DEBUG(LOG_REGION_JOINER_SESSION, "joiner session timer triggered");

    auto it = mJoinerSessions.begin();
    while (it != mJoinerSessions.end())
    {
        auto &session = it->second;

        if (now >= session.GetExpirationTime())
        {
            it = mJoinerSessions.erase(it);

            LOG_INFO(LOG_REGION_JOINER_SESSION, "joiner session (joiner ID={}) removed",
                     utils::Hex(session.GetJoinerId()));
        }
        else
        {
            if (!hasNextShot || session.GetExpirationTime() < nextShot)
            {
                nextShot = session.GetExpirationTime();
            }
            hasNextShot = true;

            it++;
        }
    }

    if (hasNextShot)
    {
        aTimer.Start(nextShot);
    }
}

Error CommissionerImpl::SendToJoiner(const ByteArray &aJoinerId, uint16_t aPort, const ByteArray &aPayload)
{
    auto it = mJoinerSessions.find(aJoinerId);
    if (it == mJoinerSessions.end())
    {
        LOG_WARN(LOG_REGION_MGMT, "Joiner[{}] not found", utils::Hex(aJoinerId));
        return ERROR_NOT_FOUND("Joiner not found");
    }

    auto error = it->second.SendTo(aPort, aPayload.data(), aPayload.size());
    LOG_INFO(LOG_REGION_MGMT, "Send to joiner[{}] at port={}: {}", utils::Hex(aJoinerId), aPort,
             error.GetMessage().c_str());
    return error;
}

} // namespace commissioner

} // namespace ot
