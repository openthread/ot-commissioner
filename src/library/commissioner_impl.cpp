/*
 *    Copyright (c) 2019, The OpenThread Authors.
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

#include "library/coap.hpp"
#include "library/cose.hpp"
#include "library/dtls.hpp"
#include "library/logging.hpp"
#include "library/openthread/bloom_filter.hpp"
#include "library/openthread/pbkdf2_cmac.hpp"
#include "library/openthread/sha256.hpp"
#include "library/uri.hpp"

namespace ot {

namespace commissioner {

static constexpr uint8_t kLocalExternalAddrMask = 1 << 1;

Error Commissioner::GeneratePSKc(ByteArray &        aPSKc,
                                 const std::string &aPassphrase,
                                 const std::string &aNetworkName,
                                 const ByteArray &  aExtendedPanId)
{
    Error             error      = Error::kNone;
    const std::string saltPrefix = "Thread";
    ByteArray         salt;

    VerifyOrExit((aPassphrase.size() >= kMinCommissionerPassphraseLength) &&
                     (aPassphrase.size() <= kMaxCommissionerPassPhraseLength),
                 error = Error::kInvalidArgs);
    VerifyOrExit(aNetworkName.size() <= kMaxNetworkNameLength, error = Error::kInvalidArgs);
    VerifyOrExit(aExtendedPanId.size() == kExtendedPanIdLength, error = Error::kInvalidArgs);

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

Error Commissioner::GetMeshLocalAddr(std::string &      aMeshLocalAddr,
                                     const std::string &aMeshLocalPrefix,
                                     uint16_t           aLocator16)
{
    Error     error = Error::kNone;
    ByteArray rawAddr;
    Address   addr;

    SuccessOrExit(error = Ipv6PrefixFromString(rawAddr, aMeshLocalPrefix));

    VerifyOrExit(rawAddr.size() == 8, error = Error::kInvalidArgs);
    utils::Encode<uint16_t>(rawAddr, 0x0000);
    utils::Encode<uint16_t>(rawAddr, 0x00FF);
    utils::Encode<uint16_t>(rawAddr, 0xFE00);
    utils::Encode<uint16_t>(rawAddr, aLocator16);

    SuccessOrExit(addr.Set(rawAddr));
    SuccessOrExit(error = addr.ToString(aMeshLocalAddr));

exit:
    return error;
}

JoinerInfo::JoinerInfo(JoinerType aType, uint64_t aEui64, const ByteArray &aPSKd, const std::string &aProvisioningUrl)
    : mType(aType)
    , mEui64(aEui64)
    , mPSKd(aPSKd)
    , mProvisioningUrl(aProvisioningUrl)
{
}

CommissionerImpl::CommissionerImpl(struct event_base *aEventBase)
    : mState(State::kDisabled)
    , mSessionId(0)
    , mEventBase(aEventBase)
    , mKeepAliveTimer(mEventBase, [this](Timer &aTimer) { SendKeepAlive(aTimer); })
    , mBrClient(mEventBase)
    , mCommissioningSessionTimer(mEventBase, [this](Timer &aTimer) { HandleCommissioningSessionTimer(aTimer); })
    , mResourceUdpRx(uri::kUdpRx, [this](const coap::Request &aRequest) { mProxyClient.HandleUdpRx(aRequest); })
    , mResourceRlyRx(uri::kRelayRx, [this](const coap::Request &aRequest) { HandleRlyRx(aRequest); })
    , mProxyClient(mEventBase, mBrClient)
    , mTokenManager(mEventBase)
    , mResourceDatasetChanged(uri::kMgmtDatasetChanged,
                              [this](const coap::Request &aRequest) { HandleDatasetChanged(aRequest); })
    , mResourcePanIdConflict(uri::kMgmtPanidConflict,
                             [this](const coap::Request &aRequest) { HandlePanIdConflict(aRequest); })
    , mResourceEnergyReport(uri::kMgmtEdReport, [this](const coap::Request &aRequest) { HandleEnergyReport(aRequest); })
    , mDatasetChangedHandler(nullptr)
    , mPanIdConflictHandler(nullptr)
    , mEnergyReportHandler(nullptr)
    , mJoinerInfoRequester(nullptr)
    , mCommissioningHandler(nullptr)
{
    ASSERT(mBrClient.AddResource(mResourceUdpRx) == Error::kNone);
    ASSERT(mBrClient.AddResource(mResourceRlyRx) == Error::kNone);
    ASSERT(mProxyClient.AddResource(mResourceDatasetChanged) == Error::kNone);
    ASSERT(mProxyClient.AddResource(mResourcePanIdConflict) == Error::kNone);
    ASSERT(mProxyClient.AddResource(mResourceEnergyReport) == Error::kNone);
}

CommissionerImpl::~CommissionerImpl()
{
    Stop();

    mProxyClient.RemoveResource(mResourceEnergyReport);
    mProxyClient.RemoveResource(mResourcePanIdConflict);
    mProxyClient.RemoveResource(mResourceDatasetChanged);
    mBrClient.RemoveResource(mResourceRlyRx);
    mBrClient.RemoveResource(mResourceUdpRx);
}

Error CommissionerImpl::Init(const Config &aConfig)
{
    Error error = Error::kFailed;

    SuccessOrExit(error = ValidateConfig(aConfig));
    mConfig = aConfig;

    InitLogger(aConfig.mLogger);
    LoggingConfig();

    SuccessOrExit(error = mBrClient.Init(GetDtlsConfig(mConfig)));

    if (IsCcmMode())
    {
        // It is not good to leave the token manager uninitialized in non-CCM mode.
        // TODO(wgtdkp): create TokenManager only in CCM Mode.
        SuccessOrExit(error = mTokenManager.Init(mConfig));
    }

    error = Error::kNone;

exit:
    return error;
}

Error CommissionerImpl::ValidateConfig(const Config &aConfig)
{
    std::string error;

    {
        tlv::Tlv commissionerIdTlv{tlv::Type::kCommissionerId, aConfig.mId};
        VerifyOrExit(!aConfig.mId.empty(), error = "commissioner ID is mandatory");
        VerifyOrExit(commissionerIdTlv.IsValid(), error = "invalid commissioner ID: " + aConfig.mId);
    }

    VerifyOrExit(
        (aConfig.mKeepAliveInterval >= kMinKeepAliveInterval && aConfig.mKeepAliveInterval <= kMaxKeepAliveInterval),
        error = "invalid keep alive interval");

    if (aConfig.mEnableCcm)
    {
        tlv::Tlv domainNameTlv{tlv::Type::kDomainName, aConfig.mDomainName};
        VerifyOrExit(!aConfig.mDomainName.empty(), error = "domain name is mandatory for CCM network");
        VerifyOrExit(domainNameTlv.IsValid(), error = "invalid domain name (too long): " + aConfig.mDomainName);
        VerifyOrExit(!aConfig.mPrivateKey.empty(), error = "private key is mandatory for CCM network");
        VerifyOrExit(!aConfig.mCertificate.empty(), error = "certificate is mandatory for CCM network");
        VerifyOrExit(!aConfig.mTrustAnchor.empty(), error = "trust anchor is mandatory for CCM network");
    }
    else
    {
        // Should we also enable setting PSKc from passphrase?
        VerifyOrExit(!aConfig.mPSKc.empty(), error = "PSKc is mandatory for non-CCM network");
        VerifyOrExit(aConfig.mPSKc.size() <= kMaxPSKcLength, error = "PSKc is too long");
    }

exit:
    if (!error.empty())
    {
        LOG_WARN("bad configuration: {}", error);
        return Error::kFailed;
    }
    else
    {
        return Error::kNone;
    }
}

void CommissionerImpl::LoggingConfig()
{
    LOG_INFO("config: Id = {}", mConfig.mId);
    LOG_INFO("config: enable CCM = {}", mConfig.mEnableCcm);
    LOG_INFO("config: domain name = {}", mConfig.mDomainName);
    LOG_INFO("config: keep alive interval = {}", mConfig.mKeepAliveInterval);
    LOG_INFO("config: enable DTLS debug logging = {}", mConfig.mEnableDtlsDebugLogging);
    LOG_INFO("config: maximum connection number = {}", mConfig.mMaxConnectionNum);

    // Do not logging credentials
}

const Config &CommissionerImpl::GetConfig() const
{
    return mConfig;
}

void CommissionerImpl::SetJoinerInfoRequester(JoinerInfoRequester aJoinerInfoRequester)
{
    mJoinerInfoRequester = aJoinerInfoRequester;
}

void CommissionerImpl::SetCommissioningHandler(CommissioningHandler aCommissioningHandler)
{
    mCommissioningHandler = aCommissioningHandler;
}

Error CommissionerImpl::Start()
{
    LOG_INFO("event loop started in background thread");
    event_base_loop(mEventBase, EVLOOP_NO_EXIT_ON_EMPTY);
    return Error::kNone;
}

// Stop the commissioner event loop.
void CommissionerImpl::Stop()
{
    event_base_loopbreak(mEventBase);
}

void CommissionerImpl::Petition(PetitionHandler aHandler, const std::string &aAddr, uint16_t aPort)
{
    auto onConnected = [this, aHandler](Error aError) {
        if (aError != Error::kNone)
        {
            aHandler(nullptr, aError);
        }
        else
        {
            LOG_DEBUG("DTLS connection to border agent succeed");
            SendPetition(aHandler);
        }
    };

    LOG_DEBUG("starting petition: border agent = ({}, {})", aAddr, aPort);

    if (mBrClient.IsConnected())
    {
        SendPetition(aHandler);
    }
    else
    {
        Connect(onConnected, aAddr, aPort);
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

    Disconnect();

    aHandler(Error::kNone);
}

void CommissionerImpl::Connect(ErrorHandler aHandler, const std::string &aAddr, uint16_t aPort)
{
    auto onConnected = [aHandler](const DtlsSession &, Error aError) { aHandler(aError); };
    mBrClient.Connect(onConnected, aAddr, aPort);
}

void CommissionerImpl::Disconnect()
{
    mBrClient.Disconnect();
}

uint16_t CommissionerImpl::GetSessionId() const
{
    return mSessionId;
}

State CommissionerImpl::GetState() const
{
    return mState;
}

bool CommissionerImpl::IsActive() const
{
    return GetState() == State::kActive;
}

bool CommissionerImpl::IsCcmMode() const
{
    return mConfig.mEnableCcm;
}

const std::string &CommissionerImpl::GetDomainName() const
{
    return mConfig.mDomainName;
}

void CommissionerImpl::AbortRequests()
{
    mProxyClient.AbortRequests();
    mBrClient.AbortRequests();
    if (IsCcmMode())
    {
        mTokenManager.AbortRequests();
    }
}

void CommissionerImpl::GetCommissionerDataset(Handler<CommissionerDataset> aHandler, uint16_t aDatasetFlags)
{
    Error         error = Error::kFailed;
    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};
    ByteArray     tlvTypes = GetCommissionerDatasetTlvs(aDatasetFlags);

    auto onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        Error               error = Error::kFailed;
        CommissionerDataset dataset;

        SuccessOrExit(error = aError);
        ASSERT(aResponse != nullptr);

        VerifyOrExit(aResponse->GetCode() == coap::Code::kChanged, error = Error::kFailed);

        SuccessOrExit(error = DecodeCommissionerDataset(dataset, *aResponse));

        error = Error::kNone;
        aHandler(&dataset, error);

    exit:
        if (error != Error::kNone)
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

    error = Error::kNone;
    mBrClient.SendRequest(request, onResponse);

    LOG_DEBUG("sent MGMT_COMMISSIONER_GET.req");

exit:
    if (error != Error::kNone)
    {
        aHandler(nullptr, error);
    }
}

void CommissionerImpl::SetCommissionerDataset(ErrorHandler aHandler, const CommissionerDataset &aDataset)
{
    Error         error = Error::kFailed;
    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};

    auto onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        aHandler(HandleStateResponse(aResponse, aError));
    };

    VerifyOrExit(aDataset.mPresentFlags != 0, error = Error::kInvalidArgs);
    VerifyOrExit((aDataset.mPresentFlags & CommissionerDataset::kSessionIdBit) == 0, error = Error::kInvalidArgs);
    VerifyOrExit((aDataset.mPresentFlags & CommissionerDataset::kBorderAgentLocatorBit) == 0,
                 error = Error::kInvalidArgs);

    // TODO(wgtdkp): verify if every joiner UDP port differs from each other (required by Thread).
    //               Otherwise, this request may fail.

    SuccessOrExit(error = request.SetUriPath(uri::kMgmtCommissionerSet));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kCommissionerSessionId, GetSessionId()}));
    SuccessOrExit(error = EncodeCommissionerDataset(request, aDataset));

    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }

    error = Error::kNone;
    mBrClient.SendRequest(request, onResponse);

    LOG_DEBUG("sent MGMT_COMMISSIONER_SET.req");

exit:
    if (error != Error::kNone)
    {
        aHandler(error);
    }
}

void CommissionerImpl::SetBbrDataset(ErrorHandler aHandler, const BbrDataset &aDataset)
{
    Error error = Error::kNone;

    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};

    auto onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        aHandler(HandleStateResponse(aResponse, aError));
    };

    VerifyOrExit(IsCcmMode(), error = Error::kInvalidState);
    VerifyOrExit((aDataset.mPresentFlags & BbrDataset::kRegistrarIpv6AddrBit) == 0, error = Error::kInvalidArgs);

    SuccessOrExit(error = request.SetUriPath(uri::kMgmtBbrSet));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kCommissionerSessionId, GetSessionId()}));
    SuccessOrExit(error = EncodeBbrDataset(request, aDataset));

    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }

    error = Error::kNone;
    mBrClient.SendRequest(request, onResponse);

    LOG_DEBUG("sent MGMT_BBR_SET.req");

exit:
    if (error != Error::kNone)
    {
        aHandler(error);
    }
}

void CommissionerImpl::GetBbrDataset(Handler<BbrDataset> aHandler, uint16_t aDatasetFlags)
{
    Error error = Error::kNone;

    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};
    ByteArray     datasetList = GetBbrDatasetTlvs(aDatasetFlags);

    auto onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        Error      error = Error::kNone;
        BbrDataset dataset;

        SuccessOrExit(error = aError);
        VerifyOrExit(aResponse->GetCode() == coap::Code::kChanged, error = Error::kFailed);

        SuccessOrExit(error = DecodeBbrDataset(dataset, *aResponse));

        error = Error::kNone;
        aHandler(&dataset, error);

    exit:
        if (error != Error::kNone)
        {
            aHandler(nullptr, error);
        }
    };

    VerifyOrExit(IsCcmMode(), error = Error::kInvalidState);

    SuccessOrExit(error = request.SetUriPath(uri::kMgmtBbrGet));

    if (!datasetList.empty())
    {
        SuccessOrExit(error = AppendTlv(request, {tlv::Type::kGet, datasetList}));
    }

    mBrClient.SendRequest(request, onResponse);

    LOG_DEBUG("sent MGMT_BBR_GET.req");

exit:
    if (error != Error::kNone)
    {
        aHandler(nullptr, error);
    }
}

void CommissionerImpl::GetActiveDataset(Handler<ActiveOperationalDataset> aHandler, uint16_t aDatasetFlags)
{
    Error         error = Error::kNone;
    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};
    ByteArray     datasetList = GetActiveOperationalDatasetTlvs(aDatasetFlags);

    auto onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        Error                    error = Error::kNone;
        ActiveOperationalDataset dataset;

        SuccessOrExit(error = aError);
        VerifyOrExit(aResponse->GetCode() == coap::Code::kChanged, error = Error::kFailed);

        SuccessOrExit(error = DecodeActiveOperationalDataset(dataset, *aResponse));
        VerifyOrExit(dataset.mPresentFlags & ActiveOperationalDataset::kActiveTimestampBit, error = Error::kBadFormat);

        error = Error::kNone;
        aHandler(&dataset, error);

    exit:
        if (error != Error::kNone)
        {
            aHandler(nullptr, error);
        }
    };

    SuccessOrExit(error = request.SetUriPath(uri::kMgmtActiveGet));
    if (!datasetList.empty())
    {
        SuccessOrExit(error = AppendTlv(request, {tlv::Type::kGet, datasetList}));
    }

    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }

    mBrClient.SendRequest(request, onResponse);

    LOG_DEBUG("sent MGMT_ACTIVE_GET.req");

exit:
    if (error != Error::kNone)
    {
        aHandler(nullptr, error);
    }
}

void CommissionerImpl::SetActiveDataset(ErrorHandler aHandler, const ActiveOperationalDataset &aDataset)
{
    Error         error = Error::kNone;
    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};

    auto onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        aHandler(HandleStateResponse(aResponse, aError));
    };

    VerifyOrExit(aDataset.mPresentFlags & ActiveOperationalDataset::kActiveTimestampBit, error = Error::kInvalidArgs);

    // TLVs affect connectivity are not allowed.
    VerifyOrExit((aDataset.mPresentFlags & ActiveOperationalDataset::kChannelBit) == 0, error = Error::kInvalidArgs);
    VerifyOrExit((aDataset.mPresentFlags & ActiveOperationalDataset::kPanIdBit) == 0, error = Error::kInvalidArgs);
    VerifyOrExit((aDataset.mPresentFlags & ActiveOperationalDataset::kMeshLocalPrefixBit) == 0,
                 error = Error::kInvalidArgs);
    VerifyOrExit((aDataset.mPresentFlags & ActiveOperationalDataset::kNetworkMasterKeyBit) == 0,
                 error = Error::kInvalidArgs);

    SuccessOrExit(error = request.SetUriPath(uri::kMgmtActiveSet));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kCommissionerSessionId, GetSessionId()}));
    SuccessOrExit(error = EncodeActiveOperationalDataset(request, aDataset));

    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }

    error = Error::kNone;
    mBrClient.SendRequest(request, onResponse);

    LOG_DEBUG("sent MGMT_ACTIVE_SET.req");

exit:
    if (error != Error::kNone)
    {
        aHandler(error);
    }
}

void CommissionerImpl::GetPendingDataset(Handler<PendingOperationalDataset> aHandler, uint16_t aDatasetFlags)
{
    Error         error = Error::kNone;
    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};
    ByteArray     datasetList = GetPendingOperationalDatasetTlvs(aDatasetFlags);

    auto onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        Error                     error = Error::kNone;
        PendingOperationalDataset dataset;

        SuccessOrExit(error = aError);
        VerifyOrExit(aResponse->GetCode() == coap::Code::kChanged, error = Error::kFailed);

        SuccessOrExit(error = DecodePendingOperationalDataset(dataset, *aResponse));
        VerifyOrExit(dataset.mPresentFlags | PendingOperationalDataset::kActiveTimestampBit, error = Error::kBadFormat);
        VerifyOrExit(dataset.mPresentFlags | PendingOperationalDataset::kPendingTimestampBit,
                     error = Error::kBadFormat);
        VerifyOrExit(dataset.mPresentFlags | PendingOperationalDataset::kDelayTimerBit, error = Error::kBadFormat);

        error = Error::kNone;
        aHandler(&dataset, error);

    exit:
        if (error != Error::kNone)
        {
            aHandler(nullptr, error);
        }
    };

    SuccessOrExit(error = request.SetUriPath(uri::kMgmtPendingGet));
    if (!datasetList.empty())
    {
        SuccessOrExit(error = AppendTlv(request, {tlv::Type::kGet, datasetList}));
    }

    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }

    mBrClient.SendRequest(request, onResponse);

    LOG_DEBUG("sent MGMT_PENDING_GET.req");

exit:
    if (error != Error::kNone)
    {
        aHandler(nullptr, error);
    }
}

void CommissionerImpl::SetPendingDataset(ErrorHandler aHandler, const PendingOperationalDataset &aDataset)
{
    Error         error = Error::kNone;
    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};

    auto onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        aHandler(HandleStateResponse(aResponse, aError));
    };

    VerifyOrExit(aDataset.mPresentFlags & PendingOperationalDataset::kActiveTimestampBit, error = Error::kInvalidArgs);
    VerifyOrExit(aDataset.mPresentFlags & PendingOperationalDataset::kPendingTimestampBit, error = Error::kInvalidArgs);
    VerifyOrExit(aDataset.mPresentFlags & PendingOperationalDataset::kDelayTimerBit, error = Error::kInvalidArgs);

    SuccessOrExit(error = request.SetUriPath(uri::kMgmtPendingSet));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kCommissionerSessionId, GetSessionId()}));
    SuccessOrExit(error = EncodePendingOperationalDataset(request, aDataset));

    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }

    error = Error::kNone;
    mBrClient.SendRequest(request, onResponse);

    LOG_DEBUG("sent MGMT_PENDING_SET.req");

exit:
    if (error != Error::kNone)
    {
        aHandler(error);
    }
}

void CommissionerImpl::SetSecurePendingDataset(ErrorHandler                     aHandler,
                                               const std::string &              aPbbrAddr,
                                               uint32_t                         aMaxRetrievalTimer,
                                               const PendingOperationalDataset &aDataset)
{
    Error         error = Error::kNone;
    Address       dstAddr;
    ByteArray     secureDissemination;
    std::string   uri = "coaps://[" + aPbbrAddr + "]" + uri::kMgmtPendingGet;
    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};

    auto onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        aHandler(HandleStateResponse(aResponse, aError));
    };

    VerifyOrExit(IsCcmMode(), error = Error::kInvalidState);

    // Delay timer are mandatory.
    VerifyOrExit(aDataset.mPresentFlags & PendingOperationalDataset::kDelayTimerBit, error = Error::kInvalidArgs);

    SuccessOrExit(error = dstAddr.Set(aPbbrAddr));
    VerifyOrExit(dstAddr.IsValid(), error = Error::kInvalidArgs);

    SuccessOrExit(error = request.SetUriPath(uri::kMgmtSecPendingSet));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kCommissionerSessionId, GetSessionId()}));

    utils::Encode(secureDissemination, aDataset.mPendingTimestamp.Encode());
    utils::Encode(secureDissemination, aMaxRetrievalTimer);
    secureDissemination.insert(secureDissemination.end(), uri.begin(), uri.end());
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kSecureDissemination, secureDissemination}));

    SuccessOrExit(error = EncodePendingOperationalDataset(request, aDataset));

    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }

    error = Error::kNone;
    mProxyClient.SendRequest(request, onResponse, dstAddr, kDefaultMmPort);

    LOG_DEBUG("sent MGMT_SEC_PENDING_SET.req");

exit:
    if (error != Error::kNone)
    {
        aHandler(error);
    }
}

void CommissionerImpl::CommandReenroll(ErrorHandler aHandler, const std::string &aDstAddr)
{
    SendProxyMessage(aHandler, aDstAddr, uri::kMgmtReenroll);
}

void CommissionerImpl::CommandDomainReset(ErrorHandler aHandler, const std::string &aDstAddr)
{
    SendProxyMessage(aHandler, aDstAddr, uri::kMgmtDomainReset);
}

void CommissionerImpl::CommandMigrate(ErrorHandler       aHandler,
                                      const std::string &aDstAddr,
                                      const std::string &aDstNetworkName)
{
    Error         error = Error::kNone;
    Address       dstAddr;
    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};

    auto onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        Error       error    = Error::kNone;
        tlv::TlvPtr stateTlv = nullptr;

        SuccessOrExit(error = aError);
        VerifyOrExit(aResponse->GetCode() != coap::Code::kUnauthorized, error = Error::kSecurity);
        VerifyOrExit(aResponse->GetCode() == coap::Code::kChanged, error = Error::kFailed);

        stateTlv = GetTlv(tlv::Type::kState, *aResponse);
        if (stateTlv != nullptr)
        {
            VerifyOrExit(stateTlv->IsValid(), error = Error::kBadFormat);
            VerifyOrExit(stateTlv->GetValueAsInt8() == tlv::kStateAccept, error = Error::kReject);
        }

        error = Error::kNone;

    exit:
        aHandler(error);
    };

    VerifyOrExit(IsCcmMode(), error = Error::kInvalidState);

    SuccessOrExit(error = dstAddr.Set(aDstAddr));
    VerifyOrExit(dstAddr.IsValid(), error = Error::kInvalidArgs);

    VerifyOrExit(aDstNetworkName.size() <= kMaxNetworkNameLength, error = Error::kInvalidArgs);

    SuccessOrExit(error = request.SetUriPath(uri::kMgmtNetMigrate));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kCommissionerSessionId, GetSessionId()}));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kNetworkName, aDstNetworkName}));

    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }

    error = Error::kNone;
    mProxyClient.SendRequest(request, onResponse, dstAddr, kDefaultMmPort);

    LOG_DEBUG("sent MGMT_NET_MIGRATE.req");

exit:
    if (error != Error::kNone)
    {
        aHandler(error);
    }
}

void CommissionerImpl::RegisterMulticastListener(Handler<uint8_t>                aHandler,
                                                 const std::string &             aPbbrAddr,
                                                 const std::vector<std::string> &aMulticastAddrList,
                                                 uint32_t                        aTimeout)
{
    Error     error = Error::kNone;
    Address   dstAddr;
    Address   multicastAddr;
    ByteArray rawAddresses;

    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};

    auto onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        Error       error     = Error::kNone;
        tlv::TlvPtr statusTlv = nullptr;
        uint8_t     status;

        SuccessOrExit(error = aError);
        VerifyOrExit(aResponse->GetCode() != coap::Code::kUnauthorized, error = Error::kSecurity);
        VerifyOrExit(aResponse->GetCode() == coap::Code::kChanged, error = Error::kFailed);

        statusTlv = GetTlv(tlv::Type::kThreadStatus, *aResponse, tlv::Scope::kThread);
        VerifyOrExit(statusTlv != nullptr && statusTlv->IsValid(), error = Error::kBadFormat);

        error  = Error::kNone;
        status = statusTlv->GetValueAsUint8();
        aHandler(&status, error);

    exit:
        if (error != Error::kNone)
        {
            aHandler(nullptr, error);
        }
    };

    SuccessOrExit(error = dstAddr.Set(aPbbrAddr));
    VerifyOrExit(dstAddr.IsValid(), error = Error::kInvalidArgs);

    VerifyOrExit(!aMulticastAddrList.empty(), error = Error::kInvalidArgs);
    for (const auto &addr : aMulticastAddrList)
    {
        SuccessOrExit(error = multicastAddr.Set(addr));
        VerifyOrExit(multicastAddr.IsValid() && multicastAddr.IsIpv6() && multicastAddr.IsMulticast(),
                     error = Error::kInvalidArgs);
        rawAddresses.insert(rawAddresses.end(), multicastAddr.GetRaw().begin(), multicastAddr.GetRaw().end());
    }

    SuccessOrExit(error = request.SetUriPath(uri::kMlr));
    SuccessOrExit(
        error = AppendTlv(request, {tlv::Type::kThreadCommissionerSessionId, GetSessionId(), tlv::Scope::kThread}));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kThreadTimeout, aTimeout, tlv::Scope::kThread}));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kThreadIpv6Addresses, rawAddresses, tlv::Scope::kThread}));

    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }

    error = Error::kNone;
    mProxyClient.SendRequest(request, onResponse, dstAddr, kDefaultMmPort);

    LOG_DEBUG("sent MLR.req");

exit:
    if (error != Error::kNone)
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
    Error         error = Error::kNone;
    Address       dstAddr;
    ByteArray     channelMask;
    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};

    auto onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        Error error = Error::kNone;

        SuccessOrExit(error = aError);
        VerifyOrExit(aResponse->GetCode() != coap::Code::kUnauthorized, error = Error::kSecurity);
        VerifyOrExit(aResponse->GetCode() == coap::Code::kChanged, error = Error::kBadFormat);

    exit:
        aHandler(error);
    };

    SuccessOrExit(error = dstAddr.Set(aDstAddr));
    VerifyOrExit(dstAddr.IsValid(), error = Error::kInvalidArgs);

    if (dstAddr.IsMulticast())
    {
        request.SetType(coap::Type::kNonConfirmable);
    }

    SuccessOrExit(error = request.SetUriPath(uri::kMgmtAnnounceBegin));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kCommissionerSessionId, GetSessionId()}));
    SuccessOrExit(error = MakeChannelMask(channelMask, aChannelMask));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kChannelMask, channelMask}));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kCount, aCount}));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kPeriod, aPeriod}));

    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }

    error = Error::kNone;
    mProxyClient.SendRequest(request, onResponse, dstAddr, kDefaultMmPort);

exit:
    if (error != Error::kNone || request.IsNonConfirmable())
    {
        aHandler(error);
    }
}

void CommissionerImpl::PanIdQuery(ErrorHandler       aHandler,
                                  uint32_t           aChannelMask,
                                  uint16_t           aPanId,
                                  const std::string &aDstAddr)
{
    Error         error = Error::kNone;
    Address       dstAddr;
    ByteArray     channelMask;
    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};

    auto onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        Error error = Error::kNone;

        SuccessOrExit(error = aError);
        VerifyOrExit(aResponse->GetCode() != coap::Code::kUnauthorized, error = Error::kSecurity);
        VerifyOrExit(aResponse->GetCode() == coap::Code::kChanged, error = Error::kBadFormat);

    exit:
        aHandler(error);
    };

    SuccessOrExit(error = dstAddr.Set(aDstAddr));
    VerifyOrExit(dstAddr.IsValid(), error = Error::kInvalidArgs);

    if (dstAddr.IsMulticast())
    {
        request.SetType(coap::Type::kNonConfirmable);
    }

    SuccessOrExit(error = request.SetUriPath(uri::kMgmtPanidQuery));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kCommissionerSessionId, GetSessionId()}));
    SuccessOrExit(error = MakeChannelMask(channelMask, aChannelMask));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kChannelMask, channelMask}));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kPanId, aPanId}));

    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }

    error = Error::kNone;
    mProxyClient.SendRequest(request, onResponse, dstAddr, kDefaultMmPort);

exit:
    if (error != Error::kNone || request.IsNonConfirmable())
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
    Error         error = Error::kNone;
    Address       dstAddr;
    ByteArray     channelMask;
    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};

    auto onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        Error error = Error::kNone;

        SuccessOrExit(error = aError);
        VerifyOrExit(aResponse->GetCode() != coap::Code::kUnauthorized, error = Error::kSecurity);
        VerifyOrExit(aResponse->GetCode() == coap::Code::kChanged, error = Error::kBadFormat);

    exit:
        aHandler(error);
    };

    SuccessOrExit(error = dstAddr.Set(aDstAddr));
    VerifyOrExit(dstAddr.IsValid(), error = Error::kInvalidArgs);

    if (dstAddr.IsMulticast())
    {
        request.SetType(coap::Type::kNonConfirmable);
    }

    SuccessOrExit(error = request.SetUriPath(uri::kMgmtEdScan));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kCommissionerSessionId, GetSessionId()}));
    SuccessOrExit(error = MakeChannelMask(channelMask, aChannelMask));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kChannelMask, channelMask}));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kCount, aCount}));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kPeriod, aPeriod}));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kScanDuration, aScanDuration}));

    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }

    error = Error::kNone;
    mProxyClient.SendRequest(request, onResponse, dstAddr, kDefaultMmPort);

exit:
    if (error != Error::kNone || request.IsNonConfirmable())
    {
        aHandler(error);
    }
}

void CommissionerImpl::RequestToken(Handler<ByteArray> aHandler, const std::string &aAddr, uint16_t aPort)
{
    if (!IsCcmMode())
    {
        aHandler(nullptr, Error::kInvalidState);
    }
    else
    {
        mTokenManager.RequestToken(aHandler, aAddr, aPort);
    }
}

Error CommissionerImpl::SetToken(const ByteArray &aSignedToken, const ByteArray &aSignerCert)
{
    if (!IsCcmMode())
    {
        return Error::kInvalidState;
    }
    return mTokenManager.SetToken(aSignedToken, aSignerCert);
}

void CommissionerImpl::SetDatasetChangedHandler(ErrorHandler aHandler)
{
    mDatasetChangedHandler = aHandler;
}

void CommissionerImpl::SetPanIdConflictHandler(PanIdConflictHandler aHandler)
{
    mPanIdConflictHandler = aHandler;
}

void CommissionerImpl::SetEnergyReportHandler(EnergyReportHandler aHandler)
{
    mEnergyReportHandler = aHandler;
}

void CommissionerImpl::SendPetition(PetitionHandler aHandler)
{
    Error         error = Error::kNone;
    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};

    auto onResponse = [this, aHandler](const coap::Response *aResponse, Error aError) {
        Error       error = Error::kNone;
        tlv::TlvSet tlvSet;
        tlv::TlvPtr stateTlv          = nullptr;
        tlv::TlvPtr sessionIdTlv      = nullptr;
        tlv::TlvPtr commissionerIdTlv = nullptr;
        std::string existingCommissionerId;

        SuccessOrExit(error = aError);

        VerifyOrExit(aResponse->GetCode() == coap::Code::kChanged, error = Error::kFailed);

        SuccessOrExit(error = GetTlvSet(tlvSet, *aResponse));
        stateTlv = tlvSet[tlv::Type::kState];
        VerifyOrExit(stateTlv != nullptr, error = Error::kNotFound);
        VerifyOrExit(stateTlv->IsValid(), error = Error::kBadFormat);

        if (stateTlv->GetValueAsInt8() != tlv::kStateAccept)
        {
            commissionerIdTlv = tlvSet[tlv::Type::kCommissionerId];
            if (commissionerIdTlv != nullptr && commissionerIdTlv->IsValid())
            {
                existingCommissionerId = commissionerIdTlv->GetValueAsString();
            }
            ExitNow(error = Error::kReject);
        }

        sessionIdTlv = tlvSet[tlv::Type::kCommissionerSessionId];
        VerifyOrExit(sessionIdTlv != nullptr, error = Error::kNotFound);
        VerifyOrExit(sessionIdTlv->IsValid(), error = Error::kBadFormat);

        mSessionId = sessionIdTlv->GetValueAsUint16();

        error = Error::kNone;

        mState = State::kActive;
        mKeepAliveTimer.Start(GetKeepAliveInterval());

        LOG_INFO("petition succeed, start keep-alive timer with {} seconds", GetKeepAliveInterval().count() / 1000);

    exit:
        if (error != Error::kNone)
        {
            mState = State::kDisabled;
        }
        aHandler(existingCommissionerId.empty() ? nullptr : &existingCommissionerId, error);
    };

    VerifyOrExit(mState == State::kDisabled, error = Error::kInvalidState);

    SuccessOrExit(error = request.SetUriPath(uri::kPetitioning));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kCommissionerId, mConfig.mId}));

    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }

    mState = State::kPetitioning;

    mBrClient.SendRequest(request, onResponse);

    LOG_DEBUG("sent petition request");

exit:
    if (error != Error::kNone)
    {
        aHandler(nullptr, error);
    }
}

void CommissionerImpl::SendKeepAlive(Timer &, bool aKeepAlive)
{
    Error         error = Error::kNone;
    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};
    auto          state = (aKeepAlive ? tlv::kStateAccept : tlv::kStateReject);

    auto onResponse = [this](const coap::Response *aResponse, Error aError) {
        Error error = HandleStateResponse(aResponse, aError);

        if (error == Error::kNone)
        {
            mKeepAliveTimer.Start(GetKeepAliveInterval());
        }
        else
        {
            mState = State::kDisabled;
            Resign([](Error) {});

            // TODO(wgtdkp): notify user that we are rejected.
            LOG_WARN("keep alive message rejected: {}", ErrorToString(error));
        }
        return;
    };

    VerifyOrExit(IsActive(), error = Error::kInvalidState);

    SuccessOrExit(error = request.SetUriPath(uri::kKeepAlive));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kState, state}));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kCommissionerSessionId, GetSessionId()}));

    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }

    error = Error::kNone;

    mKeepAliveTimer.Start(GetKeepAliveInterval());

    mBrClient.SendRequest(request, onResponse);

    LOG_DEBUG("sent keep alive message: keepAlive={}", aKeepAlive);

exit:
    if (error != Error::kNone)
    {
        LOG_WARN("sending keep alive message failed: {}", ErrorToString(error));
    }
}

Error CommissionerImpl::SignRequest(coap::Request &aRequest, tlv::Scope aScope)
{
    Error     error = Error::kNone;
    ByteArray signature;

    VerifyOrExit(IsCcmMode(), error = Error::kInvalidState);

    SuccessOrExit(error = mTokenManager.SignMessage(signature, aRequest));

    SuccessOrExit(error = AppendTlv(aRequest, {tlv::Type::kCommissionerToken, mTokenManager.GetToken(), aScope}));
    SuccessOrExit(error = AppendTlv(aRequest, {tlv::Type::kCommissionerSignature, signature, aScope}));

exit:
    return error;
}

Error AppendTlv(coap::Message &aMessage, const tlv::Tlv &aTlv)
{
    Error     error;
    ByteArray buf;

    SuccessOrExit(error = aTlv.Serialize(buf));

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

Error CommissionerImpl::HandleStateResponse(const coap::Response *aResponse, Error aError)
{
    Error       error    = Error::kNone;
    tlv::TlvPtr stateTlv = nullptr;

    SuccessOrExit(error = aError);
    VerifyOrExit(aResponse->GetCode() != coap::Code::kUnauthorized, error = Error::kSecurity);
    VerifyOrExit(aResponse->GetCode() == coap::Code::kChanged, error = Error::kFailed);
    VerifyOrExit((stateTlv = GetTlv(tlv::Type::kState, *aResponse)) != nullptr, error = Error::kBadFormat);
    VerifyOrExit(stateTlv->IsValid(), error = Error::kBadFormat);
    VerifyOrExit(stateTlv->GetValueAsInt8() == tlv::kStateAccept, error = Error::kReject);

    error = Error::kNone;

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

Error CommissionerImpl::DecodeActiveOperationalDataset(ActiveOperationalDataset &aDataset,
                                                       const coap::Response &    aResponse)
{
    Error                    error = Error::kBadFormat;
    tlv::TlvSet              tlvSet;
    ActiveOperationalDataset dataset;

    // Clear all data fields
    dataset.mPresentFlags = 0;

    SuccessOrExit(GetTlvSet(tlvSet, aResponse));

    if (auto activeTimeStamp = tlvSet[tlv::Type::kActiveTimestamp])
    {
        uint64_t value;
        VerifyOrExit(activeTimeStamp->IsValid());
        value                    = utils::Decode<uint64_t>(activeTimeStamp->GetValue());
        dataset.mActiveTimestamp = Timestamp::Decode(value);
        dataset.mPresentFlags |= ActiveOperationalDataset::kActiveTimestampBit;
    }

    if (auto channel = tlvSet[tlv::Type::kChannel])
    {
        VerifyOrExit(channel->IsValid());
        dataset.mChannel.mPage   = channel->GetValue()[0];
        dataset.mChannel.mNumber = utils::Decode<uint16_t>(&channel->GetValue()[1]);
        dataset.mPresentFlags |= ActiveOperationalDataset::kChannelBit;
    }

    if (auto channelMask = tlvSet[tlv::Type::kChannelMask])
    {
        VerifyOrExit(channelMask->IsValid());
        SuccessOrExit(DecodeChannelMask(dataset.mChannelMask, channelMask->GetValue()));
        dataset.mPresentFlags |= ActiveOperationalDataset::kChannelMaskBit;
    }

    if (auto extendedPanId = tlvSet[tlv::Type::kExtendedPanId])
    {
        VerifyOrExit(extendedPanId->IsValid());
        dataset.mExtendedPanId = extendedPanId->GetValue();
        dataset.mPresentFlags |= ActiveOperationalDataset::kExtendedPanIdBit;
    }

    if (auto meshLocalPrefix = tlvSet[tlv::Type::kNetworkMeshLocalPrefix])
    {
        VerifyOrExit(meshLocalPrefix->IsValid());
        dataset.mMeshLocalPrefix = meshLocalPrefix->GetValue();
        dataset.mPresentFlags |= ActiveOperationalDataset::kMeshLocalPrefixBit;
    }

    if (auto networkMasterKey = tlvSet[tlv::Type::kNetworkMasterKey])
    {
        VerifyOrExit(networkMasterKey->IsValid());
        dataset.mNetworkMasterKey = networkMasterKey->GetValue();
        dataset.mPresentFlags |= ActiveOperationalDataset::kNetworkMasterKeyBit;
    }

    if (auto networkName = tlvSet[tlv::Type::kNetworkName])
    {
        VerifyOrExit(networkName->IsValid());
        dataset.mNetworkName = networkName->GetValueAsString();
        dataset.mPresentFlags |= ActiveOperationalDataset::kNetworkNameBit;
    }

    if (auto panId = tlvSet[tlv::Type::kPanId])
    {
        VerifyOrExit(panId->IsValid());
        dataset.mPanId = utils::Decode<uint16_t>(panId->GetValue());
        dataset.mPresentFlags |= ActiveOperationalDataset::kPanIdBit;
    }

    if (auto pskc = tlvSet[tlv::Type::kPSKc])
    {
        VerifyOrExit(pskc->IsValid());
        dataset.mPSKc = pskc->GetValue();
        dataset.mPresentFlags |= ActiveOperationalDataset::kPSKcBit;
    }

    if (auto securityPolicy = tlvSet[tlv::Type::kSecurityPolicy])
    {
        VerifyOrExit(securityPolicy->IsValid());
        auto &value                           = securityPolicy->GetValue();
        dataset.mSecurityPolicy.mRotationTime = utils::Decode<uint16_t>(value);
        dataset.mSecurityPolicy.mFlags        = {value.begin() + sizeof(uint16_t), value.end()};
        dataset.mPresentFlags |= ActiveOperationalDataset::kSecurityPolicyBit;
    }

    error    = Error::kNone;
    aDataset = dataset;

exit:
    if (error != Error::kNone)
    {
        aDataset.mPresentFlags = 0;
    }
    return error;
}

Error CommissionerImpl::DecodePendingOperationalDataset(PendingOperationalDataset &aDataset,
                                                        const coap::Response &     aResponse)
{
    Error                     error = Error::kBadFormat;
    tlv::TlvSet               tlvSet;
    PendingOperationalDataset dataset;

    // Clear all data fields
    dataset.mPresentFlags = 0;

    SuccessOrExit(DecodeActiveOperationalDataset(dataset, aResponse));
    SuccessOrExit(GetTlvSet(tlvSet, aResponse));

    if (auto delayTimer = tlvSet[tlv::Type::kDelayTimer])
    {
        VerifyOrExit(delayTimer->IsValid());
        dataset.mDelayTimer = utils::Decode<uint32_t>(delayTimer->GetValue());
        dataset.mPresentFlags |= PendingOperationalDataset::kDelayTimerBit;
    }

    if (auto pendingTimestamp = tlvSet[tlv::Type::kPendingTimestamp])
    {
        uint64_t value;
        VerifyOrExit(pendingTimestamp->IsValid());
        value                     = utils::Decode<uint64_t>(pendingTimestamp->GetValue());
        dataset.mPendingTimestamp = Timestamp::Decode(value);
        dataset.mPresentFlags |= PendingOperationalDataset::kPendingTimestampBit;
    }

    error    = Error::kNone;
    aDataset = dataset;

exit:
    if (error != Error::kNone)
    {
        aDataset.mPresentFlags = 0;
    }
    return error;
}

Error CommissionerImpl::DecodeChannelMask(ChannelMask &aChannelMask, const ByteArray &aBuf)
{
    Error       error = Error::kBadFormat;
    ChannelMask channelMask;
    size_t      offset = 0;
    size_t      length = aBuf.size();

    while (offset < length)
    {
        ChannelMaskEntry entry;
        uint8_t          entryLength;
        VerifyOrExit(offset + 2 <= length);

        entry.mPage = aBuf[offset++];
        entryLength = aBuf[offset++];

        VerifyOrExit(offset + entryLength <= length);
        entry.mMasks = {aBuf.begin() + offset, aBuf.begin() + offset + entryLength};
        channelMask.emplace_back(entry);

        offset += entryLength;
    }

    VerifyOrExit(offset == length);

    error        = Error::kNone;
    aChannelMask = channelMask;

exit:
    return error;
}

Error CommissionerImpl::EncodeActiveOperationalDataset(coap::Request &                 aRequest,
                                                       const ActiveOperationalDataset &aDataset)
{
    Error error = Error::kNone;

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

Error CommissionerImpl::EncodePendingOperationalDataset(coap::Request &                  aRequest,
                                                        const PendingOperationalDataset &aDataset)
{
    Error error = Error::kNotImplemented;

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
    Error error = Error::kNone;

    for (const auto &entry : aChannelMask)
    {
        VerifyOrExit(entry.mMasks.size() <= std::numeric_limits<uint8_t>::max(), error = Error::kInvalidArgs);
        utils::Encode(aBuf, entry.mPage);
        utils::Encode(aBuf, static_cast<uint8_t>(entry.mMasks.size()));
        aBuf.insert(aBuf.end(), entry.mMasks.begin(), entry.mMasks.end());
    }

exit:
    return error;
}

Error CommissionerImpl::DecodeBbrDataset(BbrDataset &aDataset, const coap::Response &aResponse)
{
    Error       error = Error::kBadFormat;
    tlv::TlvSet tlvSet;
    BbrDataset  dataset;

    SuccessOrExit(GetTlvSet(tlvSet, aResponse));

    if (auto triHostname = tlvSet[tlv::Type::kTriHostname])
    {
        VerifyOrExit(triHostname->IsValid());
        dataset.mTriHostname = triHostname->GetValueAsString();
        dataset.mPresentFlags |= BbrDataset::kTriHostnameBit;
    }

    if (auto registrarHostname = tlvSet[tlv::Type::kRegistrarHostname])
    {
        VerifyOrExit(registrarHostname->IsValid());
        dataset.mRegistrarHostname = registrarHostname->GetValueAsString();
        dataset.mPresentFlags |= BbrDataset::kRegistrarHostnameBit;
    }

    if (auto registrarIpv6Addr = tlvSet[tlv::Type::kRegistrarIpv6Address])
    {
        Address addr;
        VerifyOrExit(registrarIpv6Addr->IsValid());
        SuccessOrExit(addr.Set(registrarIpv6Addr->GetValue()));
        SuccessOrExit(addr.ToString(dataset.mRegistrarIpv6Addr));
        dataset.mPresentFlags |= BbrDataset::kRegistrarIpv6AddrBit;
    }

    error    = Error::kNone;
    aDataset = dataset;

exit:
    return error;
}

Error CommissionerImpl::EncodeBbrDataset(coap::Request &aRequest, const BbrDataset &aDataset)
{
    Error error = Error::kNone;

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

Error CommissionerImpl::DecodeCommissionerDataset(CommissionerDataset &aDataset, const coap::Response &aResponse)
{
    Error               error = Error::kBadFormat;
    tlv::TlvSet         tlvSet;
    CommissionerDataset dataset;

    SuccessOrExit(GetTlvSet(tlvSet, aResponse));

    if (auto sessionId = tlvSet[tlv::Type::kCommissionerSessionId])
    {
        VerifyOrExit(sessionId->IsValid());
        dataset.mSessionId = sessionId->GetValueAsUint16();
        dataset.mPresentFlags |= CommissionerDataset::kSessionIdBit;
    }

    if (auto borderAgentLocator = tlvSet[tlv::Type::kBorderAgentLocator])
    {
        VerifyOrExit(borderAgentLocator->IsValid());
        dataset.mBorderAgentLocator = borderAgentLocator->GetValueAsUint16();
        dataset.mPresentFlags |= CommissionerDataset::kBorderAgentLocatorBit;
    }

    if (auto steeringData = tlvSet[tlv::Type::kSteeringData])
    {
        VerifyOrExit(steeringData->IsValid());
        dataset.mSteeringData = steeringData->GetValue();
        dataset.mPresentFlags |= CommissionerDataset::kSteeringDataBit;
    }

    if (auto aeSteeringData = tlvSet[tlv::Type::kAeSteeringData])
    {
        VerifyOrExit(aeSteeringData->IsValid());
        dataset.mAeSteeringData = aeSteeringData->GetValue();
        dataset.mPresentFlags |= CommissionerDataset::kAeSteeringDataBit;
    }

    if (auto nmkpSteeringData = tlvSet[tlv::Type::kNmkpSteeringData])
    {
        VerifyOrExit(nmkpSteeringData->IsValid());
        dataset.mNmkpSteeringData = nmkpSteeringData->GetValue();
        dataset.mPresentFlags |= CommissionerDataset::kNmkpSteeringDataBit;
    }

    if (auto joinerUdpPort = tlvSet[tlv::Type::kJoinerUdpPort])
    {
        VerifyOrExit(joinerUdpPort->IsValid());
        dataset.mJoinerUdpPort = joinerUdpPort->GetValueAsUint16();
        dataset.mPresentFlags |= CommissionerDataset::kJoinerUdpPortBit;
    }

    if (auto aeUdpPort = tlvSet[tlv::Type::kAeUdpPort])
    {
        VerifyOrExit(aeUdpPort->IsValid());
        dataset.mAeUdpPort = aeUdpPort->GetValueAsUint16();
        dataset.mPresentFlags |= CommissionerDataset::kAeUdpPortBit;
    }

    if (auto nmkpUdpPort = tlvSet[tlv::Type::kNmkpUdpPort])
    {
        VerifyOrExit(nmkpUdpPort->IsValid());
        dataset.mNmkpUdpPort = nmkpUdpPort->GetValueAsUint16();
        dataset.mPresentFlags |= CommissionerDataset::kNmkpUdpPortBit;
    }

    error    = Error::kNone;
    aDataset = dataset;

exit:
    return error;
}

Error CommissionerImpl::EncodeCommissionerDataset(coap::Request &aRequest, const CommissionerDataset &aDataset)
{
    Error error = Error::kNone;

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

void CommissionerImpl::SendProxyMessage(ErrorHandler aHandler, const std::string &aDstAddr, const std::string &aUriPath)
{
    Error         error = Error::kNone;
    Address       dstAddr;
    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};

    auto onResponse = [aHandler](const coap::Response *aResponse, Error aError) {
        Error       error    = Error::kNone;
        tlv::TlvPtr stateTlv = nullptr;

        SuccessOrExit(error = aError);
        VerifyOrExit(aResponse->GetCode() != coap::Code::kUnauthorized, error = Error::kSecurity);
        VerifyOrExit(aResponse->GetCode() == coap::Code::kChanged, error = Error::kFailed);

        stateTlv = GetTlv(tlv::Type::kState, *aResponse);
        if (stateTlv != nullptr)
        {
            VerifyOrExit(stateTlv->IsValid(), error = Error::kBadFormat);
            VerifyOrExit(stateTlv->GetValueAsInt8() == tlv::kStateAccept, error = Error::kReject);
        }

        error = Error::kNone;

    exit:
        aHandler(error);
    };

    VerifyOrExit(IsCcmMode(), error = Error::kInvalidState);

    SuccessOrExit(error = dstAddr.Set(aDstAddr));
    VerifyOrExit(dstAddr.IsValid(), error = Error::kInvalidArgs);

    SuccessOrExit(error = request.SetUriPath(aUriPath));
    SuccessOrExit(error = AppendTlv(request, {tlv::Type::kCommissionerSessionId, GetSessionId()}));

    if (IsCcmMode())
    {
        SuccessOrExit(error = SignRequest(request));
    }

    error = Error::kNone;
    mProxyClient.SendRequest(request, onResponse, dstAddr, kDefaultMmPort);

exit:
    if (error != Error::kNone)
    {
        aHandler(error);
    }
}

void CommissionerImpl::HandleDatasetChanged(const coap::Request &aRequest)
{
    std::string peerAddr = "unknown address";

    IgnoreError(aRequest.GetEndpoint()->GetPeerAddr().ToString(peerAddr));

    LOG_INFO("received MGMT_DATASET_CHANGED.ntf from {}", peerAddr);

    mProxyClient.SendEmptyChanged(aRequest);

    if (mDatasetChangedHandler != nullptr)
    {
        mDatasetChangedHandler(Error::kNone);
    }
}

void CommissionerImpl::HandlePanIdConflict(const coap::Request &aRequest)
{
    Error       error = Error::kNone;
    tlv::TlvSet tlvSet;
    ChannelMask channelMask;
    uint16_t    panId;
    std::string peerAddr = "unknown address";

    IgnoreError(aRequest.GetEndpoint()->GetPeerAddr().ToString(peerAddr));

    LOG_INFO("received MGMT_PANID_CONFLICT.ans from {}", peerAddr);

    mProxyClient.SendEmptyChanged(aRequest);

    SuccessOrExit(error = GetTlvSet(tlvSet, aRequest));
    if (auto channelMaskTlv = tlvSet[tlv::Type::kChannelMask])
    {
        SuccessOrExit(error = DecodeChannelMask(channelMask, channelMaskTlv->GetValue()));
    }
    if (auto panIdTlv = tlvSet[tlv::Type::kPanId])
    {
        panId = panIdTlv->GetValueAsUint16();
    }

    error = Error::kNone;
    if (mPanIdConflictHandler != nullptr)
    {
        mPanIdConflictHandler(&peerAddr, &channelMask, &panId, Error::kNone);
    }

exit:
    if (error != Error::kNone)
    {
        LOG_WARN("handle MGMT_PANID_CONFLICT.ans from {} failed: {}", peerAddr, ErrorToString(error));

        if (mPanIdConflictHandler != nullptr)
        {
            mPanIdConflictHandler(nullptr, nullptr, nullptr, error);
        }
    }
}

void CommissionerImpl::HandleEnergyReport(const coap::Request &aRequest)
{
    Error       error = Error::kNone;
    tlv::TlvSet tlvSet;
    ChannelMask channelMask;
    ByteArray   energyList;
    std::string peerAddr = "unknown address";

    IgnoreError(aRequest.GetEndpoint()->GetPeerAddr().ToString(peerAddr));

    LOG_INFO("received MGMT_ED_REPORT.ans from {}", peerAddr);

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

    error = Error::kNone;
    if (mEnergyReportHandler != nullptr)
    {
        mEnergyReportHandler(&peerAddr, &channelMask, &energyList, Error::kNone);
    }

exit:
    if (error != Error::kNone)
    {
        LOG_WARN("handle MGMT_ED_REPORT.ans from {} failed: {}", peerAddr, ErrorToString(error));

        if (mEnergyReportHandler != nullptr)
        {
            mEnergyReportHandler(nullptr, nullptr, nullptr, error);
        }
    }
}

Error CommissionerImpl::MakeChannelMask(ByteArray &aBuf, uint32_t aChannelMask)
{
    Error            error = Error::kNone;
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

    VerifyOrExit(!entry.mMasks.empty(), error = Error::kInvalidArgs);
    error = EncodeChannelMask(aBuf, {entry});
    ASSERT(error == Error::kNone);

exit:
    return error;
}

void CommissionerImpl::HandleRlyRx(const coap::Request &aRlyRx)
{
    Error       error = Error::kNone;
    tlv::TlvSet tlvSet;

    tlv::TlvPtr tlv;

    const JoinerInfo *joinerInfo = nullptr;
    uint16_t          joinerUdpPort;
    uint16_t          joinerRouterLocator;
    ByteArray         joinerIid;
    ByteArray         dtlsRecords;

    SuccessOrExit(error = GetTlvSet(tlvSet, aRlyRx));

    VerifyOrExit((tlv = tlvSet[tlv::Type::kJoinerUdpPort]) != nullptr, error = Error::kNotFound);
    VerifyOrExit(tlv->IsValid(), error = Error::kBadFormat);
    joinerUdpPort = tlv->GetValueAsUint16();

    VerifyOrExit((tlv = tlvSet[tlv::Type::kJoinerRouterLocator]) != nullptr, error = Error::kNotFound);
    VerifyOrExit(tlv->IsValid(), error = Error::kBadFormat);
    joinerRouterLocator = tlv->GetValueAsUint16();

    VerifyOrExit((tlv = tlvSet[tlv::Type::kJoinerIID]) != nullptr, error = Error::kNotFound);
    VerifyOrExit(tlv->IsValid(), error = Error::kBadFormat);
    joinerIid = tlv->GetValue();

    VerifyOrExit((tlv = tlvSet[tlv::Type::kJoinerDtlsEncapsulation]) != nullptr, error = Error::kNotFound);
    VerifyOrExit(tlv->IsValid(), error = Error::kBadFormat);
    dtlsRecords = tlv->GetValue();

    LOG_DEBUG("received RLY_RX.ntf: joinerIID={}, joinerRouterLocator={}, length={}", utils::Hex(joinerIid),
              joinerRouterLocator, dtlsRecords.size());

    if (mJoinerInfoRequester == nullptr)
    {
        LOG_WARN("joiner info requester is nil, give up");
        ExitNow();
    }
    else
    {
        auto joinerId = joinerIid;
        joinerId[0] ^= kLocalExternalAddrMask;
        joinerInfo = mJoinerInfoRequester(JoinerType::kMeshCoP, joinerId);
        if (joinerInfo == nullptr)
        {
            LOG_INFO("joiner(IID={}) is disabled", utils::Hex(joinerIid));
            ExitNow();
        }
    }

    {
        auto it = mCommissioningSessions.find(joinerIid);
        if (it != mCommissioningSessions.end() && it->second.Disabled())
        {
            mCommissioningSessions.erase(it);
            it = mCommissioningSessions.end();
        }

        if (it == mCommissioningSessions.end())
        {
            it = mCommissioningSessions
                     .emplace(std::piecewise_construct, std::forward_as_tuple(joinerIid),
                              std::forward_as_tuple(*this, *joinerInfo, joinerUdpPort, joinerRouterLocator, joinerIid,
                                                    aRlyRx.GetEndpoint()->GetPeerAddr(),
                                                    aRlyRx.GetEndpoint()->GetPeerPort()))
                     .first;
            auto &session = it->second;

            std::string peerAddr = "unknown address";
            IgnoreError(session.GetPeerAddr().ToString(peerAddr));

            LOG_DEBUG("received a new joiner(IID={}) DTLS connection from [{}]:{}", utils::Hex(session.GetJoinerIid()),
                      peerAddr, session.GetPeerPort());

            auto onConnected = [peerAddr](CommissioningSession &aSession, Error aError) {
                if (aError != Error::kNone)
                {
                    LOG_ERROR("a joiner(IID={}) DTLS connection from [{}]:{} failed: {}",
                              utils::Hex(aSession.GetJoinerIid()), peerAddr, aSession.GetPeerPort(),
                              ErrorToString(aError));
                }
                else
                {
                    LOG_INFO("a joiner(IID={}) DTLS connection from [{}]:{} succeed",
                             utils::Hex(aSession.GetJoinerIid()), peerAddr, aSession.GetPeerPort());
                }
            };

            if ((error = session.Start(onConnected)) != Error::kNone)
            {
                mCommissioningSessions.erase(it);
                it = mCommissioningSessions.end();
                ExitNow();
            }
            else
            {
                mCommissioningSessionTimer.Start(session.GetExpirationTime());
            }
        }

        ASSERT(it != mCommissioningSessions.end());
        auto &session = it->second;
        SuccessOrExit(error = session.RecvJoinerDtlsRecords(dtlsRecords));
    }

exit:
    if (error != Error::kNone)
    {
        LOG_ERROR("failed to handle RLY_RX.ntf message: {}", ErrorToString(error));
    }
}

void CommissionerImpl::HandleCommissioningSessionTimer(Timer &aTimer)
{
    TimePoint nextShot;
    bool      hasNextShot = false;

    auto now = Clock::now();
    auto it  = mCommissioningSessions.begin();
    while (it != mCommissioningSessions.end())
    {
        auto &session = it->second;

        if (now >= session.GetExpirationTime())
        {
            it = mCommissioningSessions.erase(it);

            LOG_INFO("commissioning session (joiner IID={}) removed", utils::Hex(session.GetJoinerIid()));
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

} // namespace commissioner

} // namespace ot
