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
 *   The file includes definition of Thread Network Data.
 */

#ifndef OT_COMM_NETWORK_DATA_HPP_
#define OT_COMM_NETWORK_DATA_HPP_

#include <string>

#include <stdint.h>

#include <commissioner/defines.hpp>
#include <commissioner/error.hpp>

namespace ot {

namespace commissioner {

// Should them be exposed to user?
static constexpr uint8_t kMlrStatusSuccess     = 0;
static constexpr uint8_t kMlrStatusInvalid     = 2;
static constexpr uint8_t kMlrStatusNoResources = 4;
static constexpr uint8_t kMlrStatusNotPrimary  = 5;
static constexpr uint8_t kMlrStatusFailure     = 6;

/**
 * Extended PAN Id wrapper
 */
struct XpanId
{
    static constexpr uint64_t kEmptyXpanId = 0;

    uint64_t mValue;

    XpanId(uint64_t val);

    XpanId();

    std::string str() const;

    bool operator==(const XpanId &aOther) const;

    bool operator!=(const uint64_t aOther) const;
    bool operator<(const XpanId aOther) const;

    operator std::string() const;

    /**
     * Decodes hexadecimal string.
     */
    Error FromHex(const std::string &aInput);
};

typedef std::vector<XpanId> XpanIdArray;

/**
 * @brief The Commissioner Dataset of the Thread Network Data.
 *
 * Each data field of Commissioner Dataset is optional. The field is
 * meaningful only when associative PresentFlags is included in
 * `mPresentFlags`.
 */
struct CommissionerDataset
{
    /**
     * The RLOC16 of the Border Agent.
     * Read only, always present for a read operation.
     * Ignored by a write operation.
     */
    uint16_t mBorderAgentLocator = 0;

    /**
     * The Commissioner Session ID.
     * Read only, always present for a read operation.
     * Ignored by a write operation.
     */
    uint16_t mSessionId = 0;

    /**
     * @brief The MeshCoP Steering Data.
     *
     * This contains the Bloom Filter as provided by the Commissioner,
     * and specified in Section 8.4.4.3, to signal which set of
     * Joiner Identifiers (Joiner ID) are permitted to join.
     */
    ByteArray mSteeringData;

    /**
     * @brief The AE Steering Data.
     *
     * Controls which joiner is allowed for CCM Autonomous Enrollment.
     * Defined for only CCM network.
     */
    ByteArray mAeSteeringData;

    /**
     * @brief The NMKP Steering Data.
     *
     * Controls which joiner is allowed for CCM Network Masterkey Provisioning.
     * Defined for only CCM network.
     */
    ByteArray mNmkpSteeringData;

    /**
     * @brief The MeshCoP Joiner UDP Port.
     *
     * Used by a 1.1 non-CCM joiner to perform MeshCoP joining.
     */
    uint16_t mJoinerUdpPort = 0;

    /**
     * @brief The AE UDP Port.
     *
     * Used by a CCM joiner to perform AE joining.
     * Defined for only CCM network.
     */
    uint16_t mAeUdpPort = 0;

    /**
     * @brief The NMKP UDP Port.
     *
     * Used by a CCM joiner to perform CCM NMKP.
     * Defined for only CCM network.
     */
    uint16_t mNmkpUdpPort = 0;

    /**
     * Indicates which fields are included in the dataset.
     */
    uint16_t mPresentFlags = 0;

    static constexpr uint16_t kBorderAgentLocatorBit = (1 << 15);
    static constexpr uint16_t kSessionIdBit          = (1 << 14);
    static constexpr uint16_t kSteeringDataBit       = (1 << 13);
    static constexpr uint16_t kAeSteeringDataBit     = (1 << 12);
    static constexpr uint16_t kNmkpSteeringDataBit   = (1 << 11);
    static constexpr uint16_t kJoinerUdpPortBit      = (1 << 10);
    static constexpr uint16_t kAeUdpPortBit          = (1 << 9);
    static constexpr uint16_t kNmkpUdpPortBit        = (1 << 8);
};

/**
 * Timestamp of Operational Dataset.
 */
struct Timestamp
{
    uint64_t mSeconds : 48;
    uint64_t mTicks : 15;
    uint64_t mU : 1;

    /**
     * Get current timestamp.
     */
    static Timestamp Cur();

    /**
     * Decode timestamp from an uint64_t integer.
     */
    static Timestamp Decode(uint64_t aValue);

    /**
     * Encode to an uint64_t integer.
     */
    uint64_t Encode() const;
};

#ifndef SWIG
static_assert(sizeof(Timestamp) == sizeof(uint64_t), "wrong timestamp size");
#endif

/**
 * A Channel includes ChannelPage and ChannelNumber.
 */
struct Channel
{
    uint8_t  mPage;
    uint16_t mNumber;
};

/**
 * A ChannelMaskEntry includes ChannelPage and ChannelMasks.
 */
struct ChannelMaskEntry
{
    uint8_t   mPage;
    ByteArray mMasks;
};

using ChannelMask = std::vector<ChannelMaskEntry>;

/**
 * A SecurityPolicy includes RotationTime and SecurityFlags.
 */
struct SecurityPolicy
{
    uint16_t  mRotationTime; ///< Rotation time in hours.
    ByteArray mFlags;        ///< Security flags.
};

/**
 * Mask bit constants originate from the Spec pt. 8.10.1.15
 */
enum SecurityPolicyFlags
{
    // Byte[0]
    kSecurityPolicyBit_O   = 1 << 0, /// out-of-band commissioning enabled
    kSecurityPolicyBit_N   = 1 << 1, /// native commissioning using PSKc allowed
    kSecurityPolicyBit_R   = 1 << 2, /// Thread 1.1.x Routers enabled
    kSecurityPolicyBit_C   = 1 << 3, /// external commissioning using PSKc allowed
    kSecurityPolicyBit_B   = 1 << 4, /// Thread 1.1.x Beacons enabled
    kSecurityPolicyBit_CCM = 1 << 5, /// Commercial Commissioning Mode disabled
    kSecurityPolicyBit_AE  = 1 << 6, /// Autonomous Enrollment disabled
    kSecurityPolicyBit_NMP = 1 << 7, /// Network Master-key Provisioning disabled

    // Byte[1]
    kSecurityPolicyBit_L   = 1 << 0, /// ToBLE Link enabled
    kSecurityPolicyBit_NCR = 1 << 1, /// non-CCM Routers disabled in the CCM network

    kSecurityPolicyMask_Rsv = 1 << 2 | 1 << 3 | 1 << 4, /// Reserved bits
    kSecurityPolicyMask_VR  = 1 << 5 | 1 << 6 | 1 << 7, /// Protocol version
};

/**
 * A PAN identifier.
 */
struct PanId
{
    static constexpr uint64_t kEmptyPanId = 0;

    uint16_t mValue;
    PanId(uint16_t aValue);
    PanId();

    PanId &operator=(uint16_t aValue);
           operator uint16_t() const;
           operator std::string() const;

    Error FromHex(const std::string &aInput);
};

/**
 * @brief The Active Operational Dataset of the Thread Network Data.
 *
 * Each data field except `mActiveTimestamp` is optional. The field is
 * meaningful only when associative PresentFlags is included in
 * `mPresentFlags`.
 *
 * @note For a write operation, `mChannel`, `mPanId`, `mMeshLocalPrefix`
 *       and `mNetworkMasterKey` must be excluded. Otherwise, it will be
 *       rejected.
 */
struct ActiveOperationalDataset
{
    Timestamp      mActiveTimestamp;
    Channel        mChannel;
    ChannelMask    mChannelMask;
    XpanId         mExtendedPanId;
    ByteArray      mMeshLocalPrefix;
    ByteArray      mNetworkMasterKey;
    std::string    mNetworkName;
    PanId          mPanId;
    ByteArray      mPSKc;
    SecurityPolicy mSecurityPolicy;

    /**
     * Indicates which fields are included in the dataset.
     */
    uint16_t mPresentFlags;

    static constexpr uint16_t kActiveTimestampBit  = (1 << 15);
    static constexpr uint16_t kChannelBit          = (1 << 14);
    static constexpr uint16_t kChannelMaskBit      = (1 << 13);
    static constexpr uint16_t kExtendedPanIdBit    = (1 << 12);
    static constexpr uint16_t kMeshLocalPrefixBit  = (1 << 11);
    static constexpr uint16_t kNetworkMasterKeyBit = (1 << 10);
    static constexpr uint16_t kNetworkNameBit      = (1 << 9);
    static constexpr uint16_t kPanIdBit            = (1 << 8);
    static constexpr uint16_t kPSKcBit             = (1 << 7);
    static constexpr uint16_t kSecurityPolicyBit   = (1 << 6);

    ActiveOperationalDataset();
};

/**
 * @brief The Pending Operational Dataset of the Thread Network Data.
 *
 * `mDelayTimer` and `mPendingTimestamp` are both mandatory.
 * The field is meaningful only when associative PresentFlags
 * is included in `mPresentFlags`.
 */
struct PendingOperationalDataset : ActiveOperationalDataset
{
    uint32_t  mDelayTimer; ///< Delay timer in milliseconds.
    Timestamp mPendingTimestamp;

    static constexpr uint16_t kDelayTimerBit       = (1 << 5);
    static constexpr uint16_t kPendingTimestampBit = (1 << 4);

    PendingOperationalDataset();
};

/**
 * @brief The Backbone Router Dataset.
 *
 * `mDelayTimer` and `mPendingTimestamp` are both mandatory.
 * The field is meaningful only when associative PresentFlags
 * is included in `mPresentFlags`.
 */
struct BbrDataset
{
    std::string mTriHostname;
    std::string mRegistrarHostname;
    std::string mRegistrarIpv6Addr; ///< Read only.

    /**
     * Indicates which fields are included in the dataset.
     */
    uint16_t mPresentFlags = 0;

    static constexpr uint16_t kTriHostnameBit       = (1 << 15);
    static constexpr uint16_t kRegistrarHostnameBit = (1 << 14);
    static constexpr uint16_t kRegistrarIpv6AddrBit = (1 << 13);
};

/**
 * @brief Parsing IPv6 prefix into raw byte array.
 *
 * This function parses an IPv6 prefix string which is followed by
 * a slash ('/') and prefix length.
 * For example, "2002::/16" will be parsed into [0x20, 0x02].
 *
 * @param[out] aPrefix  A prefix in raw byte array.
 * @param[in]  aStr     A prefix string.
 *
 * @return Error::kNone, succeed; Otherwise, failed.
 */
Error Ipv6PrefixFromString(ByteArray &aPrefix, const std::string &aStr);

/**
 * @brief Encoding IPv6 prefix into string.
 *
 * @param[in] aPrefix  A prefix in raw byte array.
 * @return The encoded Ipv6 prefix string.
 */
std::string Ipv6PrefixToString(ByteArray aPrefix);

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_NETWORK_DATA_HPP_
