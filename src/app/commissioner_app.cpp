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
 *   The file implements commissioner application.
 */

#include "app/commissioner_app.hpp"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "app/file_util.hpp"
#include "app/json.hpp"
#include "commissioner/commissioner.hpp"
#include "commissioner/defines.hpp"
#include "commissioner/error.hpp"
#include "commissioner/network_data.hpp"
#include "commissioner/network_diag_data.hpp"
#include "common/address.hpp"
#include "common/error_macros.hpp"
#include "common/utils.hpp"

namespace ot {
namespace commissioner {

JoinerInfo::JoinerInfo(JoinerType aType, uint64_t aEui64, const std::string &aPSKd, const std::string &aProvisioningUrl)
    : mType(aType)
    , mEui64(aEui64)
    , mPSKd(aPSKd)
    , mProvisioningUrl(aProvisioningUrl)
{
}

Error CommissionerAppCreate(std::shared_ptr<CommissionerApp> &aCommApp, const Config &aConfig)
{
    return CommissionerApp::Create(aCommApp, aConfig);
}

Error CommissionerApp::Create(std::shared_ptr<CommissionerApp> &aCommApp, const Config &aConfig)
{
    Error error;
    auto  app = std::shared_ptr<CommissionerApp>(new CommissionerApp());

    SuccessOrExit(error = app->Init(aConfig));
    if (aConfig.mCommissionerToken.size() > 0)
    {
        app->SetToken(aConfig.mCommissionerToken).IgnoreError();
    }

    aCommApp = app;

exit:
    return error;
}

Error CommissionerApp::Init(const Config &aConfig)
{
    Error error;

    mCommissioner = Commissioner::Create(*this);
    VerifyOrExit(mCommissioner != nullptr, error = ERROR_OUT_OF_MEMORY("Commissioner::Create"));
    SuccessOrExit(error = mCommissioner->Init(aConfig));

    mCommDataset = MakeDefaultCommissionerDataset();

exit:
    return error;
}

Error CommissionerApp::Connect(const std::string &aBorderAgentAddr, uint16_t aBorderAgentPort)
{
    Error error;

    SuccessOrExit(error = mCommissioner->Connect(aBorderAgentAddr, aBorderAgentPort));

exit:
    if (error != ErrorCode::kNone)
    {
        Stop();
    }
    return error;
}

Error CommissionerApp::Start(std::string       &aExistingCommissionerId,
                             const std::string &aBorderAgentAddr,
                             uint16_t           aBorderAgentPort)
{
    Error error;

    // We need to report the already active commissioner ID if one exists.
    SuccessOrExit(error = mCommissioner->Petition(aExistingCommissionerId, aBorderAgentAddr, aBorderAgentPort));
    SuccessOrExit(error = SyncNetworkData());

exit:
    if (error != ErrorCode::kNone && IsActive())
    {
        Stop();
    }
    return error;
}

void CommissionerApp::Stop()
{
    IgnoreError(mCommissioner->Resign());

    mJoiners.clear();
    mPanIdConflicts.clear();
    mEnergyReports.clear();
    mActiveDataset  = ActiveOperationalDataset();
    mPendingDataset = PendingOperationalDataset();
    mCommDataset    = MakeDefaultCommissionerDataset();
    mBbrDataset     = BbrDataset();
}

void CommissionerApp::CancelRequests()
{
    mCommissioner->CancelRequests();
}

bool CommissionerApp::IsActive() const
{
    return mCommissioner->IsActive();
}

State CommissionerApp::GetState() const
{
    return mCommissioner->GetState();
}

bool CommissionerApp::IsCcmMode() const
{
    return mCommissioner->IsCcmMode();
}

Error CommissionerApp::SaveNetworkData(const std::string &aFilename)
{
    Error           error;
    JsonNetworkData jsonNetworkData;

    jsonNetworkData.mActiveDataset  = mActiveDataset;
    jsonNetworkData.mPendingDataset = mPendingDataset;
    jsonNetworkData.mCommDataset    = mCommDataset;
    jsonNetworkData.mBbrDataset     = mBbrDataset;
    auto jsonString                 = NetworkDataToJson(jsonNetworkData);

    SuccessOrExit(error = WriteFile(jsonString, aFilename));

exit:
    return error;
}

Error CommissionerApp::SyncNetworkData(void)
{
    Error                     error;
    ActiveOperationalDataset  activeDataset;
    PendingOperationalDataset pendingDataset;
    BbrDataset                bbrDataset;
    CommissionerDataset       commDataset;

    SuccessOrExit(error = mCommissioner->GetActiveDataset(activeDataset, 0xFFFF));
    SuccessOrExit(error = mCommissioner->GetPendingDataset(pendingDataset, 0xFFFF));
    SuccessOrExit(error = mCommissioner->SetCommissionerDataset(mCommDataset));
    SuccessOrExit(error = mCommissioner->GetCommissionerDataset(commDataset, 0xFFFF));
    MergeDataset(mCommDataset, commDataset);

    if (IsCcmMode())
    {
        SuccessOrExit(error = mCommissioner->GetBbrDataset(bbrDataset, 0xFFFF));
        mBbrDataset = bbrDataset;
    }
    mActiveDataset  = activeDataset;
    mPendingDataset = pendingDataset;

exit:
    return error;
}

Error CommissionerApp::GetSessionId(uint16_t &aSessionId) const
{
    Error error;

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    aSessionId = mCommissioner->GetSessionId();

exit:
    return error;
}

Error CommissionerApp::GetBorderAgentLocator(uint16_t &aLocator) const
{
    Error error;

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    // We must have Border Agent Locator in commissioner dataset.
    VerifyOrDie(mCommDataset.mPresentFlags & CommissionerDataset::kBorderAgentLocatorBit);

    aLocator = mCommDataset.mBorderAgentLocator;

exit:
    return error;
}

Error CommissionerApp::GetSteeringData(ByteArray &aSteeringData, JoinerType aJoinerType) const
{
    Error error;

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    switch (aJoinerType)
    {
    case JoinerType::kMeshCoP:
        VerifyOrExit((mCommDataset.mPresentFlags & CommissionerDataset::kSteeringDataBit),
                     error = ERROR_NOT_FOUND("cannot find Thread 1.1 joiner Steering Data"));
        aSteeringData = mCommDataset.mSteeringData;
        break;

    case JoinerType::kAE:
        VerifyOrExit((mCommDataset.mPresentFlags & CommissionerDataset::kAeSteeringDataBit),
                     error = ERROR_NOT_FOUND("cannot find Thread CCM AE Steering Data"));
        aSteeringData = mCommDataset.mAeSteeringData;
        break;

    case JoinerType::kNMKP:
        VerifyOrExit((mCommDataset.mPresentFlags & CommissionerDataset::kNmkpSteeringDataBit),
                     error = ERROR_NOT_FOUND("cannot find CCM NMKP Steering Data"));
        aSteeringData = mCommDataset.mNmkpSteeringData;
        break;
    }

exit:
    return error;
}

Error CommissionerApp::EnableJoiner(JoinerType         aType,
                                    uint64_t           aEui64,
                                    const std::string &aPSKd,
                                    const std::string &aProvisioningUrl)
{
    Error error;
    auto  joinerId    = Commissioner::ComputeJoinerId(aEui64);
    auto  commDataset = mCommDataset;
    commDataset.mPresentFlags &= ~CommissionerDataset::kSessionIdBit;
    commDataset.mPresentFlags &= ~CommissionerDataset::kBorderAgentLocatorBit;
    auto &steeringData = GetSteeringData(commDataset, aType);

    if (aType == JoinerType::kMeshCoP)
    {
        SuccessOrExit(error = ValidatePSKd(aPSKd));
    }

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    VerifyOrExit(mJoiners.count({aType, joinerId}) == 0,
                 error = ERROR_ALREADY_EXISTS("joiner(type={}, EUI64={:X}) has already been enabled",
                                              utils::to_underlying(aType), aEui64));

    Commissioner::AddJoiner(steeringData, joinerId);
    SuccessOrExit(error = mCommissioner->SetCommissionerDataset(commDataset));

    MergeDataset(mCommDataset, commDataset);
    mJoiners.emplace(JoinerKey{aType, joinerId}, JoinerInfo{aType, aEui64, aPSKd, aProvisioningUrl});

exit:
    return error;
}

Error CommissionerApp::DisableJoiner(JoinerType aType, uint64_t aEui64)
{
    Error     error;
    ByteArray joinerId;
    auto      commDataset = mCommDataset;
    commDataset.mPresentFlags &= ~CommissionerDataset::kSessionIdBit;
    commDataset.mPresentFlags &= ~CommissionerDataset::kBorderAgentLocatorBit;
    auto &steeringData = GetSteeringData(commDataset, aType);

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    steeringData = {0x00};
    for (const auto &kv : mJoiners)
    {
        auto &joiner = kv.second;
        if (joiner.mType == aType && joiner.mEui64 == aEui64)
        {
            continue;
        }
        joinerId = Commissioner::ComputeJoinerId(aEui64);
        Commissioner::AddJoiner(steeringData, joinerId);
    }

    SuccessOrExit(error = mCommissioner->SetCommissionerDataset(commDataset));

    MergeDataset(mCommDataset, commDataset);
    mJoiners.erase(JoinerKey{aType, joinerId});

exit:
    return error;
}

Error CommissionerApp::EnableAllJoiners(JoinerType aType, const std::string &aPSKd, const std::string &aProvisioningUrl)
{
    Error     error;
    ByteArray joinerId;
    auto      commDataset = mCommDataset;
    commDataset.mPresentFlags &= ~CommissionerDataset::kSessionIdBit;
    commDataset.mPresentFlags &= ~CommissionerDataset::kBorderAgentLocatorBit;
    auto &steeringData = GetSteeringData(commDataset, aType);

    if (aType == JoinerType::kMeshCoP)
    {
        SuccessOrExit(error = ValidatePSKd(aPSKd));
    }
    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    // Set steering data to all 1 to enable all joiners.
    steeringData = {0xFF};
    SuccessOrExit(error = mCommissioner->SetCommissionerDataset(commDataset));

    MergeDataset(mCommDataset, commDataset);

    EraseAllJoiners(aType);
    joinerId = Commissioner::ComputeJoinerId(0);
    mJoiners.emplace(JoinerKey{aType, joinerId}, JoinerInfo{aType, 0, aPSKd, aProvisioningUrl});

exit:
    return error;
}

Error CommissionerApp::DisableAllJoiners(JoinerType aType)
{
    Error error;
    auto  commDataset = mCommDataset;
    commDataset.mPresentFlags &= ~CommissionerDataset::kSessionIdBit;
    commDataset.mPresentFlags &= ~CommissionerDataset::kBorderAgentLocatorBit;
    auto &steeringData = GetSteeringData(commDataset, aType);

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    // Set steering data to all 0 to disable all joiners.
    steeringData = {0x00};
    SuccessOrExit(error = mCommissioner->SetCommissionerDataset(commDataset));

    MergeDataset(mCommDataset, commDataset);
    EraseAllJoiners(aType);

exit:
    return error;
}

Error CommissionerApp::GetJoinerUdpPort(uint16_t &aJoinerUdpPort, JoinerType aJoinerType) const
{
    Error error;

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    switch (aJoinerType)
    {
    case JoinerType::kMeshCoP:
        VerifyOrExit(mCommDataset.mPresentFlags & CommissionerDataset::kJoinerUdpPortBit,
                     error = ERROR_NOT_FOUND("cannot find Thread 1.1 Joiner UDP Port"));
        aJoinerUdpPort = mCommDataset.mJoinerUdpPort;
        break;

    case JoinerType::kAE:
        VerifyOrExit(mCommDataset.mPresentFlags & CommissionerDataset::kAeUdpPortBit,
                     error = ERROR_NOT_FOUND("cannot find Thread CCM AE UDP Port"));
        aJoinerUdpPort = mCommDataset.mAeUdpPort;
        break;

    case JoinerType::kNMKP:
        VerifyOrExit(mCommDataset.mPresentFlags & CommissionerDataset::kNmkpUdpPortBit,
                     error = ERROR_NOT_FOUND("cannot find Thread CCM NMKP Port"));
        aJoinerUdpPort = mCommDataset.mNmkpUdpPort;
        break;
    }

exit:
    return error;
}

Error CommissionerApp::SetJoinerUdpPort(JoinerType aType, uint16_t aUdpPort)
{
    Error error;
    auto  commDataset = mCommDataset;
    commDataset.mPresentFlags &= ~CommissionerDataset::kSessionIdBit;
    commDataset.mPresentFlags &= ~CommissionerDataset::kBorderAgentLocatorBit;
    auto &joinerUdpPort = GetJoinerUdpPort(commDataset, aType);

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    joinerUdpPort = aUdpPort;
    SuccessOrExit(error = mCommissioner->SetCommissionerDataset(commDataset));

    MergeDataset(mCommDataset, commDataset);

exit:
    return error;
}

Error CommissionerApp::GetCommissionerDataset(CommissionerDataset &aDataset, uint16_t aDatasetFlags)
{
    return mCommissioner->GetCommissionerDataset(aDataset, aDatasetFlags);

    // Don't merge requested commissioner dataset, because the commissioner is
    // the source of commissioner dataset.
}

Error CommissionerApp::SetCommissionerDataset(const CommissionerDataset &aDataset)
{
    Error error;

    SuccessOrExit(error = mCommissioner->SetCommissionerDataset(aDataset));
    MergeDataset(mCommDataset, aDataset);

exit:
    return error;
}

Error CommissionerApp::GetActiveTimestamp(Timestamp &aTimestamp) const
{
    Error error;

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    VerifyOrDie(mActiveDataset.mPresentFlags & ActiveOperationalDataset::kActiveTimestampBit);
    aTimestamp = mActiveDataset.mActiveTimestamp;

exit:
    return error;
}

Error CommissionerApp::GetChannel(Channel &aChannel)
{
    Error error;

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    // Since channel will be updated by pending operational after a delay time,
    // we need to pull the active operational dataset.

    // TODO(wgtdkp): should we send MGMT_ACTIVE_GET.req for all GetXXX APIs ?
    SuccessOrExit(error = mCommissioner->GetActiveDataset(mActiveDataset, 0xFFFF));

    VerifyOrDie(mActiveDataset.mPresentFlags & ActiveOperationalDataset::kChannelBit);

    aChannel = mActiveDataset.mChannel;

exit:
    return error;
}

Error CommissionerApp::SetChannel(const Channel &aChannel, MilliSeconds aDelay)
{
    Error                     error;
    PendingOperationalDataset pendingDataset;

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    pendingDataset.mChannel = aChannel;
    pendingDataset.mPresentFlags |= PendingOperationalDataset::kChannelBit;

    pendingDataset.mDelayTimer = aDelay.count();
    pendingDataset.mPresentFlags |= PendingOperationalDataset::kDelayTimerBit;

    SuccessOrExit(error = mCommissioner->SetPendingDataset(pendingDataset));

    MergeDataset(mPendingDataset, pendingDataset);

exit:
    return error;
}

Error CommissionerApp::GetChannelMask(ChannelMask &aChannelMask) const
{
    Error error;

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    VerifyOrExit(mActiveDataset.mPresentFlags & ActiveOperationalDataset::kChannelMaskBit,
                 error = ERROR_NOT_FOUND("cannot find valid Channel Masks in Active Operational Dataset"));
    aChannelMask = mActiveDataset.mChannelMask;

exit:
    return error;
}

Error CommissionerApp::SetChannelMask(const ChannelMask &aChannelMask)
{
    Error                    error;
    ActiveOperationalDataset activeDataset;

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    activeDataset.mChannelMask = aChannelMask;
    activeDataset.mPresentFlags |= ActiveOperationalDataset::kChannelMaskBit;

    SuccessOrExit(error = mCommissioner->SetActiveDataset(activeDataset));

    MergeDataset(mActiveDataset, activeDataset);

exit:
    return error;
}

Error CommissionerApp::GetExtendedPanId(ByteArray &aExtendedPanId) const
{
    Error       error;
    std::string extendedPanIdString;

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    VerifyOrExit(mActiveDataset.mPresentFlags & ActiveOperationalDataset::kExtendedPanIdBit,
                 error = ERROR_NOT_FOUND("cannot find valid Extended PAN ID in Active Operational Dataset"));
    aExtendedPanId = mActiveDataset.mExtendedPanId;

exit:
    return error;
}

Error CommissionerApp::SetExtendedPanId(const ByteArray &aExtendedPanId)
{
    Error                    error;
    ActiveOperationalDataset activeDataset;

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    activeDataset.mExtendedPanId = aExtendedPanId;
    activeDataset.mPresentFlags |= ActiveOperationalDataset::kExtendedPanIdBit;

    SuccessOrExit(error = mCommissioner->SetActiveDataset(activeDataset));

    MergeDataset(mActiveDataset, activeDataset);

exit:
    return error;
}

Error CommissionerApp::GetMeshLocalPrefix(std::string &aPrefix)
{
    Error error;

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    SuccessOrExit(error = mCommissioner->GetActiveDataset(mActiveDataset, 0xFFFF));

    VerifyOrExit(mActiveDataset.mPresentFlags & ActiveOperationalDataset::kMeshLocalPrefixBit,
                 error = ERROR_NOT_FOUND("cannot find valid Mesh-Local Prefix in Active Operational Dataset"));
    aPrefix = Ipv6PrefixToString(mActiveDataset.mMeshLocalPrefix);

exit:
    return error;
}

Error CommissionerApp::SetMeshLocalPrefix(const std::string &aPrefix, MilliSeconds aDelay)
{
    Error                     error;
    PendingOperationalDataset pendingDataset;

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    SuccessOrExit(error = Ipv6PrefixFromString(pendingDataset.mMeshLocalPrefix, aPrefix));
    pendingDataset.mPresentFlags |= PendingOperationalDataset::kMeshLocalPrefixBit;

    pendingDataset.mDelayTimer = aDelay.count();
    pendingDataset.mPresentFlags |= PendingOperationalDataset::kDelayTimerBit;

    SuccessOrExit(error = mCommissioner->SetPendingDataset(pendingDataset));

    MergeDataset(mPendingDataset, pendingDataset);

exit:
    return error;
}

Error CommissionerApp::GetMeshLocalAddr(std::string       &aMeshLocalAddr,
                                        const std::string &aMeshLocalPrefix,
                                        uint16_t           aLocator16)
{
    static const size_t kThreadMeshLocalPrefixLength = 8;
    Error               error;
    ByteArray           rawAddr;
    Address             addr;

    SuccessOrExit(error = Ipv6PrefixFromString(rawAddr, aMeshLocalPrefix));
    VerifyOrExit(rawAddr.size() == kThreadMeshLocalPrefixLength,
                 error = ERROR_INVALID_ARGS("Thread Mesh local prefix length={} != {}", rawAddr.size(),
                                            kThreadMeshLocalPrefixLength));

    utils::Encode<uint16_t>(rawAddr, 0x0000);
    utils::Encode<uint16_t>(rawAddr, 0x00FF);
    utils::Encode<uint16_t>(rawAddr, 0xFE00);
    utils::Encode<uint16_t>(rawAddr, aLocator16);

    SuccessOrExit(addr.Set(rawAddr));
    aMeshLocalAddr = addr.ToString();

exit:
    return error;
}

Error CommissionerApp::GetNetworkMasterKey(ByteArray &aMasterKey)
{
    Error error;

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));
    ;

    SuccessOrExit(error = mCommissioner->GetActiveDataset(mActiveDataset, 0xFFFF));

    VerifyOrExit(mActiveDataset.mPresentFlags & ActiveOperationalDataset::kNetworkMasterKeyBit,
                 error = ERROR_NOT_FOUND("cannot find valid Network Master Key in Active Operational Dataset"));
    aMasterKey = mActiveDataset.mNetworkMasterKey;

exit:
    return error;
}

Error CommissionerApp::SetNetworkMasterKey(const ByteArray &aMasterKey, MilliSeconds aDelay)
{
    Error                     error;
    PendingOperationalDataset pendingDataset;

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    pendingDataset.mNetworkMasterKey = aMasterKey;
    pendingDataset.mPresentFlags |= PendingOperationalDataset::kNetworkMasterKeyBit;

    pendingDataset.mDelayTimer = aDelay.count();
    pendingDataset.mPresentFlags |= PendingOperationalDataset::kDelayTimerBit;

    SuccessOrExit(error = mCommissioner->SetPendingDataset(pendingDataset));

    MergeDataset(mPendingDataset, pendingDataset);

exit:
    return error;
}

Error CommissionerApp::GetNetworkName(std::string &aNetworkName) const
{
    Error error;

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    VerifyOrExit(mActiveDataset.mPresentFlags & ActiveOperationalDataset::kNetworkNameBit,
                 error = ERROR_NOT_FOUND("cannot find valid Network Name in Active Operational Dataset"));
    aNetworkName = mActiveDataset.mNetworkName;

exit:
    return error;
}

Error CommissionerApp::SetNetworkName(const std::string &aNetworkName)
{
    Error                    error;
    ActiveOperationalDataset activeDataset;

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    activeDataset.mNetworkName = aNetworkName;
    activeDataset.mPresentFlags |= ActiveOperationalDataset::kNetworkNameBit;

    SuccessOrExit(error = mCommissioner->SetActiveDataset(activeDataset));

    MergeDataset(mActiveDataset, activeDataset);

exit:
    return error;
}

Error CommissionerApp::GetPanId(uint16_t &aPanId)
{
    Error error;

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    SuccessOrExit(error = mCommissioner->GetActiveDataset(mActiveDataset, 0xFFFF));

    VerifyOrExit(mActiveDataset.mPresentFlags & ActiveOperationalDataset::kPanIdBit,
                 error = ERROR_NOT_FOUND("cannot find valid PAN ID in Active Operational Dataset"));
    aPanId = mActiveDataset.mPanId;

exit:
    return error;
}

Error CommissionerApp::SetPanId(uint16_t aPanId, MilliSeconds aDelay)
{
    Error                     error;
    PendingOperationalDataset pendingDataset;

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    pendingDataset.mPanId = aPanId;
    pendingDataset.mPresentFlags |= PendingOperationalDataset::kPanIdBit;

    pendingDataset.mDelayTimer = aDelay.count();
    pendingDataset.mPresentFlags |= PendingOperationalDataset::kDelayTimerBit;

    SuccessOrExit(error = mCommissioner->SetPendingDataset(pendingDataset));

    MergeDataset(mPendingDataset, pendingDataset);

exit:
    return error;
}

Error CommissionerApp::GetPSKc(ByteArray &aPSKc) const
{
    Error error;

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    VerifyOrExit(mActiveDataset.mPresentFlags & ActiveOperationalDataset::kPSKcBit,
                 error = ERROR_NOT_FOUND("cannot find valid PSKc in Active Operational Dataset"));
    aPSKc = mActiveDataset.mPSKc;

exit:
    return error;
}

Error CommissionerApp::SetPSKc(const ByteArray &aPSKc)
{
    Error                    error;
    ActiveOperationalDataset activeDataset;

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    activeDataset.mPSKc = aPSKc;
    activeDataset.mPresentFlags |= ActiveOperationalDataset::kPSKcBit;

    SuccessOrExit(error = mCommissioner->SetActiveDataset(activeDataset));

    MergeDataset(mActiveDataset, activeDataset);

exit:
    return error;
}

Error CommissionerApp::GetSecurityPolicy(SecurityPolicy &aSecurityPolicy) const
{
    Error error;

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    VerifyOrExit(mActiveDataset.mPresentFlags & ActiveOperationalDataset::kSecurityPolicyBit,
                 error = ERROR_NOT_FOUND("cannot find valid Security Policy in Active Operational Dataset"));
    aSecurityPolicy = mActiveDataset.mSecurityPolicy;

exit:
    return error;
}

Error CommissionerApp::SetSecurityPolicy(const SecurityPolicy &aSecurityPolicy)
{
    Error                    error;
    ActiveOperationalDataset activeDataset;

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    activeDataset.mSecurityPolicy = aSecurityPolicy;
    activeDataset.mPresentFlags |= ActiveOperationalDataset::kSecurityPolicyBit;

    SuccessOrExit(error = mCommissioner->SetActiveDataset(activeDataset));

    MergeDataset(mActiveDataset, activeDataset);

exit:
    return error;
}

Error CommissionerApp::GetActiveDataset(ActiveOperationalDataset &aDataset, uint16_t aDatasetFlags)
{
    Error error;

    SuccessOrExit(error = mCommissioner->GetActiveDataset(aDataset, aDatasetFlags));
    MergeDataset(mActiveDataset, aDataset);

exit:
    return error;
}

Error CommissionerApp::SetActiveDataset(const ActiveOperationalDataset &aDataset)
{
    Error error;

    SuccessOrExit(error = mCommissioner->SetActiveDataset(aDataset));
    MergeDataset(mActiveDataset, aDataset);

exit:
    return error;
}

Error CommissionerApp::GetPendingDataset(PendingOperationalDataset &aDataset, uint16_t aDatasetFlags)
{
    Error error;

    SuccessOrExit(error = mCommissioner->GetPendingDataset(aDataset, aDatasetFlags));
    MergeDataset(mPendingDataset, aDataset);

exit:
    return error;
}

Error CommissionerApp::SetPendingDataset(const PendingOperationalDataset &aDataset)
{
    Error error;

    SuccessOrExit(error = mCommissioner->SetPendingDataset(aDataset));
    MergeDataset(mPendingDataset, aDataset);

exit:
    return error;
}

Error CommissionerApp::GetTriHostname(std::string &aHostname) const
{
    Error error;

    VerifyOrExit(mBbrDataset.mPresentFlags & BbrDataset::kTriHostnameBit,
                 error = ERROR_NOT_FOUND("cannot find valid TRI Hostname in BBR Dataset"));
    aHostname = mBbrDataset.mTriHostname;

exit:
    return error;
}

Error CommissionerApp::SetTriHostname(const std::string &aHostname)
{
    Error      error;
    BbrDataset bbrDataset;

    bbrDataset.mTriHostname = aHostname;
    bbrDataset.mPresentFlags |= BbrDataset::kTriHostnameBit;

    SuccessOrExit(error = mCommissioner->SetBbrDataset(bbrDataset));

    MergeDataset(mBbrDataset, bbrDataset);

exit:
    return error;
}

Error CommissionerApp::GetRegistrarHostname(std::string &aHostname) const
{
    Error error;

    VerifyOrExit(mBbrDataset.mPresentFlags & BbrDataset::kRegistrarHostnameBit,
                 error = ERROR_NOT_FOUND("cannot find valid Registrar Hostname in BBR Dataset"));
    aHostname = mBbrDataset.mRegistrarHostname;

exit:
    return error;
}

Error CommissionerApp::SetRegistrarHostname(const std::string &aHostname)
{
    Error      error;
    BbrDataset bbrDataset;
    BbrDataset bbrDatasetWithRegAddr;

    bbrDataset.mRegistrarHostname = aHostname;
    bbrDataset.mPresentFlags |= BbrDataset::kRegistrarHostnameBit;

    SuccessOrExit(error = mCommissioner->SetBbrDataset(bbrDataset));
    SuccessOrExit(error = mCommissioner->GetBbrDataset(bbrDatasetWithRegAddr, BbrDataset::kRegistrarIpv6AddrBit));
    VerifyOrExit(bbrDatasetWithRegAddr.mPresentFlags & BbrDataset::kRegistrarIpv6AddrBit,
                 error = ERROR_BAD_FORMAT("cannot resolve the Registrar Hostname: {}", aHostname));

    MergeDataset(mBbrDataset, bbrDataset);
    MergeDataset(mBbrDataset, bbrDatasetWithRegAddr);

exit:
    return error;
}

Error CommissionerApp::GetRegistrarIpv6Addr(std::string &aIpv6Addr) const
{
    Error error;

    VerifyOrExit(mBbrDataset.mPresentFlags & BbrDataset::kRegistrarIpv6AddrBit,
                 error = ERROR_NOT_FOUND("cannot find valid Registrar IPv6 Address in BBR Dataset"));
    aIpv6Addr = mBbrDataset.mRegistrarIpv6Addr;

exit:
    return error;
}

Error CommissionerApp::GetBbrDataset(BbrDataset &aDataset, uint16_t aDatasetFlags)
{
    Error error;

    SuccessOrExit(error = mCommissioner->GetBbrDataset(aDataset, aDatasetFlags));
    MergeDataset(mBbrDataset, aDataset);

exit:
    return error;
}

Error CommissionerApp::SetBbrDataset(const BbrDataset &aDataset)
{
    Error error;

    SuccessOrExit(error = mCommissioner->SetBbrDataset(aDataset));
    MergeDataset(mBbrDataset, aDataset);

exit:
    return error;
}

Error CommissionerApp::Reenroll(const std::string &aDstAddr)
{
    return mCommissioner->CommandReenroll(aDstAddr);
}

Error CommissionerApp::DomainReset(const std::string &aDstAddr)
{
    return mCommissioner->CommandDomainReset(aDstAddr);
}

Error CommissionerApp::Migrate(const std::string &aDstAddr, const std::string &aDesignatedNetwork)
{
    return mCommissioner->CommandMigrate(aDstAddr, aDesignatedNetwork);
}

Error CommissionerApp::RegisterMulticastListener(const std::vector<std::string> &aMulticastAddrList, Seconds aTimeout)
{
    Error   error;
    uint8_t status;

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    SuccessOrExit(error = mCommissioner->RegisterMulticastListener(status, aMulticastAddrList, aTimeout.count()));
    VerifyOrExit(status == kMlrStatusSuccess,
                 error = ERROR_REJECTED("request was rejected with statusCode={}", status));

exit:
    return error;
}

Error CommissionerApp::AnnounceBegin(uint32_t           aChannelMask,
                                     uint8_t            aCount,
                                     MilliSeconds       aPeriod,
                                     const std::string &aDtsAddr)
{
    Error error;

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    SuccessOrExit(error = mCommissioner->AnnounceBegin(aChannelMask, aCount, aPeriod.count(), aDtsAddr));

exit:
    return error;
}

Error CommissionerApp::PanIdQuery(uint32_t aChannelMask, uint16_t aPanId, const std::string &aDstAddr)
{
    Error error;

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    SuccessOrExit(error = mCommissioner->PanIdQuery(aChannelMask, aPanId, aDstAddr));

exit:
    return error;
}

bool CommissionerApp::HasPanIdConflict(uint16_t aPanId) const
{
    return mPanIdConflicts.count(aPanId) != 0;
}

Error CommissionerApp::EnergyScan(uint32_t           aChannelMask,
                                  uint8_t            aCount,
                                  uint16_t           aPeriod,
                                  uint16_t           aScanDuration,
                                  const std::string &aDstAddr)
{
    Error error;

    VerifyOrExit(IsActive(), error = ERROR_INVALID_STATE("the commissioner is not active"));

    SuccessOrExit(error = mCommissioner->EnergyScan(aChannelMask, aCount, aPeriod, aScanDuration, aDstAddr));

exit:
    return error;
}

const EnergyReport *CommissionerApp::GetEnergyReport(const Address &aDstAddr) const
{
    auto report = mEnergyReports.find(aDstAddr);
    if (report == mEnergyReports.end())
    {
        return nullptr;
    }
    return &report->second;
}

const EnergyReportMap &CommissionerApp::GetAllEnergyReports() const
{
    return mEnergyReports;
}

const std::string &CommissionerApp::GetDomainName() const
{
    return mCommissioner->GetDomainName();
}

const ByteArray &CommissionerApp::GetToken() const
{
    return mSignedToken;
}

Error CommissionerApp::RequestToken(const std::string &aAddr, uint16_t aPort)
{
    return mCommissioner->RequestToken(mSignedToken, aAddr, aPort);
}

Error CommissionerApp::SetToken(const ByteArray &aSignedToken)
{
    Error error;

    SuccessOrExit(error = mCommissioner->SetToken(aSignedToken));

    mSignedToken = aSignedToken;

exit:
    return error;
}

bool CommissionerApp::JoinerKey::operator<(const JoinerKey &aOther) const
{
    return mType < aOther.mType || (mType == aOther.mType && mId < aOther.mId);
}

CommissionerDataset CommissionerApp::MakeDefaultCommissionerDataset() const
{
    CommissionerDataset dataset;

    dataset.mJoinerUdpPort = kDefaultJoinerUdpPort;
    dataset.mPresentFlags |= CommissionerDataset::kJoinerUdpPortBit;

    if (IsCcmMode())
    {
        dataset.mAeUdpPort = kDefaultAeUdpPort;
        dataset.mPresentFlags |= CommissionerDataset::kAeUdpPortBit;
        dataset.mNmkpUdpPort = kDefaultNmkpUdpPort;
        dataset.mPresentFlags |= CommissionerDataset::kNmkpUdpPortBit;
    }

    return dataset;
}

ByteArray &CommissionerApp::GetSteeringData(CommissionerDataset &aDataset, JoinerType aJoinerType)
{
    switch (aJoinerType)
    {
    case JoinerType::kMeshCoP:
        aDataset.mPresentFlags |= CommissionerDataset::kSteeringDataBit;
        return aDataset.mSteeringData;

    case JoinerType::kAE:
        aDataset.mPresentFlags |= CommissionerDataset::kAeSteeringDataBit;
        return aDataset.mAeSteeringData;

    case JoinerType::kNMKP:
        aDataset.mPresentFlags |= CommissionerDataset::kNmkpSteeringDataBit;
        return aDataset.mNmkpSteeringData;

    default:
        VerifyOrDie(false);
        aDataset.mPresentFlags |= CommissionerDataset::kSteeringDataBit;
        return aDataset.mSteeringData;
    }
}

uint16_t &CommissionerApp::GetJoinerUdpPort(CommissionerDataset &aDataset, JoinerType aJoinerType)
{
    switch (aJoinerType)
    {
    case JoinerType::kMeshCoP:
        aDataset.mPresentFlags |= CommissionerDataset::kJoinerUdpPortBit;
        return aDataset.mJoinerUdpPort;

    case JoinerType::kAE:
        aDataset.mPresentFlags |= CommissionerDataset::kAeUdpPortBit;
        return aDataset.mAeUdpPort;

    case JoinerType::kNMKP:
        aDataset.mPresentFlags |= CommissionerDataset::kNmkpUdpPortBit;
        return aDataset.mNmkpUdpPort;

    default:
        VerifyOrDie(false);
        aDataset.mPresentFlags |= CommissionerDataset::kJoinerUdpPortBit;
        return aDataset.mJoinerUdpPort;
    }
}

size_t CommissionerApp::EraseAllJoiners(JoinerType aJoinerType)
{
    size_t count  = 0;
    auto   joiner = mJoiners.begin();
    while (joiner != mJoiners.end())
    {
        if (joiner->first.mType == aJoinerType)
        {
            ++count;
            joiner = mJoiners.erase(joiner);
        }
        else
        {
            ++joiner;
        }
    }
    return count;
}

void CommissionerApp::MergeDataset(ActiveOperationalDataset &aDst, const ActiveOperationalDataset &aSrc)
{
#define SET_IF_PRESENT(name)                                          \
    if (aSrc.mPresentFlags & ActiveOperationalDataset::k##name##Bit)  \
    {                                                                 \
        aDst.m##name = aSrc.m##name;                                  \
        aDst.mPresentFlags |= ActiveOperationalDataset::k##name##Bit; \
    }

    SET_IF_PRESENT(ActiveTimestamp);
    SET_IF_PRESENT(Channel);
    SET_IF_PRESENT(ChannelMask);
    SET_IF_PRESENT(ExtendedPanId);
    SET_IF_PRESENT(MeshLocalPrefix);
    SET_IF_PRESENT(NetworkMasterKey);
    SET_IF_PRESENT(NetworkName);
    SET_IF_PRESENT(PanId);
    SET_IF_PRESENT(PSKc);
    SET_IF_PRESENT(SecurityPolicy);

#undef SET_IF_PRESENT
}

void CommissionerApp::MergeDataset(PendingOperationalDataset &aDst, const PendingOperationalDataset &aSrc)
{
    MergeDataset(static_cast<ActiveOperationalDataset &>(aDst), static_cast<const ActiveOperationalDataset &>(aSrc));

#define SET_IF_PRESENT(name)                                           \
    if (aSrc.mPresentFlags & PendingOperationalDataset::k##name##Bit)  \
    {                                                                  \
        aDst.m##name = aSrc.m##name;                                   \
        aDst.mPresentFlags |= PendingOperationalDataset::k##name##Bit; \
    }

    SET_IF_PRESENT(PendingTimestamp);
    SET_IF_PRESENT(DelayTimer);

#undef SET_IF_PRESENT
}

void CommissionerApp::MergeDataset(BbrDataset &aDst, const BbrDataset &aSrc)
{
#define SET_IF_PRESENT(name)                            \
    if (aSrc.mPresentFlags & BbrDataset::k##name##Bit)  \
    {                                                   \
        aDst.m##name = aSrc.m##name;                    \
        aDst.mPresentFlags |= BbrDataset::k##name##Bit; \
    }

    SET_IF_PRESENT(TriHostname);
    SET_IF_PRESENT(RegistrarHostname);
    SET_IF_PRESENT(RegistrarIpv6Addr);

#undef SET_IF_PRESENT
}

// Remove dst dataset's steering data and joiner UDP port
// if they are not presented in the src dataset.
void CommissionerApp::MergeDataset(CommissionerDataset &aDst, const CommissionerDataset &aSrc)
{
#define SET_IF_PRESENT(name)                                     \
    if (aSrc.mPresentFlags & CommissionerDataset::k##name##Bit)  \
    {                                                            \
        aDst.m##name = aSrc.m##name;                             \
        aDst.mPresentFlags |= CommissionerDataset::k##name##Bit; \
    }

    SET_IF_PRESENT(BorderAgentLocator);
    SET_IF_PRESENT(SessionId);

#undef SET_IF_PRESENT
#define SET_IF_PRESENT(name)                                      \
    if (aSrc.mPresentFlags & CommissionerDataset::k##name##Bit)   \
    {                                                             \
        aDst.m##name = aSrc.m##name;                              \
        aDst.mPresentFlags |= CommissionerDataset::k##name##Bit;  \
    }                                                             \
    else                                                          \
    {                                                             \
        aDst.mPresentFlags &= ~CommissionerDataset::k##name##Bit; \
    }

    SET_IF_PRESENT(SteeringData);
    SET_IF_PRESENT(AeSteeringData);
    SET_IF_PRESENT(NmkpSteeringData);
    SET_IF_PRESENT(JoinerUdpPort);
    SET_IF_PRESENT(AeUdpPort);
    SET_IF_PRESENT(NmkpUdpPort);

#undef SET_IF_PRESENT
}

std::string CommissionerApp::OnJoinerRequest(const ByteArray &aJoinerId)
{
    std::string pskd;

    auto joinerInfo = mJoiners.find({JoinerType::kMeshCoP, aJoinerId});
    if (joinerInfo != mJoiners.end())
    {
        ExitNow(pskd = joinerInfo->second.mPSKd);
    }

    // Check if all joiners has been enabled.
    joinerInfo = mJoiners.find({JoinerType::kMeshCoP, Commissioner::ComputeJoinerId(0)});
    if (joinerInfo != mJoiners.end())
    {
        ExitNow(pskd = joinerInfo->second.mPSKd);
    }

exit:
    return pskd;
}

void CommissionerApp::OnJoinerConnected(const ByteArray &aJoinerId, Error aError)
{
    (void)aJoinerId;
    (void)aError;

    // TODO(wgtdkp): logging
}

bool CommissionerApp::OnJoinerFinalize(const ByteArray   &aJoinerId,
                                       const std::string &aVendorName,
                                       const std::string &aVendorModel,
                                       const std::string &aVendorSwVersion,
                                       const ByteArray   &aVendorStackVersion,
                                       const std::string &aProvisioningUrl,
                                       const ByteArray   &aVendorData)
{
    (void)aVendorName;
    (void)aVendorModel;
    (void)aVendorSwVersion;
    (void)aVendorStackVersion;
    (void)aVendorData;

    bool accepted = false;

    auto configuredJoinerInfo = GetJoinerInfo(JoinerType::kMeshCoP, aJoinerId);

    // TODO(deimi): logging
    VerifyOrExit(configuredJoinerInfo != nullptr, accepted = false);
    VerifyOrExit(aProvisioningUrl == configuredJoinerInfo->mProvisioningUrl, accepted = false);

    accepted = true;

exit:
    return accepted;
}

void CommissionerApp::OnKeepAliveResponse(Error aError)
{
    (void)aError;

    // Dummy handler.
}

void CommissionerApp::OnPanIdConflict(const std::string &aPeerAddr, const ChannelMask &aChannelMask, uint16_t aPanId)
{
    (void)aPeerAddr;

    // FIXME(wgtdkp): synchronization
    mPanIdConflicts[aPanId] = aChannelMask;
}

void CommissionerApp::OnEnergyReport(const std::string &aPeerAddr,
                                     const ChannelMask &aChannelMask,
                                     const ByteArray   &aEnergyList)
{
    Address addr;

    SuccessOrDie(addr.Set(aPeerAddr));

    // FIXME(wgtdkp): synchronization
    mEnergyReports[addr] = {aChannelMask, aEnergyList};
}

void CommissionerApp::OnDatasetChanged()
{
    mCommissioner->GetActiveDataset(
        [this](const ActiveOperationalDataset *aDataset, Error aError) {
            if (aError == ErrorCode::kNone)
            {
                // FIXME(wgtdkp): synchronization
                mActiveDataset = *aDataset;
            }
            else
            {
                // TODO(wgtdkp): logging
            }
        },
        0xFFFF);

    mCommissioner->GetPendingDataset(
        [this](const PendingOperationalDataset *aDataset, Error aError) {
            if (aError == ErrorCode::kNone)
            {
                // FIXME(wgtdkp): synchronization
                mPendingDataset = *aDataset;
            }
            else
            {
                // TODO(wgtdkp): logging
            }
        },
        0xFFFF);
}

Error CommissionerApp::ValidatePSKd(const std::string &aPSKd)
{
    Error error;

    VerifyOrExit(aPSKd.size() >= kMinJoinerDeviceCredentialLength && aPSKd.size() <= kMaxJoinerDeviceCredentialLength,
                 error = ERROR_INVALID_ARGS("PSKd length(={}) exceeds range [{}, {}]", aPSKd.size(),
                                            kMinJoinerDeviceCredentialLength, kMaxJoinerDeviceCredentialLength));

    for (auto c : aPSKd)
    {
        VerifyOrExit(isdigit(c) || isupper(c),
                     error = ERROR_INVALID_ARGS("PSKd includes non-digit and non-uppercase characters: {}", c));
        VerifyOrExit(c != 'I' && c != 'O' && c != 'Q' && c != 'Z',
                     error = ERROR_INVALID_ARGS("PSKd includes invalid uppercase characters: {}", c));
    }

exit:
    return error;
}

const JoinerInfo *CommissionerApp::GetJoinerInfo(JoinerType aType, const ByteArray &aJoinerId)
{
    auto joinerInfo = mJoiners.find({aType, aJoinerId});
    if (joinerInfo != mJoiners.end())
    {
        return &joinerInfo->second;
    }
    joinerInfo = mJoiners.find({aType, Commissioner::ComputeJoinerId(0)});
    if (joinerInfo != mJoiners.end())
    {
        return &joinerInfo->second;
    }
    return nullptr;
}

// Network Diagnostic
Error CommissionerApp::CommandDiagGetQuery(const std::string &aAddr, uint64_t aDiagDataFlags)
{
    Error error;

    error = mCommissioner->CommandDiagGetQuery(aAddr, aDiagDataFlags);
    return error;
}

Error CommissionerApp::CommandDiagGetQuery(uint16_t aPeerAloc16, uint64_t aDiagDataFlags)
{
    Error error;

    error = mCommissioner->CommandDiagGetQuery(aPeerAloc16, aDiagDataFlags);
    return error;
}

void CommissionerApp::OnDiagGetAnswerMessage(const std::string &aPeerAddr, const NetDiagData &aDiagAnsMsg)
{
    Address addr;

    SuccessOrDie(addr.Set(aPeerAddr));

    mDiagAnsDataMap[addr] = aDiagAnsMsg;
}

const DiagAnsDataMap &CommissionerApp::GetNetDiagTlvs() const
{
    return mDiagAnsDataMap;
}

Error CommissionerApp::CommandDiagReset(const std::string &aAddr, uint64_t aDiagDataFlags)
{
    Error error;

    error = mCommissioner->CommandDiagReset(aAddr, aDiagDataFlags);
    return error;
}

} // namespace commissioner

} // namespace ot
