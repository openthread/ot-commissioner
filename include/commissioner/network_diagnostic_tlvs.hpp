/*
 *    Copyright (c) 2024, The OpenThread Commissioner Authors.
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
 *   This file defines the types of Thread Network Diagnostic TLVs used for network diagnostics.
 */

#ifndef OT_COMM_NETWORK_DIAG_TLVS_HPP_
#define OT_COMM_NETWORK_DIAG_TLVS_HPP_

#include <cstdint>
#include <stdbool.h>
#include <string>
#include <vector>

#include "defines.hpp"
#include "error.hpp"

namespace ot {

namespace commissioner {

/**
 * @brief Enum representing the types of Network Diagnostic TLVs.
 */
enum class NetworkDiagTlvType : uint8_t
{
    kNetworkDiagExtMacAddress           = 0,  ///< Extended MAC Address TLV
    kNetworkDiagMacAddress              = 1,  ///< MAC Address TLV
    kNetworkDiagMode                    = 2,  ///< Mode TLV
    kNetworkDiagTimeout                 = 3,  ///< Timeout TLV
    kNetworkDiagConnectivity            = 4,  ///< Connectivity TLV
    kNetworkDiagRoute64                 = 5,  ///< Route64 TLV
    kNetworkDiagLeaderData              = 6,  ///< Leader Data TLV
    kNetworkDiagNetworkData             = 7,  ///< Network Data TLV
    kNetworkDiagIpv6Address             = 8,  ///< IPv6 Address TLV
    kNetworkDiagMacCounters             = 9,  ///< MAC Counters TLV
    kNetworkDiagBatteryLevel            = 14, ///< Battery Level TLV
    kNetworkDiagSupplyVoltage           = 15, ///< Supply Voltage TLV
    kNetworkDiagChildTable              = 16, ///< Child Table TLV
    kNetworkDiagChannelPages            = 17, ///< Channel Pages TLV
    kNetworkDiagTypeList                = 18, ///< Type List TLV
    kNetworkDiagMaxChildTimeout         = 19, ///< Max Child Timeout TLV
    kNetworkDiagLDevIDSubjectPubKeyInfo = 20, ///< LDevID Subject Public Key Info TLV
    kNetworkDiagIDevIDCert              = 21, ///< IDevID Certificate TLV
    kNetworkDiagEui64                   = 23, ///< EUI-64 TLV
    kNetworkDiagVersion                 = 24, ///< Version TLV
    kNetworkDiagVendorName              = 25, ///< Vendor Name TLV
    kNetworkDiagVendorModel             = 26, ///< Vendor Model TLV
    kNetworkDiagVendorSWVersion         = 27, ///< Vendor Software Version TLV
    kNetworkDiagThreadStackVersion      = 28, ///< Thread Stack Version TLV
    kNetworkDiagChild                   = 29, ///< Child TLV
    kNetworkDiagChildIpv6Address        = 30, ///< Child IPv6 Address TLV
    kNetworkDiagRouterNeighbor          = 31, ///< Router Neighbor TLV
    kNetworkDiagAnswer                  = 32, ///< Answer TLV
    kNetworkDiagQueryID                 = 33, ///< Query ID TLV
    kNetworkDiagMleCounters             = 34  ///< MLE Counters TLV
};

/**
 * @brief Mode TLV
 */
struct Mode
{
    bool mIsRxOnWhenIdleMode          = false;
    bool mIsMtd                       = false;
    bool mIsStableNetworkDataRequired = false;

    /**
     * Decodes Mode from a ByteArray
     */
    static void Decode(Mode &aMode, uint8_t aBuf);

    /**
     * Returns a string representation of the Mode.
     */
    std::string ToString() const;
};

/**
 * @brief Child Entry in Child Table TLV
 */
struct ChildEntry
{
    uint8_t mTimeout             = 0;
    uint8_t mIncomingLinkQuality = 0;
    uint8_t mChildId             = 0;
    Mode    mModeData;

    /**
     * Decodes ChildEntry from a ByteArray
     */
    static Error Decode(ChildEntry &aChildEntry, const ByteArray &aBuf);

    /**
     * Returns a string representation of the ChildEntry.
     */
    std::string ToString() const;
};

/**
 * @brief Child Table TLV
 */
struct ChildTable
{
    std::vector<ChildEntry> mChildEntries;
    /**
     * Decodes ChildTable from a ByteArray
     */
    static Error Decode(ChildTable &aChildTable, const ByteArray &aBuf);

    /**
     * Returns a string representation of the ChildTable.
     */
    std::string ToString() const;
};

/**
 * @brief IPv6 Address TLV
 */
struct Ipv6AddressList
{
    std::vector<ByteArray> mIpv6Addresses;
    /**
     * Decodes Ipv6Address from a ByteArray
     */
    static Error Decode(Ipv6AddressList &aIpv6Address, const ByteArray &aBuf);

    /**
     * Returns a string representation of the Ipv6Address.
     */
    std::string ToString() const;
};

/**
 * @brief Leader Data TLV
 */
struct LeaderData
{
    uint32_t mPartitionId       = 0;
    uint8_t  mWeighting         = 0;
    uint8_t  mDataVersion       = 0;
    uint8_t  mStableDataVersion = 0;
    uint8_t  mRouterId          = 0;

    /**
     * Decodes LeaderData from a ByteArray
     */
    static Error Decode(LeaderData &aLeaderData, const ByteArray &aBuf);

    /**
     * Returns a string representation of the LeaderData.
     */
    std::string ToString() const;
};

/**
 * @brief Route Data Entry of RouteData in Route64 TLV
 */
struct RouteDataEntry
{
    uint8_t mRouterId             = 0;
    uint8_t mOutgoingLinkQuality  = 0;
    uint8_t mIncommingLinkQuality = 0;
    uint8_t mRouteCost            = 0;
    /**
     * Decodes RouteDataEntry from a ByteArray
     */
    static void Decode(RouteDataEntry &aRouteDataEntry, uint8_t aBuf);
};

/**
 * @brief Route Data in Route64 TLV
 */
using RouteData = std::vector<RouteDataEntry>;

/**
 * @brief Route64 TLV
 */
struct Route64
{
    uint8_t   mIdSequence = 0;
    ByteArray mMask;
    RouteData mRouteData;
    /**
     * Decodes Route64 from a ByteArray
     */
    static Error Decode(Route64 &aRoute64, const ByteArray &aBuf);

    /**
     * Extracts router IDs from a router ID mask.
     */
    static ByteArray ExtractRouterIds(const ByteArray& aMask);

    /**
     * Returns a string representation of the Route64.
     */
    std::string ToString() const;
};

struct ChildIpv6AddressList
{
    uint16_t mRloc16 = 0;
    Ipv6AddressList mIpv6AddressList;

    /**
     * Decodes ChildIpv6Address from a ByteArray
     */
    static Error Decode(ChildIpv6AddressList &aChildIpv6AddressList, const ByteArray &aBuf);

    /**
     * Returns a string representation of the ChildIpv6Address.
     */
    std::string ToString() const;
};

struct Child
{
    bool       mRxOnWhenIdleFlag              = false;
    bool       mFullThreadDeviceFlag          = false;
    bool       mFullNetworkDataFlag           = false;
    bool       mCslFlag                       = false;
    bool       mErrorRateFlag                 = false;
    uint16_t   mRloc16                        = 0;
    ByteArray  mExtMacAddress;
    uint16_t   mThreadVersion                 = 0;
    uint32_t   mTimeout                       = 0;
    uint32_t   mAge                           = 0;
    uint32_t   mConnectionTime                = 0;
    uint16_t   mSupervisionInterval           = 0;
    uint8_t    mLinkMargin                    = 0;
    uint8_t    mAverageRssi                   = 0;
    uint8_t    mLastRssi                      = 0;
    uint16_t   mFrameErrorRate                = 0;
    uint16_t   mMessageErrorRate              = 0;
    uint16_t   mQueuedMessageCount            = 0;
    uint16_t   mCslPeriod                     = 0;
    uint32_t   mCslTimeout                    = 0;
    uint8_t    mCslChannel                    = 0;
};

struct MacCounters {
    uint32_t mIfInUnknownProtos  = 0;
    uint32_t mIfInErrors         = 0;
    uint32_t mIfOutErrors        = 0;
    uint32_t mIfInUcastPkts      = 0;
    uint32_t mIfInBroadcastPkts  = 0;
    uint32_t mIfInDiscards       = 0;
    uint32_t mIfOutUcastPkts     = 0;
    uint32_t mIfOutBroadcastPkts = 0;
    uint32_t mIfOutDiscards      = 0;

    /**
     * Decodes MacCounters from a ByteArray
     */
    static Error Decode(MacCounters &aMacCounters, const ByteArray &aBuf);

    /**
     * Returns a string representation of the MacCounters.
     */
    std::string ToString() const;
};

/**
 * @brief network diagnostic TLVs in TMF
 *
 * Each data field of Diagnostic TLVs is optional. The field is
 * meaningful only when associative PresentFlags is included in
 * `mPresentFlags`.
 */
struct NetDiagTlvs
{
    ByteArray            mExtMacAddress;
    uint16_t             mMacAddress = 0;
    Mode                 mMode;
    Route64              mRoute64;
    LeaderData           mLeaderData;
    Ipv6AddressList      mIpv6Addresses;
    ChildTable           mChildTable;
    ByteArray            mEui64;
    ByteArray            mTlvTypeList;
    ChildIpv6AddressList mChildIpv6AddressList;
    MacCounters          mMacCounters;

    /**
     * Indicates which fields are included in the dataset.
     */
    uint64_t mPresentFlags;

    static constexpr uint64_t kExtMacAddressBit =
        (1ull << static_cast<uint8_t>(NetworkDiagTlvType::kNetworkDiagExtMacAddress));
    static constexpr uint64_t kMacAddressBit =
        (1ull << static_cast<uint8_t>(NetworkDiagTlvType::kNetworkDiagMacAddress));
    static constexpr uint64_t kModeBit    = (1ull << static_cast<uint8_t>(NetworkDiagTlvType::kNetworkDiagMode));
    static constexpr uint64_t kRoute64Bit = (1ull << static_cast<uint8_t>(NetworkDiagTlvType::kNetworkDiagRoute64));
    static constexpr uint64_t kLeaderDataBit =
        (1ull << static_cast<uint8_t>(NetworkDiagTlvType::kNetworkDiagLeaderData));
    static constexpr uint64_t kIpv6AddressBit =
        (1ull << static_cast<uint8_t>(NetworkDiagTlvType::kNetworkDiagIpv6Address));
    static constexpr uint64_t kChildTableBit =
        (1ull << static_cast<uint8_t>(NetworkDiagTlvType::kNetworkDiagChildTable));
    static constexpr uint64_t kEui64Bit   = (1ull << static_cast<uint8_t>(NetworkDiagTlvType::kNetworkDiagEui64));
    static constexpr uint64_t kTlvTypeBit = (1ull << static_cast<uint8_t>(NetworkDiagTlvType::kNetworkDiagTypeList));
    static constexpr uint64_t kMacCountersBit = (1ull << static_cast<uint8_t>(NetworkDiagTlvType::kNetworkDiagMacCounters));
    static constexpr uint64_t kChildIpv6AddressBit =
        (1ull << static_cast<uint8_t>(NetworkDiagTlvType::kNetworkDiagChildIpv6Address));
};

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_NETWORK_DIAG_TLVS_HPP_
