/*
 *    Copyright (c) 2021, The OpenThread Commissioner Authors.
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

#include <cstdint>
#include <string>
#include <vector>

#include "app/commissioner_app.hpp"
#include "commissioner/defines.hpp"
#include "commissioner/error.hpp"
#include "commissioner/network_data.hpp"
#include "common/address.hpp"

#define UNUSED(A) (void)A

namespace ot {

namespace commissioner {

std::string CommissionerApp::OnJoinerRequest(const ByteArray &aJoinerId)
{
    UNUSED(aJoinerId);
    return "";
}

void CommissionerApp::OnJoinerConnected(const ByteArray &aJoinerId, Error aError)
{
    UNUSED(aJoinerId);
    UNUSED(aError);
}

bool CommissionerApp::OnJoinerFinalize(const ByteArray   &aJoinerId,
                                       const std::string &aVendorName,
                                       const std::string &aVendorModel,
                                       const std::string &aVendorSwVersion,
                                       const ByteArray   &aVendorStackVersion,
                                       const std::string &aProvisioningUrl,
                                       const ByteArray   &aVendorData)
{
    UNUSED(aJoinerId);
    UNUSED(aVendorName);
    UNUSED(aVendorData);
    UNUSED(aVendorModel);
    UNUSED(aVendorSwVersion);
    UNUSED(aVendorStackVersion);
    UNUSED(aProvisioningUrl);
    UNUSED(aVendorData);

    return false;
}

void CommissionerApp::OnKeepAliveResponse(Error aError)
{
    UNUSED(aError);
}

void CommissionerApp::OnPanIdConflict(const std::string &aPeerAddr, const ChannelMask &aChannelMask, uint16_t aPanId)
{
    UNUSED(aPeerAddr);
    UNUSED(aChannelMask);
    UNUSED(aPanId);
}

void CommissionerApp::OnEnergyReport(const std::string &aPeerAddr,
                                     const ChannelMask &aChannelMask,
                                     const ByteArray   &aEnergyList)
{
    UNUSED(aPeerAddr);
    UNUSED(aChannelMask);
    UNUSED(aEnergyList);
}

void CommissionerApp::OnDatasetChanged()
{
}

Error CommissionerApp::Start(std::string       &aExistingCommissionerId,
                             const std::string &aBorderAgentAddr,
                             uint16_t           aBorderAgentPort)
{
    UNUSED(aExistingCommissionerId);
    UNUSED(aBorderAgentAddr);
    UNUSED(aBorderAgentPort);
    return Error{};
}

Error CommissionerApp::Connect(const std::string &aBorderAgentAddr, uint16_t aBorderAgentPort)
{
    UNUSED(aBorderAgentAddr);
    UNUSED(aBorderAgentPort);
    return Error{};
}

void CommissionerApp::Stop()
{
}

void CommissionerApp::CancelRequests()
{
}

bool CommissionerApp::IsActive() const
{
    return false;
}

State CommissionerApp::GetState() const
{
    return State::kDisabled;
}

Error CommissionerApp::SaveNetworkData(const std::string &aFilename)
{
    UNUSED(aFilename);
    return Error{};
};

Error CommissionerApp::SyncNetworkData(void)
{
    return Error{};
}

Error CommissionerApp::GetSessionId(uint16_t &aSessionId) const
{
    UNUSED(aSessionId);
    return Error{};
}

Error CommissionerApp::GetBorderAgentLocator(uint16_t &aLocator) const
{
    UNUSED(aLocator);
    return Error{};
}

Error CommissionerApp::GetSteeringData(ByteArray &aSteeringData, JoinerType aJoinerType) const
{
    UNUSED(aSteeringData);
    UNUSED(aJoinerType);
    return Error{};
}

Error CommissionerApp::EnableJoiner(JoinerType         aType,
                                    uint64_t           aEui64,
                                    const std::string &aPSKd,
                                    const std::string &aProvisioningUrl)
{
    UNUSED(aType);
    UNUSED(aEui64);
    UNUSED(aPSKd);
    UNUSED(aProvisioningUrl);
    return Error{};
}

Error CommissionerApp::DisableJoiner(JoinerType aType, uint64_t aEui64)
{
    UNUSED(aType);
    UNUSED(aEui64);
    return Error{};
}

Error CommissionerApp::EnableAllJoiners(JoinerType aType, const std::string &aPSKd, const std::string &aProvisioningUrl)
{
    UNUSED(aType);
    UNUSED(aPSKd);
    UNUSED(aProvisioningUrl);
    return Error{};
}

Error CommissionerApp::DisableAllJoiners(JoinerType aType)
{
    UNUSED(aType);
    return Error{};
}

Error CommissionerApp::GetJoinerUdpPort(uint16_t &aJoinerUdpPort, JoinerType aJoinerType) const
{
    UNUSED(aJoinerUdpPort);
    UNUSED(aJoinerType);
    return Error{};
}

Error CommissionerApp::SetJoinerUdpPort(JoinerType aType, uint16_t aUdpPort)
{
    UNUSED(aType);
    UNUSED(aUdpPort);
    return Error{};
}

Error CommissionerApp::GetCommissionerDataset(CommissionerDataset &aDataset, uint16_t aDatasetFlags)
{
    UNUSED(aDataset);
    UNUSED(aDatasetFlags);
    return Error{};
}

Error CommissionerApp::SetCommissionerDataset(const CommissionerDataset &aDataset)
{
    UNUSED(aDataset);
    return Error{};
}

Error CommissionerApp::GetActiveTimestamp(Timestamp &aTimestamp) const
{
    UNUSED(aTimestamp);
    return Error{};
}

Error CommissionerApp::GetChannel(Channel &aChannel)
{
    UNUSED(aChannel);
    return Error{};
}

Error CommissionerApp::SetChannel(const Channel &aChannel, MilliSeconds aDelay)
{
    UNUSED(aChannel);
    UNUSED(aDelay);
    return Error{};
}

Error CommissionerApp::GetChannelMask(ChannelMask &aChannelMask) const
{
    UNUSED(aChannelMask);
    return Error{};
}

Error CommissionerApp::SetChannelMask(const ChannelMask &aChannelMask)
{
    UNUSED(aChannelMask);
    return Error{};
}

Error CommissionerApp::GetExtendedPanId(ByteArray &aExtendedPanId) const
{
    UNUSED(aExtendedPanId);
    return Error{};
}

Error CommissionerApp::SetExtendedPanId(const ByteArray &aExtendedPanId)
{
    UNUSED(aExtendedPanId);
    return Error{};
}

Error CommissionerApp::GetMeshLocalPrefix(std::string &aPrefix)
{
    UNUSED(aPrefix);
    return Error{};
}

Error CommissionerApp::SetMeshLocalPrefix(const std::string &aPrefix, MilliSeconds aDelay)
{
    UNUSED(aPrefix);
    UNUSED(aDelay);
    return Error{};
}

Error CommissionerApp::GetMeshLocalAddr(std::string       &aMeshLocalAddr,
                                        const std::string &aMeshLocalPrefix,
                                        uint16_t           aLocator16)
{
    UNUSED(aMeshLocalAddr);
    UNUSED(aMeshLocalPrefix);
    UNUSED(aLocator16);
    return Error{};
}

Error CommissionerApp::GetNetworkMasterKey(ByteArray &aMasterKey)
{
    UNUSED(aMasterKey);
    return Error{};
}

Error CommissionerApp::SetNetworkMasterKey(const ByteArray &aMasterKey, MilliSeconds aDelay)
{
    UNUSED(aMasterKey);
    UNUSED(aDelay);
    return Error{};
}

Error CommissionerApp::GetNetworkName(std::string &aNetworkName) const
{
    UNUSED(aNetworkName);
    return Error{};
}

Error CommissionerApp::SetNetworkName(const std::string &aNetworkName)
{
    UNUSED(aNetworkName);
    return Error{};
}

Error CommissionerApp::GetPanId(uint16_t &aPanId)
{
    UNUSED(aPanId);
    return Error{};
}

Error CommissionerApp::SetPanId(uint16_t aPanId, MilliSeconds aDelay)
{
    UNUSED(aPanId);
    UNUSED(aDelay);
    return Error{};
}

Error CommissionerApp::GetPSKc(ByteArray &aPSKc) const
{
    UNUSED(aPSKc);
    return Error{};
}

Error CommissionerApp::SetPSKc(const ByteArray &aPSKc)
{
    UNUSED(aPSKc);
    return Error{};
}

Error CommissionerApp::GetSecurityPolicy(SecurityPolicy &aSecurityPolicy) const
{
    UNUSED(aSecurityPolicy);
    return Error{};
}

Error CommissionerApp::SetSecurityPolicy(const SecurityPolicy &aSecurityPolicy)
{
    UNUSED(aSecurityPolicy);
    return Error{};
}

Error CommissionerApp::GetActiveDataset(ActiveOperationalDataset &aDataset, uint16_t aDatasetFlags)
{
    UNUSED(aDataset);
    UNUSED(aDatasetFlags);
    return Error{};
}

Error CommissionerApp::SetActiveDataset(const ActiveOperationalDataset &aDataset)
{
    UNUSED(aDataset);
    return Error{};
}

Error CommissionerApp::GetPendingDataset(PendingOperationalDataset &aDataset, uint16_t aDatasetFlags)
{
    UNUSED(aDataset);
    UNUSED(aDatasetFlags);
    return Error{};
}

Error CommissionerApp::SetPendingDataset(const PendingOperationalDataset &aDataset)
{
    UNUSED(aDataset);
    return Error{};
}

Error CommissionerApp::GetTriHostname(std::string &aHostname) const
{
    UNUSED(aHostname);
    return Error{};
}

Error CommissionerApp::SetTriHostname(const std::string &aHostname)
{
    UNUSED(aHostname);
    return Error{};
}

Error CommissionerApp::GetRegistrarHostname(std::string &aHostname) const
{
    UNUSED(aHostname);
    return Error{};
}

Error CommissionerApp::SetRegistrarHostname(const std::string &aHostname)
{
    UNUSED(aHostname);
    return Error{};
}

Error CommissionerApp::GetRegistrarIpv6Addr(std::string &aIpv6Addr) const
{
    UNUSED(aIpv6Addr);
    return Error{};
}

Error CommissionerApp::GetBbrDataset(BbrDataset &aDataset, uint16_t aDatasetFlags)
{
    UNUSED(aDataset);
    UNUSED(aDatasetFlags);
    return Error{};
}

Error CommissionerApp::SetBbrDataset(const BbrDataset &aDataset)
{
    UNUSED(aDataset);
    return Error{};
}

Error CommissionerApp::Reenroll(const std::string &aDstAddr)
{
    UNUSED(aDstAddr);
    return Error{};
}

Error CommissionerApp::DomainReset(const std::string &aDstAddr)
{
    UNUSED(aDstAddr);
    return Error{};
}

Error CommissionerApp::Migrate(const std::string &aDstAddr, const std::string &aDesignatedNetwork)
{
    UNUSED(aDstAddr);
    UNUSED(aDesignatedNetwork);
    return Error{};
}

const ByteArray &CommissionerApp::GetToken() const
{
    static ByteArray token{};
    return token;
}

Error CommissionerApp::RequestToken(const std::string &aAddr, uint16_t aPort)
{
    UNUSED(aAddr);
    UNUSED(aPort);
    return Error{};
}

Error CommissionerApp::SetToken(const ByteArray &aSignedToken)
{
    UNUSED(aSignedToken);
    return Error{};
}

Error CommissionerApp::RegisterMulticastListener(const std::vector<std::string> &aMulticastAddrList, Seconds aTimeout)
{
    UNUSED(aMulticastAddrList);
    UNUSED(aTimeout);
    return Error{};
}

Error CommissionerApp::AnnounceBegin(uint32_t           aChannelMask,
                                     uint8_t            aCount,
                                     MilliSeconds       aPeriod,
                                     const std::string &aDtsAddr)
{
    UNUSED(aChannelMask);
    UNUSED(aCount);
    UNUSED(aPeriod);
    UNUSED(aDtsAddr);
    return Error{};
}

Error CommissionerApp::PanIdQuery(uint32_t aChannelMask, uint16_t aPanId, const std::string &aDstAddr)
{
    UNUSED(aChannelMask);
    UNUSED(aPanId);
    UNUSED(aDstAddr);
    return Error{};
}

bool CommissionerApp::HasPanIdConflict(uint16_t aPanId) const
{
    UNUSED(aPanId);
    return false;
}

Error CommissionerApp::EnergyScan(uint32_t           aChannelMask,
                                  uint8_t            aCount,
                                  uint16_t           aPeriod,
                                  uint16_t           aScanDuration,
                                  const std::string &aDstAddr)
{
    UNUSED(aChannelMask);
    UNUSED(aCount);
    UNUSED(aPeriod);
    UNUSED(aScanDuration);
    UNUSED(aDstAddr);
    return Error{};
}

const EnergyReport *CommissionerApp::GetEnergyReport(const Address &aDstAddr) const
{
    UNUSED(aDstAddr);
    return nullptr;
}

const EnergyReportMap &CommissionerApp::GetAllEnergyReports() const
{
    static EnergyReportMap sEnergyReportMap{};
    return sEnergyReportMap;
}

Error CommissionerApp::CommandDiagReset(const std::string &aAddr, uint64_t aDiagTlvFlags)
{
    UNUSED(aAddr);
    UNUSED(aDiagTlvFlags);
    return Error{};
}

} // namespace commissioner

} // namespace ot
