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
 *   This file implements Thread Network Diagnostic Data.
 */

#include "commissioner/network_diag_data.hpp"

namespace ot {

namespace commissioner {

constexpr uint64_t NetDiagData::kExtMacAddrBit;
constexpr uint64_t NetDiagData::kMacAddrBit;
constexpr uint64_t NetDiagData::kModeBit;
constexpr uint64_t NetDiagData::kRoute64Bit;
constexpr uint64_t NetDiagData::kLeaderDataBit;
constexpr uint64_t NetDiagData::kAddrsBit;
constexpr uint64_t NetDiagData::kChildTableBit;
constexpr uint64_t NetDiagData::kEui64Bit;
constexpr uint64_t NetDiagData::kMacCountersBit;
constexpr uint64_t NetDiagData::kChildIpv6AddrsInfoListBit;
constexpr uint64_t NetDiagData::kNetworkDataBit;
constexpr uint64_t NetDiagData::kTimeoutBit;
constexpr uint64_t NetDiagData::kConnectivityBit;
constexpr uint64_t NetDiagData::kBatteryLevelBit;
constexpr uint64_t NetDiagData::kSupplyVoltageBit;
constexpr uint64_t NetDiagData::kChannelPagesBit;
constexpr uint64_t NetDiagData::kTypeListBit;
constexpr uint64_t NetDiagData::kMaxChildTimeoutBit;
constexpr uint64_t NetDiagData::kVersionBit;
constexpr uint64_t NetDiagData::kVendorNameBit;
constexpr uint64_t NetDiagData::kVendorModelBit;
constexpr uint64_t NetDiagData::kVendorSWVersionBit;
constexpr uint64_t NetDiagData::kThreadStackVersionBit;
constexpr uint64_t NetDiagData::kChildBit;
constexpr uint64_t NetDiagData::kRouterNeighborBit;
constexpr uint64_t NetDiagData::kMleCountersBit;
constexpr uint64_t NetDiagData::kVendorAppURLBit;
constexpr uint64_t NetDiagData::kNonPreferredChannelsMaskBit;
constexpr uint64_t NetDiagData::kAnswerBit;
constexpr uint64_t NetDiagData::kQueryIdBit;

void MergeNetDiagData(NetDiagData &aDst, const NetDiagData &aSrc)
{
    aDst.mPresentFlags |= aSrc.mPresentFlags;

#define MERGE_FIELD(bit, field)     \
    if (aSrc.mPresentFlags & (bit)) \
    {                               \
        aDst.field = aSrc.field;    \
    }

#define MERGE_VECTOR_FIELD(bit, field)                                             \
    if (aSrc.mPresentFlags & (bit))                                                \
    {                                                                              \
        aDst.field.insert(aDst.field.end(), aSrc.field.begin(), aSrc.field.end()); \
    }

    MERGE_FIELD(NetDiagData::kExtMacAddrBit, mExtMacAddr);
    MERGE_FIELD(NetDiagData::kMacAddrBit, mMacAddr);
    MERGE_FIELD(NetDiagData::kModeBit, mMode);
    MERGE_FIELD(NetDiagData::kRoute64Bit, mRoute64);
    MERGE_FIELD(NetDiagData::kLeaderDataBit, mLeaderData);
    MERGE_VECTOR_FIELD(NetDiagData::kAddrsBit, mAddrs);
    MERGE_VECTOR_FIELD(NetDiagData::kChildTableBit, mChildTable);
    MERGE_FIELD(NetDiagData::kEui64Bit, mEui64);
    MERGE_FIELD(NetDiagData::kMacCountersBit, mMacCounters);
    MERGE_VECTOR_FIELD(NetDiagData::kChildIpv6AddrsInfoListBit, mChildIpv6AddrsInfoList);
    MERGE_FIELD(NetDiagData::kNetworkDataBit, mNetworkData);
    MERGE_FIELD(NetDiagData::kTimeoutBit, mTimeout);
    MERGE_FIELD(NetDiagData::kConnectivityBit, mConnectivity);
    MERGE_FIELD(NetDiagData::kBatteryLevelBit, mBatteryLevel);
    MERGE_FIELD(NetDiagData::kSupplyVoltageBit, mSupplyVoltage);
    MERGE_FIELD(NetDiagData::kChannelPagesBit, mChannelPages);
    MERGE_FIELD(NetDiagData::kTypeListBit, mTypeList);
    MERGE_FIELD(NetDiagData::kMaxChildTimeoutBit, mMaxChildTimeout);
    MERGE_FIELD(NetDiagData::kVersionBit, mVersion);
    MERGE_FIELD(NetDiagData::kVendorNameBit, mVendorName);
    MERGE_FIELD(NetDiagData::kVendorModelBit, mVendorModel);
    MERGE_FIELD(NetDiagData::kVendorSWVersionBit, mVendorSWVersion);
    MERGE_FIELD(NetDiagData::kThreadStackVersionBit, mThreadStackVersion);
    MERGE_VECTOR_FIELD(NetDiagData::kChildBit, mChild);
    MERGE_VECTOR_FIELD(NetDiagData::kRouterNeighborBit, mRouterNeighbor);
    MERGE_FIELD(NetDiagData::kMleCountersBit, mMleCounters);
    MERGE_FIELD(NetDiagData::kVendorAppURLBit, mVendorAppURL);
    MERGE_FIELD(NetDiagData::kNonPreferredChannelsMaskBit, mNonPreferredChannelsMask);

    // Answer TLV is used for reassembly and shouldn't be part of the final merged data strictly speaking
    // But for completeness we can overwrite them.
    MERGE_FIELD(NetDiagData::kAnswerBit, mAnswer);
    MERGE_FIELD(NetDiagData::kQueryIdBit, mQueryId);

#undef MERGE_FIELD
#undef MERGE_VECTOR_FIELD
}

} // namespace commissioner

} // namespace ot
