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
 *   The file defines the interface of a commissioner application.
 *
 *   A commissioner application provides easy for use interface
 *   based on the commissioner library and stores network data
 *   of the connecting Thread network.
 */

#ifndef OT_COMM_APP_COMMISSIONER_APP_HPP_
#define OT_COMM_APP_COMMISSIONER_APP_HPP_

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "commissioner/commissioner.hpp"
#include "commissioner/defines.hpp"
#include "commissioner/error.hpp"
#include "commissioner/network_data.hpp"
#include "commissioner/network_diag_data.hpp"
#include "common/address.hpp"

namespace ot {

namespace commissioner {

struct EnergyReport
{
    ChannelMask mChannelMask;
    ByteArray   mEnergyList;
};
using EnergyReportMap = std::map<Address, EnergyReport>;

using DiagAnsDataMap = std::map<Address, NetDiagData>;

/**
 * @brief Enumeration of Joiner Type for steering.
 *
 */
enum class JoinerType
{
    kMeshCoP = 0, ///< Conventional non-CCM joiner.
    kAE,          ///< CCM AE joiner.
    kNMKP         ///< CCM NMKP joiner.
};

/**
 * @brief Definition of joiner information.
 */
struct JoinerInfo
{
    JoinerType mType;

    // If the value is all-zeros, it represents for all joiners of this type.
    uint64_t mEui64; ///< The IEEE EUI-64 value.

    // Valid only if mType is kMeshCoP.
    std::string mPSKd; ///< The pre-shared device key.

    // Valid only if mType is kMeshCoP.
    std::string mProvisioningUrl;

    JoinerInfo(JoinerType aType, uint64_t aEui64, const std::string &aPSKd, const std::string &aProvisioningUrl);
};

class CommissionerApp : public CommissionerHandler
{
public:
    using MilliSeconds = std::chrono::milliseconds;
    using Seconds      = std::chrono::seconds;

    MOCKABLE ~CommissionerApp() = default;

    // Handle commissioner events.
    MOCKABLE std::string OnJoinerRequest(const ByteArray &aJoinerId) override;

    MOCKABLE void OnJoinerConnected(const ByteArray &aJoinerId, Error aError) override;

    MOCKABLE bool OnJoinerFinalize(const ByteArray   &aJoinerId,
                                   const std::string &aVendorName,
                                   const std::string &aVendorModel,
                                   const std::string &aVendorSwVersion,
                                   const ByteArray   &aVendorStackVersion,
                                   const std::string &aProvisioningUrl,
                                   const ByteArray   &aVendorData) override;

    MOCKABLE void OnKeepAliveResponse(Error aError) override;

    MOCKABLE void OnPanIdConflict(const std::string &aPeerAddr,
                                  const ChannelMask &aChannelMask,
                                  uint16_t           aPanId) override;

    MOCKABLE void OnEnergyReport(const std::string &aPeerAddr,
                                 const ChannelMask &aChannelMask,
                                 const ByteArray   &aEnergyList) override;

    MOCKABLE void OnDatasetChanged() override;

    MOCKABLE void OnDiagGetAnswerMessage(const std::string &aPeerAddr, const NetDiagData &aDiagAnsMsg) override;

    MOCKABLE Error Connect(const std::string &aBorderAgentAddr, uint16_t aBorderAgentPort);

    MOCKABLE State GetState() const;

    MOCKABLE Error Start(std::string       &aExistingCommissionerId,
                         const std::string &aBorderAgentAddr,
                         uint16_t           aBorderAgentPort);
    MOCKABLE void  Stop();

    MOCKABLE void CancelRequests();

    // Returns if current commissioner is in active state.
    // Should always be true if starting the commissioner app has succeed.
    MOCKABLE bool IsActive() const;

    bool IsCcmMode() const;

    // Save network data of current Thread network to file in JSON format.
    MOCKABLE Error SaveNetworkData(const std::string &aFilename);

    // Sync network data between the Thread Network and Commissioner.
    MOCKABLE Error SyncNetworkData(void);

    /*
     * Commissioner Dataset APIs
     */
    MOCKABLE Error GetSessionId(uint16_t &aSessionId) const;
    MOCKABLE Error GetBorderAgentLocator(uint16_t &aLocator) const;

    MOCKABLE Error GetSteeringData(ByteArray &aSteeringData, JoinerType aJoinerType) const;
    MOCKABLE Error EnableJoiner(JoinerType         aType,
                                uint64_t           aEui64,
                                const std::string &aPSKd            = {},
                                const std::string &aProvisioningUrl = {});
    MOCKABLE Error DisableJoiner(JoinerType aType, uint64_t aEui64);
    MOCKABLE Error EnableAllJoiners(JoinerType aType, const std::string &aPSKd, const std::string &aProvisioningUrl);
    MOCKABLE Error DisableAllJoiners(JoinerType aType);

    MOCKABLE Error GetJoinerUdpPort(uint16_t &aJoinerUdpPort, JoinerType aJoinerType) const;
    MOCKABLE Error SetJoinerUdpPort(JoinerType aType, uint16_t aUdpPort);

    // Advanced Commissioner Dataset APIs exposed for setting with customized user data.
    // Not recommended unless you have good understanding of the encoding of each fields.

    // Always send MGMT_COMMISSIONER_GET.req.
    MOCKABLE Error GetCommissionerDataset(CommissionerDataset &aDataset, uint16_t aDatasetFlags);

    // Always send MGMT_COMMISSIONER_SET.req.
    MOCKABLE Error SetCommissionerDataset(const CommissionerDataset &aDataset);

    /*
     * Operational Dataset APIs
     */
    // TODO(wgtdkp): secure pending operational dataset update
    MOCKABLE Error GetActiveTimestamp(Timestamp &aTimestamp) const;
    MOCKABLE Error GetChannel(Channel &aChannel);
    MOCKABLE Error SetChannel(const Channel &aChannel, MilliSeconds aDelay);
    MOCKABLE Error GetChannelMask(ChannelMask &aChannelMask) const;
    MOCKABLE Error SetChannelMask(const ChannelMask &aChannelMask);
    MOCKABLE Error GetExtendedPanId(ByteArray &aExtendedPanId) const;
    MOCKABLE Error SetExtendedPanId(const ByteArray &aExtendedPanId);
    MOCKABLE Error GetMeshLocalPrefix(std::string &aPrefix);
    MOCKABLE Error SetMeshLocalPrefix(const std::string &aPrefix, MilliSeconds aDelay);
    /**
     * @brief Get the Thread mesh local address of given 16bits mesh local prefix and locator.
     *
     * @param[out] aMeshLocalAddr    The returned mesh local address.
     * @param[in]  aMeshLocalPrefix  A mesh local prefix of the address.
     * @param[in]  aLocator16        A 16bits locator.
     *
     * @return Error::kNone, succeed; Otherwise, failed.
     */
    MOCKABLE Error GetMeshLocalAddr(std::string       &aMeshLocalAddr,
                                    const std::string &aMeshLocalPrefix,
                                    uint16_t           aLocator16);

    MOCKABLE Error GetNetworkMasterKey(ByteArray &aMasterKey);
    MOCKABLE Error SetNetworkMasterKey(const ByteArray &aMasterKey, MilliSeconds aDelay);
    MOCKABLE Error GetNetworkName(std::string &aNetworkName) const;
    MOCKABLE Error SetNetworkName(const std::string &aNetworkName);
    MOCKABLE Error GetPanId(uint16_t &aPanId);
    MOCKABLE Error SetPanId(uint16_t aPanId, MilliSeconds aDelay);
    MOCKABLE Error GetPSKc(ByteArray &aPSKc) const;
    MOCKABLE Error SetPSKc(const ByteArray &aPSKc);

    MOCKABLE Error GetSecurityPolicy(SecurityPolicy &aSecurityPolicy) const;
    MOCKABLE Error SetSecurityPolicy(const SecurityPolicy &aSecurityPolicy);

    // Advanced Active/Pending Operational Dataset APIs exposed for setting with customized user data.
    // Not recommended unless you have good understanding of the encoding of each fields.

    // Always send MGMT_ACTIVE_GET.req.
    MOCKABLE Error GetActiveDataset(ActiveOperationalDataset &aDataset, uint16_t aDatasetFlags);

    // Always send MGMT_ACTIVE_SET.req.
    MOCKABLE Error SetActiveDataset(const ActiveOperationalDataset &aDataset);

    // Always send MGMT_PENDING_GET.req.
    MOCKABLE Error GetPendingDataset(PendingOperationalDataset &aDataset, uint16_t aDatasetFlags);

    // Always send MGMT_PENDING_SET.req.
    MOCKABLE Error SetPendingDataset(const PendingOperationalDataset &aDataset);

    // Network Diagnostic
    MOCKABLE Error CommandDiagGetQuery(const std::string &aAddr, uint64_t aDiagDataFlags);
    MOCKABLE Error CommandDiagGetQuery(uint64_t aPeerAloc16, uint64_t aDiagDataFlags);

    MOCKABLE Error CommandDiagReset(const std::string &aAddr, uint64_t aDiagDataFlags);

    /*
     * BBR Dataset APIs
     */
    MOCKABLE Error GetTriHostname(std::string &aHostname) const;
    MOCKABLE Error SetTriHostname(const std::string &aHostname);
    MOCKABLE Error GetRegistrarHostname(std::string &aHostname) const;
    MOCKABLE Error SetRegistrarHostname(const std::string &aHostname);
    MOCKABLE Error GetRegistrarIpv6Addr(std::string &aIpv6Addr) const;

    // Advanced BBR Dataset APIs exposed for setting with customized user data.
    // Not recommended unless you have good understanding of the encoding of each fields.

    // Always send MGMT_BBR_GET.req.
    MOCKABLE Error GetBbrDataset(BbrDataset &aDataset, uint16_t aDatasetFlags);

    // Always send MGMT_BBR_SET.req.
    MOCKABLE Error SetBbrDataset(const BbrDataset &aDataset);

    /*
     * Commercial Commissioning features
     */

    MOCKABLE Error            Reenroll(const std::string &aDstAddr);
    MOCKABLE Error            DomainReset(const std::string &aDstAddr);
    MOCKABLE Error            Migrate(const std::string &aDstAddr, const std::string &aDesignatedNetwork);
    MOCKABLE const ByteArray &GetToken() const;
    MOCKABLE Error            RequestToken(const std::string &aAddr, uint16_t aPort);
    MOCKABLE Error            SetToken(const ByteArray &aSignedToken);

    MOCKABLE Error RegisterMulticastListener(const std::vector<std::string> &aMulticastAddrList, Seconds aTimeout);
    MOCKABLE Error AnnounceBegin(uint32_t           aChannelMask,
                                 uint8_t            aCount,
                                 MilliSeconds       aPeriod,
                                 const std::string &aDtsAddr);
    MOCKABLE Error PanIdQuery(uint32_t aChannelMask, uint16_t aPanId, const std::string &aDstAddr);
    MOCKABLE bool  HasPanIdConflict(uint16_t aPanId) const;
    MOCKABLE Error EnergyScan(uint32_t           aChannelMask,
                              uint8_t            aCount,
                              uint16_t           aPeriod,
                              uint16_t           aScanDuration,
                              const std::string &aDstAddr);
    MOCKABLE const EnergyReport    *GetEnergyReport(const Address &aDstAddr) const;
    MOCKABLE const EnergyReportMap &GetAllEnergyReports() const;

    const std::string    &GetDomainName() const;
    const DiagAnsDataMap &GetNetDiagTlvs() const;

protected:
    CommissionerApp() = default;

private:
    friend Error CommissionerAppCreate(std::shared_ptr<CommissionerApp> &aCommApp, const Config &aConfig);

    Error Init(const Config &aConfig);

    static Error Create(std::shared_ptr<CommissionerApp> &aCommApp, const Config &aConfig);

    struct JoinerKey
    {
        JoinerType mType;
        ByteArray  mId;

        bool operator<(const JoinerKey &aOther) const;
    };

    CommissionerDataset MakeDefaultCommissionerDataset() const;

    static ByteArray &GetSteeringData(CommissionerDataset &aDataset, JoinerType aJoinerType);
    static uint16_t  &GetJoinerUdpPort(CommissionerDataset &aDataset, JoinerType aJoinerType);

    // Erases all joiner with specific type. Returns the number of erased joiners.
    size_t      EraseAllJoiners(JoinerType aJoinerType);
    static void MergeDataset(ActiveOperationalDataset &aDst, const ActiveOperationalDataset &aSrc);
    static void MergeDataset(PendingOperationalDataset &aDst, const PendingOperationalDataset &aSrc);
    static void MergeDataset(BbrDataset &aDst, const BbrDataset &aSrc);
    static void MergeDataset(CommissionerDataset &aDst, const CommissionerDataset &aSrc);

    static Error ValidatePSKd(const std::string &aPSKd);

    const JoinerInfo *GetJoinerInfo(JoinerType aType, const ByteArray &aJoinerId);

    std::shared_ptr<Commissioner> mCommissioner;

    ByteArray mSignedToken;

private:
    /*
     * Below are data associated with the connected Thread Network.
     */
    std::map<JoinerKey, JoinerInfo> mJoiners;
    std::map<uint16_t, ChannelMask> mPanIdConflicts;
    EnergyReportMap                 mEnergyReports;
    ActiveOperationalDataset        mActiveDataset;
    PendingOperationalDataset       mPendingDataset;
    CommissionerDataset             mCommDataset;
    BbrDataset                      mBbrDataset;
    DiagAnsDataMap                  mDiagAnsDataMap;
};

Error CommissionerAppCreate(std::shared_ptr<CommissionerApp> &aCommApp, const Config &aConfig);

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_APP_COMMISSIONER_APP_HPP_
