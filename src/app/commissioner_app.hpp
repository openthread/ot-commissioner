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
 *   The file defines the interface of a commissioner application.
 *
 *   A commissioner application provides easy for use interface
 *   based on the commissioner library and stores network data
 *   of the connecting Thread network.
 */

#ifndef OT_COMM_APP_COMMISSIONER_APP_HPP_
#define OT_COMM_APP_COMMISSIONER_APP_HPP_

#include <chrono>
#include <fstream>
#include <list>
#include <map>
#include <set>
#include <unordered_set>

#include <commissioner/commissioner.hpp>
#include <commissioner/network_data.hpp>

#include <address.hpp>

namespace ot {

namespace commissioner {

struct EnergyReport
{
    ChannelMask mChannelMask;
    ByteArray   mEnergyList;
};
using EnergyReportMap = std::map<Address, EnergyReport>;

class CommissionerApp
{
public:
    using MilliSeconds = std::chrono::milliseconds;
    using Seconds      = std::chrono::seconds;

    static std::shared_ptr<CommissionerApp> Create(const Config &aConfig);
    ~CommissionerApp() = default;

    Error Start(std::string &aExistingCommissionerId, const std::string &aBorderAgentAddr, uint16_t aBorderAgentPort);
    void  Stop();

    void AbortRequests();

    // Returns if current commissioner is in active state.
    // Should always be true if starting the commissioner app has succeed.
    bool IsActive() const;

    bool IsCcmMode() const;

    // Save network data of current Thread network to file in JSON format.
    Error SaveNetworkData(const std::string &aFilename);

    // Pull network data to local.
    Error PullNetworkData();

    /*
     * Commissioner Dataset APIs
     */
    Error GetSessionId(uint16_t &aSessionId) const;
    Error GetBorderAgentLocator(uint16_t &aLocator) const;

    Error GetSteeringData(ByteArray &aSteeringData, JoinerType aJoinerType) const;
    Error EnableJoiner(JoinerType         aType,
                       uint64_t           aEui64,
                       const ByteArray &  aPSKd            = {},
                       const std::string &aProvisioningUrl = {});
    Error DisableJoiner(JoinerType aType, uint64_t aEui64);
    Error EnableAllJoiners(JoinerType aType, const ByteArray &aPSKd, const std::string &aProvisioningUrl);
    Error DisableAllJoiners(JoinerType aType);

    // Currently, for only non-CCM joiners.
    bool IsJoinerCommissioned(JoinerType aType, uint64_t aEui64);

    Error GetJoinerUdpPort(uint16_t &aJoinerUdpPort, JoinerType aJoinerType) const;
    Error SetJoinerUdpPort(JoinerType aType, uint16_t aUdpPort);

    // Advanced Commissioner Dataset APIs exposed for setting with customized user data.
    // Not recommended unless you have good understanding of the encoding of each fields.

    // Always send MGMT_COMMISSIONER_GET.req.
    Error GetCommissionerDataset(CommissionerDataset &aDataset, uint16_t aDatasetFlags);

    // Always send MGMT_COMMISSIONER_SET.req.
    Error SetCommissionerDataset(const CommissionerDataset &aDataset);

    /*
     * Operational Dataset APIs
     */
    // TODO(wgtdkp): secure pending operational dataset update
    Error GetActiveTimestamp(Timestamp &aTimestamp) const;
    Error GetChannel(Channel &aChannel);
    Error SetChannel(const Channel &aChannel, MilliSeconds aDelay);
    Error GetChannelMask(ChannelMask &aChannelMask) const;
    Error SetChannelMask(const ChannelMask &aChannelMask);
    Error GetExtendedPanId(ByteArray &aExtendedPanId) const;
    Error SetExtendedPanId(const ByteArray &aExtendedPanId);
    Error GetMeshLocalPrefix(std::string &aPrefix);
    Error SetMeshLocalPrefix(const std::string &aPrefix, MilliSeconds aDelay);
    Error GetNetworkMasterKey(ByteArray &aMasterKey);
    Error SetNetworkMasterKey(const ByteArray &aMasterKey, MilliSeconds aDelay);
    Error GetNetworkName(std::string &aNetworkName) const;
    Error SetNetworkName(const std::string &aNetworkName);
    Error GetPanId(uint16_t &aPanId);
    Error SetPanId(uint16_t aPanId, MilliSeconds aDelay);
    Error GetPSKc(ByteArray &aPSKc) const;
    Error SetPSKc(const ByteArray &aPSKc);

    Error GetSecurityPolicy(SecurityPolicy &aSecurityPolicy) const;
    Error SetSecurityPolicy(const SecurityPolicy &aSecurityPolicy);

    // Advanced Active/Pending Operational Dataset APIs exposed for setting with customized user data.
    // Not recommended unless you have good understanding of the encoding of each fields.

    // Always send MGMT_ACTIVE_GET.req.
    Error GetActiveDataset(ActiveOperationalDataset &aDataset, uint16_t aDatasetFlags);

    // Always send MGMT_ACTIVE_SET.req.
    Error SetActiveDataset(const ActiveOperationalDataset &aDataset);

    // Always send MGMT_PENDING_GET.req.
    Error GetPendingDataset(PendingOperationalDataset &aDataset, uint16_t aDatasetFlags);

    // Always send MGMT_PENDING_SET.req.
    Error SetPendingDataset(const PendingOperationalDataset &aDataset);

    /*
     * BBR Dataset APIs
     */
    Error GetTriHostname(std::string &aHostname) const;
    Error SetTriHostname(const std::string &aHostname);
    Error GetRegistrarHostname(std::string &aHostname) const;
    Error SetRegistrarHostname(const std::string &aHostname);
    Error GetRegistrarIpv6Addr(std::string &aIpv6Addr) const;

    // Advanced BBR Dataset APIs exposed for setting with customized user data.
    // Not recommended unless you have good understanding of the encoding of each fields.

    // Always send MGMT_BBR_GET.req.
    Error GetBbrDataset(BbrDataset &aDataset, uint16_t aDatasetFlags);

    // Always send MGMT_BBR_SET.req.
    Error SetBbrDataset(const BbrDataset &aDataset);

    /*
     * Command APIs
     */
    Error Reenroll(const std::string &aDstAddr);
    Error DomainReset(const std::string &aDstAddr);
    Error Migrate(const std::string &aDstAddr, const std::string &aDesignatedNetwork);
    Error RegisterMulticastListener(const std::vector<std::string> &aMulticastAddrList, Seconds aTimeout);
    Error AnnounceBegin(uint32_t aChannelMask, uint8_t aCount, MilliSeconds aPeriod, const std::string &aDtsAddr);
    Error PanIdQuery(uint32_t aChannelMask, uint16_t aPanId, const std::string &aDstAddr);
    bool  HasPanIdConflict(uint16_t aPanId) const;
    Error EnergyScan(uint32_t           aChannelMask,
                     uint8_t            aCount,
                     uint16_t           aPeriod,
                     uint16_t           aScanDuration,
                     const std::string &aDstAddr);
    const EnergyReport *   GetEnergyReport(const Address &aDstAddr) const;
    const EnergyReportMap &GetAllEnergyReports() const;

    /*
     * Others
     */
    // Get current domain name.
    const std::string &GetDomainName() const;
    Error              GetPrimaryBbrAddr(std::string &aAddr);
    const ByteArray &  GetToken() const;
    Error              RequestToken(const std::string &aAddr, uint16_t aPort);
    Error              SetToken(const ByteArray &aSignedToken, const ByteArray &aSignerCert);

private:
    CommissionerApp() = default;
    Error Init(const Config &aConfig);

    struct JoinerKey
    {
        JoinerType mType;
        ByteArray  mId;

        bool operator<(const JoinerKey &aOther) const;
    };

    static ByteArray &GetSteeringData(CommissionerDataset &aDataset, JoinerType aJoinerType);
    static uint16_t & GetJoinerUdpPort(CommissionerDataset &aDataset, JoinerType aJoinerType);

    // Erases all joiner with specific type. Returns the number of erased joiners.
    size_t      EraseAllJoiners(JoinerType aJoinerType);
    static void MergeDataset(ActiveOperationalDataset &aDst, const ActiveOperationalDataset &aSrc);
    static void MergeDataset(PendingOperationalDataset &aDst, const PendingOperationalDataset &aSrc);
    static void MergeDataset(BbrDataset &aDst, const BbrDataset &aSrc);
    static void MergeDataset(CommissionerDataset &aDst, const CommissionerDataset &aSrc);

    const JoinerInfo *GetJoinerInfo(JoinerType aType, const ByteArray &aJoinerId);

    std::shared_ptr<Commissioner> mCommissioner;

    std::map<JoinerKey, JoinerInfo> mJoiners;

    ByteArray mSignedToken;

private:
    void HandlePanIdConflict(const std::string *aPeerAddr,
                             const ChannelMask *aChannelMask,
                             const uint16_t *   aPanId,
                             Error              aError);
    void HandleEnergyReport(const std::string *aPeerAddr,
                            const ChannelMask *aChannelMask,
                            const ByteArray *  aEnergyList,
                            Error              aError);

    std::map<uint16_t, ChannelMask> mPanIdConflicts;
    EnergyReportMap                 mEnergyReports;

    void HandleDatasetChanged(Error error);

    /*
     * Below are network data associated to the connected Thread network.
     */
    ActiveOperationalDataset  mActiveDataset;
    PendingOperationalDataset mPendingDataset;
    CommissionerDataset       mCommDataset;
    BbrDataset                mBbrDataset;
};

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_APP_COMMISSIONER_APP_HPP_
