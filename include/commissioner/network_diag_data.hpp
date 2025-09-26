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

#ifndef OT_COMM_NETWORK_DIAG_DATA_HPP_
#define OT_COMM_NETWORK_DIAG_DATA_HPP_

#include <cstddef>
#include <cstdint>
#include <stdbool.h>
#include <string>
#include <vector>

#include "defines.hpp"
#include "error.hpp"
#include "network_data.hpp"

namespace ot {

namespace commissioner {

/**
 * @brief Mode Data
 */
struct ModeData
{
    bool mIsRxOnWhenIdleMode          = false;
    bool mIsMtd                       = false;
    bool mIsStableNetworkDataRequired = false;
};

/**
 * @brief Child Entry in Child Table
 */
struct ChildTableEntry
{
    uint32_t mTimeout             = 0;
    uint8_t  mIncomingLinkQuality = 0;
    uint16_t mChildId             = 0;
    ModeData mModeData;
};

/**
 * @brief Leader Data
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
 * @brief Route Data Entry of RouteData in Route64
 */
struct RouteDataEntry
{
    uint8_t mRouterId            = 0;
    uint8_t mOutgoingLinkQuality = 0;
    uint8_t mIncomingLinkQuality = 0;
    uint8_t mRouteCost           = 0;
};

/**
 * @brief Route64 Data
 */
struct Route64
{
    uint8_t                     mIdSequence = 0;
    ByteArray                   mMask;
    std::vector<RouteDataEntry> mRouteData;
};

/**
 * @brief Child IPv6 Address Info
 */
struct ChildIpv6AddrInfo
{
    uint16_t                 mRloc16  = 0;
    uint16_t                 mChildId = 0;
    std::vector<std::string> mAddrs;
};

/**
 * @brief MAC Counters
 */
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
};

/**
 * @brief Connectivity.
 *
 */
struct Connectivity
{
    static constexpr uint16_t kRxOffChildBufferSizeBit    = 1 << 0;
    static constexpr uint16_t kRxOffChildDatagramCountBit = 1 << 1;

    uint16_t mRxOffChildBufferSize    = 0;
    uint8_t  mPresentFlags            = 0;
    int8_t   mParentPriority          = 0;
    uint8_t  mLinkQuality1            = 0;
    uint8_t  mLinkQuality2            = 0;
    uint8_t  mLinkQuality3            = 0;
    uint8_t  mLeaderCost              = 0;
    uint8_t  mIdSequence              = 0;
    uint8_t  mActiveRouters           = 0;
    uint8_t  mRxOffChildDatagramCount = 0;
};

/**
 * @brief Child TLV Data
 */
struct Child
{
    ByteArray mExtAddress;
    uint32_t  mTimeout             = 0;
    uint32_t  mAge                 = 0;
    uint32_t  mConnectionTime      = 0;
    uint32_t  mCslTimeout          = 0;
    uint16_t  mRloc16              = 0;
    uint16_t  mSupervisionInterval = 0;
    uint16_t  mThreadVersion       = 0;
    uint16_t  mFrameErrorRate      = 0;
    uint16_t  mMessageErrorRate    = 0;
    uint16_t  mQueuedMessageCount  = 0;
    uint16_t  mCslPeriod           = 0;
    uint8_t   mLinkMargin          = 0;
    uint8_t   mCslChannel          = 0;
    int8_t    mAverageRssi         = 127;
    int8_t    mLastRssi            = 127;
    bool      mIsRxOnWhenIdle      = false;
    bool      mIsDeviceTypeMtd     = false;
    bool      mHasNetworkData      = false;
    bool      mSupportsCsl         = false;
    bool      mSupportsErrorRates  = false;
};

/**
 * @brief Router Neighbor TLV Data
 *
 */
struct RouterNeighbor
{
    ByteArray mExtAddress;
    uint32_t  mConnectionTime     = 0;
    uint16_t  mRloc16             = 0;
    uint16_t  mThreadVersion      = 0;
    uint16_t  mFrameErrorRate     = 0;
    uint16_t  mMessageErrorRate   = 0;
    uint8_t   mLinkMargin         = 0;
    int8_t    mAverageRssi        = 127;
    int8_t    mLastRssi           = 127;
    bool      mSupportsErrorRates = false;
};

/**
 * @brief MLE Counters TLV Data
 *
 */
struct MleCounters
{
    uint64_t mTotalTrackingTime                    = 0;
    uint64_t mRadioDisabledTime                    = 0;
    uint64_t mDetachedRoleTime                     = 0;
    uint64_t mChildRoleTime                        = 0;
    uint64_t mRouterRoleTime                       = 0;
    uint64_t mLeaderRoleTime                       = 0;
    uint16_t mRadioDisabledCounter                 = 0;
    uint16_t mDetachedRoleCounter                  = 0;
    uint16_t mChildRoleCounter                     = 0;
    uint16_t mRouterRoleCounter                    = 0;
    uint16_t mLeaderRoleCounter                    = 0;
    uint16_t mAttachAttemptsCounter                = 0;
    uint16_t mPartitionIdChangesCounter            = 0;
    uint16_t mBetterPartitionAttachAttemptsCounter = 0;
    uint16_t mNewParentCounter                     = 0;
};

/**
 * @brief network diagnostic data in TMF
 *
 * Each data field of Diagnostic TLVs is optional. The field is
 * meaningful only when associative PresentFlags is included in
 * `mPresentFlags`.
 */
struct NetDiagData
{
    ByteArray                      mEui64;
    ByteArray                      mExtMacAddr;
    ByteArray                      mChannelPages;
    ByteArray                      mTypeList;
    uint32_t                       mMaxChildTimeout = 0;
    uint32_t                       mTimeout         = 0;
    uint16_t                       mMacAddr         = 0;
    uint16_t                       mSupplyVoltage   = 0;
    uint16_t                       mVersion         = 0;
    uint8_t                        mBatteryLevel    = 0;
    std::string                    mVendorName;
    std::string                    mVendorModel;
    std::string                    mVendorSWVersion;
    std::string                    mThreadStackVersion;
    std::string                    mVendorAppURL;
    std::vector<std::string>       mAddrs;
    std::vector<ChildTableEntry>   mChildTable;
    std::vector<ChildIpv6AddrInfo> mChildIpv6AddrsInfoList;
    std::vector<Child>             mChild;
    std::vector<RouterNeighbor>    mRouterNeighbor;
    Route64                        mRoute64;
    LeaderData                     mLeaderData;
    MacCounters                    mMacCounters;
    ModeData                       mMode;
    NetworkData                    mNetworkData;
    Connectivity                   mConnectivity;
    MleCounters                    mMleCounters;
    ChannelMask                    mNonPreferredChannelsMask;

    /**
     * Indicates which fields are included in the object.
     */
    uint64_t mPresentFlags = 0;

    static constexpr uint64_t kExtMacAddrBit               = (1ull << 0);
    static constexpr uint64_t kMacAddrBit                  = (1ull << 1);
    static constexpr uint64_t kModeBit                     = (1ull << 2);
    static constexpr uint64_t kRoute64Bit                  = (1ull << 3);
    static constexpr uint64_t kLeaderDataBit               = (1ull << 4);
    static constexpr uint64_t kAddrsBit                    = (1ull << 5);
    static constexpr uint64_t kChildTableBit               = (1ull << 6);
    static constexpr uint64_t kEui64Bit                    = (1ull << 7);
    static constexpr uint64_t kMacCountersBit              = (1ull << 8);
    static constexpr uint64_t kChildIpv6AddrsInfoListBit   = (1ull << 9);
    static constexpr uint64_t kNetworkDataBit              = (1ull << 10);
    static constexpr uint64_t kTimeoutBit                  = (1ull << 11);
    static constexpr uint64_t kConnectivityBit             = (1ull << 12);
    static constexpr uint64_t kBatteryLevelBit             = (1ull << 13);
    static constexpr uint64_t kSupplyVoltageBit            = (1ull << 14);
    static constexpr uint64_t kChannelPagesBit             = (1ull << 15);
    static constexpr uint64_t kTypeListBit                 = (1ull << 16);
    static constexpr uint64_t kMaxChildTimeoutBit          = (1ull << 17);
    static constexpr uint64_t kVersionBit                  = (1ull << 18);
    static constexpr uint64_t kVendorNameBit               = (1ull << 19);
    static constexpr uint64_t kVendorModelBit              = (1ull << 20);
    static constexpr uint64_t kVendorSWVersionBit          = (1ull << 21);
    static constexpr uint64_t kThreadStackVersionBit       = (1ull << 22);
    static constexpr uint64_t kChildBit                    = (1ull << 23);
    static constexpr uint64_t kRouterNeighborBit           = (1ull << 24);
    static constexpr uint64_t kMleCountersBit              = (1ull << 25);
    static constexpr uint64_t kVendorAppURLBit             = (1ull << 26);
    static constexpr uint64_t kNonPreferredChannelsMaskBit = (1ull << 27);
};

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_NETWORK_DIAG_DATA_HPP_
