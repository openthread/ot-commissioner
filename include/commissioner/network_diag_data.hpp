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

#include <cstddef>
#include <cstdint>
#include <stdbool.h>
#include <string>
#include <vector>

#include "defines.hpp"
#include "error.hpp"

namespace ot {

namespace commissioner {

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

    /**
     * Returns the size of the ChildTable.
     */
    size_t GetSize() const;

    /**
     * Returns the ChildEntry at the given index.
     */
    ChildEntry GetChildEntry(size_t aIndex) const;
};

/**
 * @brief IPv6 Address TLV
 */
struct Ipv6Address
{
    ByteArray mAddress;
    /**
     * Decodes Ipv6Address from a ByteArray
     */
    static Error Decode(Ipv6Address &aIpv6Address, const ByteArray &aBuf);

    /**
     * Returns a string representation of the Ipv6Address.
     */
    std::string ToString() const;
};

/**
 * @brief IPv6 Address TLV
 */
struct Ipv6AddressList
{
    std::vector<Ipv6Address> mIpv6Addresses;

    /**
     * Returns the size of the Ipv6AddressList.
     */
    size_t GetSize() const;

    /**
     * Returns the Ipv6Address at the given index.
     */
    Ipv6Address GetIpv6Address(size_t aIndex) const;

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
    uint8_t mRouterId            = 0;
    uint8_t mOutgoingLinkQuality = 0;
    uint8_t mIncomingLinkQuality = 0;
    uint8_t mRouteCost           = 0;
    /**
     * Decodes RouteDataEntry from a ByteArray
     */
    static void Decode(RouteDataEntry &aRouteDataEntry, uint8_t aBuf);
};

/**
 * @brief Route64 TLV
 */
struct Route64
{
    uint8_t                     mIdSequence = 0;
    ByteArray                   mMask;
    std::vector<RouteDataEntry> mRouteData;
    /**
     * Decodes Route64 from a ByteArray
     */
    static Error Decode(Route64 &aRoute64, const ByteArray &aBuf);

    /**
     * Extracts router IDs from a router ID mask.
     */
    static ByteArray ExtractRouterIds(const ByteArray &aMask);

    /**
     * Returns a string representation of the Route64.
     */
    std::string ToString() const;

    /**
     * Returns the size of the RouteData.
     */
    size_t GetRouteDataSize() const;

    /**
     * Returns the RouteDataEntry at the given index.
     */
    RouteDataEntry GetRouteData(size_t aIndex) const;
};

struct ChildIpv6AddressList
{
    uint16_t        mRloc16 = 0;
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
    bool      mRxOnWhenIdleFlag     = false;
    bool      mFullThreadDeviceFlag = false;
    bool      mFullNetworkDataFlag  = false;
    bool      mCslFlag              = false;
    bool      mErrorRateFlag        = false;
    uint16_t  mRloc16               = 0;
    ByteArray mExtMacAddress;
    uint16_t  mThreadVersion       = 0;
    uint32_t  mTimeout             = 0;
    uint32_t  mAge                 = 0;
    uint32_t  mConnectionTime      = 0;
    uint16_t  mSupervisionInterval = 0;
    uint8_t   mLinkMargin          = 0;
    uint8_t   mAverageRssi         = 0;
    uint8_t   mLastRssi            = 0;
    uint16_t  mFrameErrorRate      = 0;
    uint16_t  mMessageErrorRate    = 0;
    uint16_t  mQueuedMessageCount  = 0;
    uint16_t  mCslPeriod           = 0;
    uint32_t  mCslTimeout          = 0;
    uint8_t   mCslChannel          = 0;
};

struct MacCounters
{
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

    static constexpr uint64_t kExtMacAddressBit    = (1ull << 63);
    static constexpr uint64_t kMacAddressBit       = (1ull << 62);
    static constexpr uint64_t kModeBit             = (1ull << 61);
    static constexpr uint64_t kRoute64Bit          = (1ull << 60);
    static constexpr uint64_t kLeaderDataBit       = (1ull << 59);
    static constexpr uint64_t kIpv6AddressBit      = (1ull << 58);
    static constexpr uint64_t kChildTableBit       = (1ull << 57);
    static constexpr uint64_t kEui64Bit            = (1ull << 56);
    static constexpr uint64_t kTlvTypeBit          = (1ull << 55);
    static constexpr uint64_t kMacCountersBit      = (1ull << 54);
    static constexpr uint64_t kChildIpv6AddressBit = (1ull << 53);
};

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_NETWORK_DIAG_TLVS_HPP_