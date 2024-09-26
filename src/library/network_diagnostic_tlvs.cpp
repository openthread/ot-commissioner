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

#include "commissioner/network_diagnostic_tlvs.hpp"
#include <sys/types.h>
#include <cstddef>
#include <cstdint>
#include <string>
#include "common/utils.hpp"
#include "common/error_macros.hpp"
#include "commissioner/defines.hpp"
#include "commissioner/error.hpp"

#define ROUTER_ID_MASK_BYTES 8
#define CHILD_TABLE_ENTRY_BYTES 4
#define IPV6_ADDRESS_BYTES 16

namespace ot {

namespace commissioner {

Error LeaderData::Decode(LeaderData &aLeaderData, const ByteArray &aBuf)
{
    size_t length = aBuf.size();
    Error  error;

    VerifyOrExit(length == 8,
                 error = ERROR_BAD_FORMAT("incorrect size of LeaderData"));

    aLeaderData.mPartitionId       = utils::Decode<uint32_t>(aBuf.data(), 4);
    aLeaderData.mWeighting         = aBuf[length - 4];
    aLeaderData.mDataVersion       = aBuf[length - 3];
    aLeaderData.mStableDataVersion = aBuf[length - 2];
    aLeaderData.mRouterId          = aBuf[length - 1];
exit:
    return error;
}

void RouteDataEntry::Decode(RouteDataEntry &aRouteDataEntry, uint8_t aBuf)
{
    aRouteDataEntry.mOutgoingLinkQuality  = (aBuf >> 6) & 0x03;
    aRouteDataEntry.mIncommingLinkQuality = (aBuf >> 4) & 0x03;
    aRouteDataEntry.mRouteCost            = aBuf & 0x0F;
}

Error Route64::Decode(Route64 &aRoute64, const ByteArray &aBuf)
{
    Error     error;
    size_t    length = aBuf.size();
    size_t    offset = 0;
    ByteArray routerIdList;

    VerifyOrExit(length >= ROUTER_ID_MASK_BYTES + 1,
                 error = ERROR_BAD_FORMAT("incorrect size of Route64"));
    aRoute64.mIdSequence = aBuf[offset++];
    aRoute64.mMask = {aBuf.begin() + offset,
                      aBuf.begin() + offset + ROUTER_ID_MASK_BYTES};
    offset += ROUTER_ID_MASK_BYTES;

    routerIdList = ExtractRouterIds(aRoute64.mMask);
    VerifyOrExit((length - offset) == routerIdList.size(),
                 error = ERROR_BAD_FORMAT("incorrect size of RouteData"));
    while (offset < length)
    {
        RouteDataEntry entry;
        entry.mRouterId = routerIdList[offset - ROUTER_ID_MASK_BYTES - 1];
        RouteDataEntry::Decode(entry, aBuf[offset]);
        aRoute64.mRouteData.emplace_back(entry);
        offset++;
    }
exit:
    return error;
}

std::string Route64::ToString() const{
  std::string ret;
  ret += "id_sequence: " + std::to_string(mIdSequence) + "\n";
  ret += "mask: ";
  for (uint8_t byte : mMask) {
    ret += std::to_string(byte) + " ";
  }
  ret += "\n";
  for (const auto &entry : mRouteData) {
    ret += "router_id: " + std::to_string(entry.mRouterId) + "\n";
    ret += "outgoing_link_quality: "
            + std::to_string(entry.mOutgoingLinkQuality)
            + "\n";
    ret += "incomming_link_quality: "
            + std::to_string(entry.mIncommingLinkQuality)
            + "\n";
    ret += "route_cost: " + std::to_string(entry.mRouteCost) + "\n";
  }
  return ret;
}

std::string LeaderData::ToString() const
{
    std::string ret;
    ret += "partition_id: " + std::to_string(mPartitionId) + "\n";
    ret += "weighting: " + std::to_string(mWeighting) + "\n";
    ret += "data_version: " + std::to_string(mDataVersion) + "\n";
    ret += "stable_data_version: " + std::to_string(mStableDataVersion) + "\n";
    ret += "router_id: " + std::to_string(mRouterId) + "\n";
    return ret;
}

ByteArray Route64::ExtractRouterIds(const ByteArray  &aMask) {
  ByteArray routerIdList;

  for (size_t i = 0; i < ROUTER_ID_MASK_BYTES * 8; i++) {
    if ((aMask[i / 8] & (0x80 >> (i % 8))) != 0) {
      routerIdList.push_back(i);
    }
  }

  return routerIdList;
}

void Mode::Decode(Mode &aMode, uint8_t aBuf)
{
    aMode.mIsRxOnWhenIdleMode = (aBuf & 0x01) != 0;
    aMode.mIsMtd = (aBuf & 0x02) != 0;
    aMode.mIsStableNetworkDataRequired = (aBuf & 0x04) != 0;
}

std::string Mode::ToString() const
{
    std::string ret;
    ret += "is_rx_on_when_idle_mode: "
            + std::to_string(mIsRxOnWhenIdleMode) + "\n";
    ret += "is_mtd: " + std::to_string(mIsMtd) + "\n";
    ret += "is_stable_network_data_required: "
            + std::to_string(mIsStableNetworkDataRequired) + "\n";
    return ret;
}

Error ChildEntry::Decode(ChildEntry &aChildEntry, const ByteArray &aBuf)
{
    Error  error;
    size_t length = aBuf.size();
    size_t offset = 0;

    VerifyOrExit(offset + 4 <= length,
                  error = ERROR_BAD_FORMAT("premature end of Child Table"));
    aChildEntry.mTimeout             = aBuf[offset++];
    aChildEntry.mIncomingLinkQuality = aBuf[offset++];
    aChildEntry.mChildId             = aBuf[offset++];
    Mode::Decode(aChildEntry.mModeData, aBuf[offset++]);
exit:
    return error;
}

std::string ChildEntry::ToString() const
{
    std::string ret;
    ret += "timeout: " + std::to_string(mTimeout) + "\n";
    ret += "incoming_link_quality: " + std::to_string(mIncomingLinkQuality)
            + "\n";
    ret += "child_id: " + std::to_string(mChildId) + "\n";
    ret += "mode: " + mModeData.ToString() + "\n";
    return ret;
}

Error ChildTable::Decode(ChildTable &aChildTable, const ByteArray &aBuf)
{
    Error  error;
    size_t length = aBuf.size();
    size_t offset = 0;
    while (offset < length)
    {
        ChildEntry entry;
        VerifyOrExit(offset + 4 <= length,
                     error = ERROR_BAD_FORMAT("premature end of Child Table"));
        SuccessOrExit(error = ChildEntry::Decode(entry, {aBuf.begin() + offset,
                      aBuf.begin() + offset + CHILD_TABLE_ENTRY_BYTES}));
        aChildTable.mChildEntries.emplace_back(entry);
    }
exit:
    return error;
}

std::string ChildTable::ToString() const
{
    std::string ret;
    for (const auto &entry : mChildEntries) {
        ret += entry.ToString() + "\n";
    }
    return ret;
}

Error Ipv6Address::Decode(Ipv6Address &aIpv6Address, const ByteArray &aBuf)
{
    Error error;
    size_t length = aBuf.size();
    size_t offset = 0;
    while (offset < length)
    {
        VerifyOrExit(offset + IPV6_ADDRESS_BYTES <= length,
                     error = ERROR_BAD_FORMAT("premature end of IPv6 Address"));
        aIpv6Address.mIpv6Addresses.emplace_back(aBuf.begin() + offset,
                                                 aBuf.begin() + offset +
                                                     IPV6_ADDRESS_BYTES);
        offset += IPV6_ADDRESS_BYTES;
    }
exit:
    return error;
}

std::string Ipv6Address::ToString() const
{
    std::string ret;
    for (const auto &address : mIpv6Addresses) {
        ret += utils::Hex(address) + "\n";
    }
    return ret;
}

}  // namespace commissioner

}  // namespace ot
