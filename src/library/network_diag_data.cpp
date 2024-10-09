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

#include "commissioner/network_diag_data.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <sys/types.h>
#include "commissioner/defines.hpp"
#include "commissioner/error.hpp"
#include "common/error_macros.hpp"
#include "common/utils.hpp"

#define ROUTER_ID_MASK_BYTES 8
#define CHILD_TABLE_ENTRY_BYTES 4
#define IPV6_ADDRESS_BYTES 16
#define RLOC16_BYTES 2
#define MAC_COUNTERS_BYTES 36

namespace ot {

namespace commissioner {

Error LeaderData::Decode(LeaderData &aLeaderData, const ByteArray &aBuf)
{
    size_t length = aBuf.size();
    Error  error;

    VerifyOrExit(length == 8, error = ERROR_BAD_FORMAT("incorrect size of LeaderData"));

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
    aRouteDataEntry.mOutgoingLinkQuality = (aBuf >> 6) & 0x03;
    aRouteDataEntry.mIncomingLinkQuality = (aBuf >> 4) & 0x03;
    aRouteDataEntry.mRouteCost           = aBuf & 0x0F;
}

Error Route64::Decode(Route64 &aRoute64, const ByteArray &aBuf)
{
    Error     error;
    size_t    length = aBuf.size();
    size_t    offset = 0;
    ByteArray routerIdList;

    VerifyOrExit(length >= ROUTER_ID_MASK_BYTES + 1, error = ERROR_BAD_FORMAT("incorrect size of Route64"));
    aRoute64.mIdSequence = aBuf[offset++];
    aRoute64.mMask       = {aBuf.begin() + offset, aBuf.begin() + offset + ROUTER_ID_MASK_BYTES};
    offset += ROUTER_ID_MASK_BYTES;

    routerIdList = ExtractRouterIds(aRoute64.mMask);
    VerifyOrExit((length - offset) == routerIdList.size(), error = ERROR_BAD_FORMAT("incorrect size of RouteData"));
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

std::string Route64::ToString() const
{
    std::string ret;
    ret += "id_sequence: " + std::to_string(mIdSequence) + "\n";
    ret += "mask: ";
    for (uint8_t byte : mMask)
    {
        ret += std::to_string(byte) + " ";
    }
    ret += "\n";
    for (const auto &entry : mRouteData)
    {
        ret += "router_id: " + std::to_string(entry.mRouterId) + "\n";
        ret += "outgoing_link_quality: " + std::to_string(entry.mOutgoingLinkQuality) + "\n";
        ret += "incomming_link_quality: " + std::to_string(entry.mIncomingLinkQuality) + "\n";
        ret += "route_cost: " + std::to_string(entry.mRouteCost) + "\n";
    }
    return ret;
}

size_t Route64::GetRouteDataSize() const
{
    return mRouteData.size();
}

RouteDataEntry Route64::GetRouteData(size_t aIndex) const
{
    return mRouteData[aIndex];
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

ByteArray Route64::ExtractRouterIds(const ByteArray &aMask)
{
    ByteArray routerIdList;

    for (size_t i = 0; i < ROUTER_ID_MASK_BYTES * 8; i++)
    {
        if ((aMask[i / 8] & (0x80 >> (i % 8))) != 0)
        {
            routerIdList.push_back(i);
        }
    }

    return routerIdList;
}

void Mode::Decode(Mode &aMode, uint8_t aBuf)
{
    aMode.mIsRxOnWhenIdleMode          = (aBuf & 0x01) != 0;
    aMode.mIsMtd                       = (aBuf & 0x02) != 0;
    aMode.mIsStableNetworkDataRequired = (aBuf & 0x04) != 0;
}

std::string Mode::ToString() const
{
    std::string ret;
    ret += "is_rx_on_when_idle_mode: " + std::to_string(mIsRxOnWhenIdleMode) + "\n";
    ret += "is_mtd: " + std::to_string(mIsMtd) + "\n";
    ret += "is_stable_network_data_required: " + std::to_string(mIsStableNetworkDataRequired) + "\n";
    return ret;
}

Error ChildEntry::Decode(ChildEntry &aChildEntry, const ByteArray &aBuf)
{
    Error  error;
    size_t length = aBuf.size();
    size_t offset = 0;

    VerifyOrExit(offset + 4 <= length, error = ERROR_BAD_FORMAT("premature end of Child Table"));
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
    ret += "incoming_link_quality: " + std::to_string(mIncomingLinkQuality) + "\n";
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
        VerifyOrExit(offset + 4 <= length, error = ERROR_BAD_FORMAT("premature end of Child Table"));
        SuccessOrExit(error = ChildEntry::Decode(
                          entry, {aBuf.begin() + offset, aBuf.begin() + offset + CHILD_TABLE_ENTRY_BYTES}));
        aChildTable.mChildEntries.emplace_back(entry);
    }
exit:
    return error;
}

std::string ChildTable::ToString() const
{
    std::string ret;
    for (const auto &entry : mChildEntries)
    {
        ret += entry.ToString() + "\n";
    }
    return ret;
}

size_t ChildTable::GetSize() const
{
    return mChildEntries.size();
}

ChildEntry ChildTable::GetChildEntry(size_t aIndex) const
{
    return mChildEntries[aIndex];
}

Error Ipv6Address::Decode(Ipv6Address &aIpv6Address, const ByteArray &aBuf)
{
    Error error;
    VerifyOrExit(aBuf.size() == IPV6_ADDRESS_BYTES, error = ERROR_BAD_FORMAT("premature end of IPv6 Address"));
    aIpv6Address.mAddress = {aBuf.begin(), aBuf.end()};
exit:
    return error;
}

std::string Ipv6Address::ToString() const
{
    std::string ret;
    for (size_t i = 0; i < IPV6_ADDRESS_BYTES; i++)
    {
        if (i % 2 == 0 && i != 0 && i != IPV6_ADDRESS_BYTES - 1)
        {
            ret += ":";
        }
        ret += std::to_string(mAddress[i]);
    }
    return ret;
}

Error Ipv6AddressList::Decode(Ipv6AddressList &aIpv6AddressList, const ByteArray &aBuf)
{
    Error  error;
    size_t length = aBuf.size();
    size_t offset = 0;
    while (offset < length)
    {
        VerifyOrExit(offset + IPV6_ADDRESS_BYTES <= length, error = ERROR_BAD_FORMAT("premature end of IPv6 Address"));
        Ipv6Address ipv6Address;
        SuccessOrExit(error = Ipv6Address::Decode(ipv6Address,
                                                  {aBuf.begin() + offset, aBuf.begin() + offset + IPV6_ADDRESS_BYTES}));
        aIpv6AddressList.mIpv6Addresses.emplace_back(ipv6Address);
        offset += IPV6_ADDRESS_BYTES;
    }
exit:
    return error;
}

std::string Ipv6AddressList::ToString() const
{
    std::string ret;
    for (const auto &address : mIpv6Addresses)
    {
        ret += address.ToString() + "\n";
    }
    return ret;
}

size_t Ipv6AddressList::GetSize() const
{
    return mIpv6Addresses.size();
}

Ipv6Address Ipv6AddressList::GetIpv6Address(size_t aIndex) const
{
    return mIpv6Addresses[aIndex];
}

Error ChildIpv6AddressList::Decode(ChildIpv6AddressList &aChildIpv6AddressList, const ByteArray &aBuf)
{
    Error  error;
    size_t length = aBuf.size();
    size_t offset = 0;
    VerifyOrExit(length >= RLOC16_BYTES, error = ERROR_BAD_FORMAT("premature end of Child IPv6 Address"));
    aChildIpv6AddressList.mRloc16 = utils::Decode<uint16_t>(aBuf.data(), 2);
    offset += RLOC16_BYTES;
    SuccessOrExit(
        error = Ipv6AddressList::Decode(aChildIpv6AddressList.mIpv6AddressList, {aBuf.begin() + offset, aBuf.end()}));
exit:
    return error;
}

std::string ChildIpv6AddressList::ToString() const
{
    std::string ret;
    ret += "rloc16: " + std::to_string(mRloc16) + "\n";
    ret += "ipv6_address: " + mIpv6AddressList.ToString() + "\n";
    return ret;
}

Error MacCounters::Decode(MacCounters &aMacCounters, const ByteArray &aBuf)
{
    Error  error;
    size_t length = aBuf.size();
    VerifyOrExit(length >= MAC_COUNTERS_BYTES, error = ERROR_BAD_FORMAT("premature end of MacCounters"));
    aMacCounters.mIfInUnknownProtos  = utils::Decode<uint32_t>(aBuf.data(), 4);
    aMacCounters.mIfInErrors         = utils::Decode<uint32_t>(aBuf.data() + 4, 4);
    aMacCounters.mIfOutErrors        = utils::Decode<uint32_t>(aBuf.data() + 8, 4);
    aMacCounters.mIfInUcastPkts      = utils::Decode<uint32_t>(aBuf.data() + 12, 4);
    aMacCounters.mIfInBroadcastPkts  = utils::Decode<uint32_t>(aBuf.data() + 16, 4);
    aMacCounters.mIfInDiscards       = utils::Decode<uint32_t>(aBuf.data() + 20, 4);
    aMacCounters.mIfOutUcastPkts     = utils::Decode<uint32_t>(aBuf.data() + 24, 4);
    aMacCounters.mIfOutBroadcastPkts = utils::Decode<uint32_t>(aBuf.data() + 28, 4);
    aMacCounters.mIfOutDiscards      = utils::Decode<uint32_t>(aBuf.data() + 32, 4);
exit:
    return error;
}

std::string MacCounters::ToString() const
{
    std::string ret;
    ret += "if_in_unknown_protos: " + std::to_string(mIfInUnknownProtos) + "\n";
    ret += "if_in_errors: " + std::to_string(mIfInErrors) + "\n";
    ret += "if_out_errors: " + std::to_string(mIfOutErrors) + "\n";
    ret += "if_in_ucast_pkts: " + std::to_string(mIfInUcastPkts) + "\n";
    ret += "if_in_broadcast_pkts: " + std::to_string(mIfInBroadcastPkts) + "\n";
    ret += "if_in_discards: " + std::to_string(mIfInDiscards) + "\n";
    ret += "if_out_ucast_pkts: " + std::to_string(mIfOutUcastPkts) + "\n";
    ret += "if_out_broadcast_pkts: " + std::to_string(mIfOutBroadcastPkts) + "\n";
    ret += "if_out_discards: " + std::to_string(mIfOutDiscards) + "\n";
    return ret;
}

} // namespace commissioner

} // namespace ot
