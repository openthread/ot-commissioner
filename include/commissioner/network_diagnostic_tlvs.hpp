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
#include <vector>

#include "defines.hpp"
#include "common/address.hpp"

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
};

/**
 * @brief Child Table TLV
 */
using ChildTable = std::vector<ChildEntry>;

/**
 * @brief IPv6 Address TLV
 */
using Ipv6Address = std::vector<Address>;

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
};

/**
 * @brief Route Data Entry of RouteData in Route64 TLV
 */
struct RouteDataEntry
{
    uint8_t mOutgoingLinkQuality  = 0;
    uint8_t mIncommingLinkQuality = 0;
    uint8_t mRouteCost            = 0;
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
    ByteArray   mExtMacAddress;
    uint16_t    mMacAddress = 0;
    Mode        mMode;
    Route64     mRoute64;
    LeaderData  mLeaderData;
    Ipv6Address mIpv6Addresses;
    ChildTable  mChildTable;
    ByteArray   mEui64;
    ByteArray   mTlvTypeList;

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
};

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_NETWORK_DIAG_TLVS_HPP_
