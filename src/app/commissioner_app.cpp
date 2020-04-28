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
 *   The file implements commissioner application.
 */

#include "app/commissioner_app.hpp"

#include <algorithm>

#include "app/file_util.hpp"
#include "app/json.hpp"
#include "common/address.hpp"
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

std::shared_ptr<CommissionerApp> CommissionerApp::Create(const Config &aConfig)
{
    Error error = Error::kNone;
    auto  app   = std::shared_ptr<CommissionerApp>(new CommissionerApp());

    SuccessOrExit(error = app->Init(aConfig));

exit:
    return error == Error::kNone ? app : nullptr;
}

Error CommissionerApp::Init(const Config &aConfig)
{
    Error error;

    SuccessOrExit(error = Commissioner::Create(mCommissioner, *this, aConfig));
    SuccessOrExit(error = mCommissioner->Start());

    mCommDataset = MakeDefaultCommissionerDataset();

exit:
    return error;
}

Error CommissionerApp::Start(std::string &      aExistingCommissionerId,
                             const std::string &aBorderAgentAddr,
                             uint16_t           aBorderAgentPort)
{
    Error error = Error::kNone;

    // We need to report the already active commissioner ID if one exists.
    SuccessOrExit(error = mCommissioner->Petition(aExistingCommissionerId, aBorderAgentAddr, aBorderAgentPort));
    SuccessOrExit(error = SyncNetworkData());

exit:
    if (error != Error::kNone)
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

void CommissionerApp::AbortRequests()
{
    mCommissioner->AbortRequests();
}

bool CommissionerApp::IsActive() const
{
    return mCommissioner->IsActive();
}

bool CommissionerApp::IsCcmMode() const
{
    return mCommissioner->IsCcmMode();
}

Error CommissionerApp::SaveNetworkData(const std::string &aFilename)
{
    Error       error = Error::kNone;
    NetworkData networkData;

    networkData.mActiveDataset  = mActiveDataset;
    networkData.mPendingDataset = mPendingDataset;
    networkData.mCommDataset    = mCommDataset;
    networkData.mBbrDataset     = mBbrDataset;
    auto jsonString             = NetworkDataToJson(networkData);

    SuccessOrExit(error = WriteFile(jsonString, aFilename));

exit:
    return error;
}

Error CommissionerApp::SyncNetworkData(void)
{
    Error                     error = Error::kNone;
    ActiveOperationalDataset  activeDataset;
    PendingOperationalDataset pendingDataset;
    BbrDataset                bbrDataset;

    SuccessOrExit(error = mCommissioner->SetCommissionerDataset(mCommDataset));
    if (IsCcmMode())
    {
        SuccessOrExit(error = mCommissioner->GetBbrDataset(bbrDataset, 0xFFFF));
    }

    SuccessOrExit(error = mCommissioner->GetActiveDataset(activeDataset, 0xFFFF));
    SuccessOrExit(error = mCommissioner->GetPendingDataset(pendingDataset, 0xFFFF));

    if (IsCcmMode())
    {
        mBbrDataset = bbrDataset;
    }
    mActiveDataset  = activeDataset;
    mPendingDataset = pendingDataset;

exit:
    return error;
}

Error CommissionerApp::GetSessionId(uint16_t &aSessionId) const
{
    Error error = Error::kNone;

    VerifyOrExit(IsActive(), error = Error::kInvalidState);

    aSessionId = mCommissioner->GetSessionId();

exit:
    return error;
}

Error CommissionerApp::GetBorderAgentLocator(uint16_t &aLocator) const
{
    Error error = Error::kNone;

    VerifyOrExit(IsActive(), error = Error::kInvalidState);

    // We must have Border Agent Locator in commissioner dataset.
    VerifyOrDie(mCommDataset.mPresentFlags & CommissionerDataset::kBorderAgentLocatorBit);

    aLocator = mCommDataset.mBorderAgentLocator;

exit:
    return error;
}

Error CommissionerApp::GetSteeringData(ByteArray &aSteeringData, JoinerType aJoinerType) const
{
    Error error = Error::kNone;

    VerifyOrExit(IsActive(), error = Error::kInvalidState);

    switch (aJoinerType)
    {
    case JoinerType::kMeshCoP:
        VerifyOrExit((mCommDataset.mPresentFlags & CommissionerDataset::kSteeringDataBit), error = Error::kNotFound);
        aSteeringData = mCommDataset.mSteeringData;
        break;

    case JoinerType::kAE:
        VerifyOrExit((mCommDataset.mPresentFlags & CommissionerDataset::kAeSteeringDataBit), error = Error::kNotFound);
        aSteeringData = mCommDataset.mAeSteeringData;
        break;

    case JoinerType::kNMKP:
        VerifyOrExit((mCommDataset.mPresentFlags & CommissionerDataset::kNmkpSteeringDataBit),
                     error = Error::kNotFound);
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
    Error error       = Error::kNone;
    auto  joinerId    = Commissioner::ComputeJoinerId(aEui64);
    auto  commDataset = mCommDataset;
    commDataset.mPresentFlags &= ~CommissionerDataset::kSessionIdBit;
    commDataset.mPresentFlags &= ~CommissionerDataset::kBorderAgentLocatorBit;
    auto &steeringData = GetSteeringData(commDataset, aType);

    SuccessOrExit(error = ValidatePSKd(aPSKd));

    VerifyOrExit(IsActive(), error = Error::kInvalidState);

    VerifyOrExit(mJoiners.count({aType, joinerId}) == 0, error = Error::kAlready);

    Commissioner::AddJoiner(steeringData, joinerId);
    SuccessOrExit(error = mCommissioner->SetCommissionerDataset(commDataset));

    error = Error::kNone;
    MergeDataset(mCommDataset, commDataset);
    mJoiners.emplace(JoinerKey{aType, joinerId}, JoinerInfo{aType, aEui64, aPSKd, aProvisioningUrl});

exit:
    return error;
}

Error CommissionerApp::DisableJoiner(JoinerType aType, uint64_t aEui64)
{
    Error     error = Error::kNone;
    ByteArray joinerId;
    auto      commDataset = mCommDataset;
    commDataset.mPresentFlags &= ~CommissionerDataset::kSessionIdBit;
    commDataset.mPresentFlags &= ~CommissionerDataset::kBorderAgentLocatorBit;
    auto &steeringData = GetSteeringData(commDataset, aType);

    VerifyOrExit(IsActive(), error = Error::kInvalidState);

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

    error = Error::kNone;
    MergeDataset(mCommDataset, commDataset);
    mJoiners.erase(JoinerKey{aType, joinerId});

exit:
    return error;
}

Error CommissionerApp::EnableAllJoiners(JoinerType aType, const std::string &aPSKd, const std::string &aProvisioningUrl)
{
    Error     error = Error::kNone;
    ByteArray joinerId;
    auto      commDataset = mCommDataset;
    commDataset.mPresentFlags &= ~CommissionerDataset::kSessionIdBit;
    commDataset.mPresentFlags &= ~CommissionerDataset::kBorderAgentLocatorBit;
    auto &steeringData = GetSteeringData(commDataset, aType);

    SuccessOrExit(error = ValidatePSKd(aPSKd));
    VerifyOrExit(IsActive(), error = Error::kInvalidState);

    // Set steering data to all 1 to enable all joiners.
    steeringData = {0xFF};
    SuccessOrExit(error = mCommissioner->SetCommissionerDataset(commDataset));

    error = Error::kNone;
    MergeDataset(mCommDataset, commDataset);

    EraseAllJoiners(aType);
    joinerId = Commissioner::ComputeJoinerId(0);
    mJoiners.emplace(JoinerKey{aType, joinerId}, JoinerInfo{aType, 0, aPSKd, aProvisioningUrl});

exit:
    return error;
}

Error CommissionerApp::DisableAllJoiners(JoinerType aType)
{
    Error error       = Error::kNone;
    auto  commDataset = mCommDataset;
    commDataset.mPresentFlags &= ~CommissionerDataset::kSessionIdBit;
    commDataset.mPresentFlags &= ~CommissionerDataset::kBorderAgentLocatorBit;
    auto &steeringData = GetSteeringData(commDataset, aType);

    VerifyOrExit(IsActive(), error = Error::kInvalidState);

    // Set steering data to all 0 to disable all joiners.
    steeringData = {0x00};
    SuccessOrExit(error = mCommissioner->SetCommissionerDataset(commDataset));

    error = Error::kNone;
    MergeDataset(mCommDataset, commDataset);
    EraseAllJoiners(aType);

exit:
    return error;
}

Error CommissionerApp::GetJoinerUdpPort(uint16_t &aJoinerUdpPort, JoinerType aJoinerType) const
{
    Error error = Error::kNone;

    VerifyOrExit(IsActive(), error = Error::kInvalidState);

    switch (aJoinerType)
    {
    case JoinerType::kMeshCoP:
        VerifyOrExit(mCommDataset.mPresentFlags & CommissionerDataset::kJoinerUdpPortBit, error = Error::kNotFound);
        aJoinerUdpPort = mCommDataset.mJoinerUdpPort;
        break;

    case JoinerType::kAE:
        VerifyOrExit(mCommDataset.mPresentFlags & CommissionerDataset::kAeUdpPortBit, error = Error::kNotFound);
        aJoinerUdpPort = mCommDataset.mAeUdpPort;
        break;

    case JoinerType::kNMKP:
        VerifyOrExit(mCommDataset.mPresentFlags & CommissionerDataset::kNmkpUdpPortBit, error = Error::kNotFound);
        aJoinerUdpPort = mCommDataset.mNmkpUdpPort;
        break;
    }

exit:
    return error;
}

Error CommissionerApp::SetJoinerUdpPort(JoinerType aType, uint16_t aUdpPort)
{
    Error error       = Error::kNone;
    auto  commDataset = mCommDataset;
    commDataset.mPresentFlags &= ~CommissionerDataset::kSessionIdBit;
    commDataset.mPresentFlags &= ~CommissionerDataset::kBorderAgentLocatorBit;
    auto &joinerUdpPort = GetJoinerUdpPort(commDataset, aType);

    VerifyOrExit(IsActive(), error = Error::kInvalidState);

    joinerUdpPort = aUdpPort;
    SuccessOrExit(error = mCommissioner->SetCommissionerDataset(commDataset));

    error = Error::kNone;
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
    Error error = Error::kNone;

    VerifyOrExit(IsActive(), error = Error::kInvalidState);

    VerifyOrDie(mActiveDataset.mPresentFlags & ActiveOperationalDataset::kActiveTimestampBit);
    aTimestamp = mActiveDataset.mActiveTimestamp;

exit:
    return error;
}

Error CommissionerApp::GetChannel(Channel &aChannel)
{
    Error error = Error::kNone;

    VerifyOrExit(IsActive(), error = Error::kInvalidState);

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
    Error                     error = Error::kNone;
    PendingOperationalDataset pendingDataset;

    VerifyOrExit(IsActive(), error = Error::kInvalidState);

    pendingDataset.mChannel = aChannel;
    pendingDataset.mPresentFlags |= PendingOperationalDataset::kChannelBit;

    pendingDataset.mDelayTimer = aDelay.count();
    pendingDataset.mPresentFlags |= PendingOperationalDataset::kDelayTimerBit;

    SuccessOrExit(error = mCommissioner->SetPendingDataset(pendingDataset));

    error = Error::kNone;
    MergeDataset(mPendingDataset, pendingDataset);

exit:
    return error;
}

Error CommissionerApp::GetChannelMask(ChannelMask &aChannelMask) const
{
    Error error = Error::kNone;

    VerifyOrExit(IsActive(), error = Error::kInvalidState);

    VerifyOrExit(mActiveDataset.mPresentFlags & ActiveOperationalDataset::kChannelMaskBit, error = Error::kNotFound);
    aChannelMask = mActiveDataset.mChannelMask;

exit:
    return error;
}

Error CommissionerApp::SetChannelMask(const ChannelMask &aChannelMask)
{
    Error                    error = Error::kNone;
    ActiveOperationalDataset activeDataset;

    VerifyOrExit(IsActive(), error = Error::kInvalidState);

    activeDataset.mChannelMask = aChannelMask;
    activeDataset.mPresentFlags |= ActiveOperationalDataset::kChannelMaskBit;

    SuccessOrExit(error = mCommissioner->SetActiveDataset(activeDataset));

    error = Error::kNone;
    MergeDataset(mActiveDataset, activeDataset);

exit:
    return error;
}

Error CommissionerApp::GetExtendedPanId(ByteArray &aExtendedPanId) const
{
    Error error = Error::kNone;

    VerifyOrExit(IsActive(), error = Error::kInvalidState);

    VerifyOrExit(mActiveDataset.mPresentFlags & ActiveOperationalDataset::kExtendedPanIdBit, error = Error::kNotFound);
    aExtendedPanId = mActiveDataset.mExtendedPanId;

exit:
    return error;
}

Error CommissionerApp::SetExtendedPanId(const ByteArray &aExtendedPanId)
{
    Error                    error = Error::kNone;
    ActiveOperationalDataset activeDataset;

    VerifyOrExit(IsActive(), error = Error::kInvalidState);

    activeDataset.mExtendedPanId = aExtendedPanId;
    activeDataset.mPresentFlags |= ActiveOperationalDataset::kExtendedPanIdBit;

    SuccessOrExit(error = mCommissioner->SetActiveDataset(activeDataset));

    error = Error::kNone;
    MergeDataset(mActiveDataset, activeDataset);

exit:
    return error;
}

Error CommissionerApp::GetMeshLocalPrefix(std::string &aPrefix)
{
    Error error = Error::kNone;

    VerifyOrExit(IsActive(), error = Error::kInvalidState);

    SuccessOrExit(error = mCommissioner->GetActiveDataset(mActiveDataset, 0xFFFF));

    VerifyOrExit(mActiveDataset.mPresentFlags & ActiveOperationalDataset::kMeshLocalPrefixBit,
                 error = Error::kNotFound);
    aPrefix = Ipv6PrefixToString(mActiveDataset.mMeshLocalPrefix);

exit:
    return error;
}

Error CommissionerApp::SetMeshLocalPrefix(const std::string &aPrefix, MilliSeconds aDelay)
{
    Error                     error = Error::kNone;
    PendingOperationalDataset pendingDataset;

    VerifyOrExit(IsActive(), error = Error::kInvalidState);

    SuccessOrExit(error = Ipv6PrefixFromString(pendingDataset.mMeshLocalPrefix, aPrefix));
    pendingDataset.mPresentFlags |= PendingOperationalDataset::kMeshLocalPrefixBit;

    pendingDataset.mDelayTimer = aDelay.count();
    pendingDataset.mPresentFlags |= PendingOperationalDataset::kDelayTimerBit;

    SuccessOrExit(error = mCommissioner->SetPendingDataset(pendingDataset));

    error = Error::kNone;
    MergeDataset(mPendingDataset, pendingDataset);

exit:
    return error;
}

Error CommissionerApp::GetNetworkMasterKey(ByteArray &aMasterKey)
{
    Error error = Error::kNone;

    VerifyOrExit(IsActive(), error = Error::kInvalidState);

    SuccessOrExit(error = mCommissioner->GetActiveDataset(mActiveDataset, 0xFFFF));

    VerifyOrExit(mActiveDataset.mPresentFlags & ActiveOperationalDataset::kNetworkMasterKeyBit,
                 error = Error::kNotFound);
    aMasterKey = mActiveDataset.mNetworkMasterKey;

exit:
    return error;
}

Error CommissionerApp::SetNetworkMasterKey(const ByteArray &aMasterKey, MilliSeconds aDelay)
{
    Error                     error = Error::kNone;
    PendingOperationalDataset pendingDataset;

    VerifyOrExit(IsActive(), error = Error::kInvalidState);

    pendingDataset.mNetworkMasterKey = aMasterKey;
    pendingDataset.mPresentFlags |= PendingOperationalDataset::kNetworkMasterKeyBit;

    pendingDataset.mDelayTimer = aDelay.count();
    pendingDataset.mPresentFlags |= PendingOperationalDataset::kDelayTimerBit;

    SuccessOrExit(error = mCommissioner->SetPendingDataset(pendingDataset));

    error = Error::kNone;
    MergeDataset(mPendingDataset, pendingDataset);

exit:
    return error;
}

Error CommissionerApp::GetNetworkName(std::string &aNetworkName) const
{
    Error error = Error::kNone;

    VerifyOrExit(IsActive(), error = Error::kInvalidState);

    VerifyOrExit(mActiveDataset.mPresentFlags & ActiveOperationalDataset::kNetworkNameBit, error = Error::kNotFound);
    aNetworkName = mActiveDataset.mNetworkName;

exit:
    return error;
}

Error CommissionerApp::SetNetworkName(const std::string &aNetworkName)
{
    Error                    error = Error::kNone;
    ActiveOperationalDataset activeDataset;

    VerifyOrExit(IsActive(), error = Error::kInvalidState);

    activeDataset.mNetworkName = aNetworkName;
    activeDataset.mPresentFlags |= ActiveOperationalDataset::kNetworkNameBit;

    SuccessOrExit(error = mCommissioner->SetActiveDataset(activeDataset));

    error = Error::kNone;
    MergeDataset(mActiveDataset, activeDataset);

exit:
    return error;
}

Error CommissionerApp::GetPanId(uint16_t &aPanId)
{
    Error error = Error::kNone;

    VerifyOrExit(IsActive(), error = Error::kInvalidState);

    SuccessOrExit(error = mCommissioner->GetActiveDataset(mActiveDataset, 0xFFFF));

    VerifyOrExit(mActiveDataset.mPresentFlags & ActiveOperationalDataset::kPanIdBit, error = Error::kNotFound);
    aPanId = mActiveDataset.mPanId;

exit:
    return error;
}

Error CommissionerApp::SetPanId(uint16_t aPanId, MilliSeconds aDelay)
{
    Error                     error = Error::kNone;
    PendingOperationalDataset pendingDataset;

    VerifyOrExit(IsActive(), error = Error::kInvalidState);

    pendingDataset.mPanId = aPanId;
    pendingDataset.mPresentFlags |= PendingOperationalDataset::kPanIdBit;

    pendingDataset.mDelayTimer = aDelay.count();
    pendingDataset.mPresentFlags |= PendingOperationalDataset::kDelayTimerBit;

    SuccessOrExit(error = mCommissioner->SetPendingDataset(pendingDataset));

    error = Error::kNone;
    MergeDataset(mPendingDataset, pendingDataset);

exit:
    return error;
}

Error CommissionerApp::GetPSKc(ByteArray &aPSKc) const
{
    Error error = Error::kNone;

    VerifyOrExit(IsActive(), error = Error::kInvalidState);

    VerifyOrExit(mActiveDataset.mPresentFlags & ActiveOperationalDataset::kPSKcBit, error = Error::kNotFound);
    aPSKc = mActiveDataset.mPSKc;

exit:
    return error;
}

Error CommissionerApp::SetPSKc(const ByteArray &aPSKc)
{
    Error                    error = Error::kNone;
    ActiveOperationalDataset activeDataset;

    VerifyOrExit(IsActive(), error = Error::kInvalidState);

    activeDataset.mPSKc = aPSKc;
    activeDataset.mPresentFlags |= ActiveOperationalDataset::kPSKcBit;

    SuccessOrExit(error = mCommissioner->SetActiveDataset(activeDataset));

    error = Error::kNone;
    MergeDataset(mActiveDataset, activeDataset);

exit:
    return error;
}

Error CommissionerApp::GetSecurityPolicy(SecurityPolicy &aSecurityPolicy) const
{
    Error error = Error::kNone;

    VerifyOrExit(IsActive(), error = Error::kInvalidState);

    VerifyOrExit(mActiveDataset.mPresentFlags & ActiveOperationalDataset::kSecurityPolicyBit, error = Error::kNotFound);
    aSecurityPolicy = mActiveDataset.mSecurityPolicy;

exit:
    return error;
}

Error CommissionerApp::SetSecurityPolicy(const SecurityPolicy &aSecurityPolicy)
{
    Error                    error = Error::kNone;
    ActiveOperationalDataset activeDataset;

    VerifyOrExit(IsActive(), error = Error::kInvalidState);

    activeDataset.mSecurityPolicy = aSecurityPolicy;
    activeDataset.mPresentFlags |= ActiveOperationalDataset::kSecurityPolicyBit;

    SuccessOrExit(error = mCommissioner->SetActiveDataset(activeDataset));

    error = Error::kNone;
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
    Error error = Error::kNone;

    VerifyOrExit(IsActive() && IsCcmMode(), error = Error::kInvalidState);

    VerifyOrExit(mBbrDataset.mPresentFlags & BbrDataset::kTriHostnameBit, error = Error::kNotFound);
    aHostname = mBbrDataset.mTriHostname;

exit:
    return error;
}

Error CommissionerApp::SetTriHostname(const std::string &aHostname)
{
    Error      error = Error::kNone;
    BbrDataset bbrDataset;

    VerifyOrExit(IsActive() && IsCcmMode(), error = Error::kInvalidState);

    bbrDataset.mTriHostname = aHostname;
    bbrDataset.mPresentFlags |= BbrDataset::kTriHostnameBit;

    SuccessOrExit(error = mCommissioner->SetBbrDataset(bbrDataset));

    error = Error::kNone;
    MergeDataset(mBbrDataset, bbrDataset);

exit:
    return error;
}

Error CommissionerApp::GetRegistrarHostname(std::string &aHostname) const
{
    Error error = Error::kNone;

    VerifyOrExit(IsActive() && IsCcmMode(), error = Error::kInvalidState);

    VerifyOrExit(mBbrDataset.mPresentFlags & BbrDataset::kRegistrarHostnameBit, error = Error::kNotFound);
    aHostname = mBbrDataset.mRegistrarHostname;

exit:
    return error;
}

Error CommissionerApp::SetRegistrarHostname(const std::string &aHostname)
{
    Error      error = Error::kNone;
    BbrDataset bbrDataset;

    VerifyOrExit(IsActive() && IsCcmMode(), error = Error::kInvalidState);

    bbrDataset.mRegistrarHostname = aHostname;
    bbrDataset.mPresentFlags |= BbrDataset::kRegistrarHostnameBit;

    SuccessOrExit(error = mCommissioner->SetBbrDataset(bbrDataset));

    error = Error::kNone;
    MergeDataset(mBbrDataset, bbrDataset);

exit:
    return error;
}

Error CommissionerApp::GetRegistrarIpv6Addr(std::string &aIpv6Addr) const
{
    Error error = Error::kNone;

    VerifyOrExit(IsActive() && IsCcmMode(), error = Error::kInvalidState);

    VerifyOrExit(mBbrDataset.mPresentFlags & BbrDataset::kRegistrarIpv6AddrBit, error = Error::kNotFound);
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
    Error error = Error::kNone;

    VerifyOrExit(IsActive() && IsCcmMode(), error = Error::kInvalidState);
    SuccessOrExit(error = mCommissioner->CommandReenroll(aDstAddr));

exit:
    return error;
}

Error CommissionerApp::DomainReset(const std::string &aDstAddr)
{
    Error error = Error::kNone;

    VerifyOrExit(IsActive() && IsCcmMode(), error = Error::kInvalidState);
    SuccessOrExit(error = mCommissioner->CommandDomainReset(aDstAddr));

exit:
    return error;
}

Error CommissionerApp::Migrate(const std::string &aDstAddr, const std::string &aDesignatedNetwork)
{
    Error error = Error::kNone;

    VerifyOrExit(IsActive() && IsCcmMode(), error = Error::kInvalidState);
    SuccessOrExit(error = mCommissioner->CommandMigrate(aDstAddr, aDesignatedNetwork));

exit:
    return error;
}

Error CommissionerApp::RegisterMulticastListener(const std::vector<std::string> &aMulticastAddrList, Seconds aTimeout)
{
    Error       error = Error::kNone;
    std::string pbbrAddr;
    uint8_t     status;

    VerifyOrExit(IsActive(), error = Error::kInvalidState);
    SuccessOrExit(error = GetPrimaryBbrAddr(pbbrAddr));
    SuccessOrExit(error =
                      mCommissioner->RegisterMulticastListener(status, pbbrAddr, aMulticastAddrList, aTimeout.count()));
    VerifyOrExit(status == kMlrStatusSuccess, error = Error::kFailed);

exit:
    return error;
}

Error CommissionerApp::AnnounceBegin(uint32_t           aChannelMask,
                                     uint8_t            aCount,
                                     MilliSeconds       aPeriod,
                                     const std::string &aDtsAddr)
{
    Error error = Error::kNone;

    VerifyOrExit(IsActive(), error = Error::kInvalidState);
    SuccessOrExit(error = mCommissioner->AnnounceBegin(aChannelMask, aCount, aPeriod.count(), aDtsAddr));

exit:
    return error;
}

Error CommissionerApp::PanIdQuery(uint32_t aChannelMask, uint16_t aPanId, const std::string &aDstAddr)
{
    Error error = Error::kNone;

    VerifyOrExit(IsActive(), error = Error::kInvalidState);
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
    Error error = Error::kNone;

    VerifyOrExit(IsActive(), error = Error::kInvalidState);
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

Error CommissionerApp::GetPrimaryBbrAddr(std::string &aAddr)
{
    Error       error = Error::kNone;
    std::string meshLocalPrefix;

    SuccessOrExit(error = GetMeshLocalPrefix(meshLocalPrefix));
    SuccessOrExit(error = Commissioner::GetMeshLocalAddr(aAddr, meshLocalPrefix, kPrimaryBbrAloc16));

exit:
    return error;
}

const ByteArray &CommissionerApp::GetToken() const
{
    return mSignedToken;
}

Error CommissionerApp::RequestToken(const std::string &aAddr, uint16_t aPort)
{
    return mCommissioner->RequestToken(mSignedToken, aAddr, aPort);
}

Error CommissionerApp::SetToken(const ByteArray &aSignedToken, const ByteArray &aSignerCert)
{
    Error error = mCommissioner->SetToken(aSignedToken, aSignerCert);
    if (error == Error::kNone)
    {
        mSignedToken = aSignedToken;
    }
    return error;
}

bool CommissionerApp::JoinerKey::operator<(const JoinerKey &aOther) const
{
    return mType < aOther.mType || (mType == aOther.mType && mId < aOther.mId);
}

CommissionerDataset CommissionerApp::MakeDefaultCommissionerDataset()
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

bool CommissionerApp::OnJoinerFinalize(const ByteArray &  aJoinerId,
                                       const std::string &aVendorName,
                                       const std::string &aVendorModel,
                                       const std::string &aVendorSwVersion,
                                       const ByteArray &  aVendorStackVersion,
                                       const std::string &aProvisioningUrl,
                                       const ByteArray &  aVendorData)
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
                                     const ByteArray &  aEnergyList)
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
            if (aError == Error::kNone)
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
            if (aError == Error::kNone)
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
    Error error = Error::kInvalidArgs;

    VerifyOrExit(aPSKd.size() >= kMinJoinerPassphraseLength && aPSKd.size() <= kMaxJoinerPassphraseLength);

    for (auto c : aPSKd)
    {
        VerifyOrExit(isdigit(c) || isupper(c));
        VerifyOrExit(c != 'I' && c != 'O' && c != 'Q' && c != 'Z');
    }

    error = Error::kNone;

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

} // namespace commissioner

} // namespace ot
