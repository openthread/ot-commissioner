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

#ifndef OT_COMM_COMMISSIONER_IMPL_INTERNAL_HPP_
#define OT_COMM_COMMISSIONER_IMPL_INTERNAL_HPP_

#include <string>
#include <vector>

#include "commissioner/error.hpp"
#include "commissioner/network_data.hpp"
#include "commissioner/network_diag_data.hpp"

namespace ot {

namespace commissioner {

static constexpr uint8_t kChildTableEntryBytes  = 3;
static constexpr uint8_t kIpv6AddressBytes      = 16;
static constexpr uint8_t kLeaderDataBytes       = 8;
static constexpr uint8_t kMacCountersBytes      = 36;
static constexpr uint8_t kRloc16Bytes           = 2;
static constexpr uint8_t kRouterIdMaskBytes     = 8;
static constexpr uint8_t kPrefixBytes           = 2;
static constexpr uint8_t kHasRouteBytes         = 3;
static constexpr uint8_t kBorderRouterBytes     = 4;
static constexpr uint8_t kSixLowPanContextBytes = 2;

namespace internal {

Error     DecodeIpv6AddressList(std::vector<std::string> &aAddrs, const ByteArray &aBuf);
Error     DecodeChildIpv6AddressList(std::vector<ChildIpv6AddrInfo> &aChildIpv6AddressInfoList, const ByteArray &aBuf);
Error     DecodeNetDiagData(NetDiagData &aNetDiagData, const ByteArray &aPayload);
Error     DecodeModeData(ModeData &aModeData, const ByteArray &aBuf);
Error     DecodeChildTable(std::vector<ChildTableEntry> &aChildTable, const ByteArray &aBuf);
Error     DecodeLeaderData(LeaderData &aLeaderData, const ByteArray &aBuf);
Error     DecodeMacCounters(MacCounters &aMacCounters, const ByteArray &aBuf);
Error     DecodeRoute64(Route64 &aRoute64, const ByteArray &aBuf);
Error     DecodeConnectivity(Connectivity &aConnectivity, const ByteArray &aBuf);
void      DecodeRouteDataEntry(RouteDataEntry &aRouteDataEntry, uint8_t aBuf);
ByteArray ExtractRouterIds(const ByteArray &aMask);

Error DecodeNetworkData(NetworkData &aNetworkData, const ByteArray &aBuf);
Error DecodePrefixEntry(PrefixEntry &aPrefixEntry, const ByteArray &aBuf);
Error DecodeHasRoute(std::vector<HasRouteEntry> &aHasRouteList, const ByteArray &aBuf);
Error DecodeBorderRouter(std::vector<BorderRouterEntry> &aBorderRouterList, const ByteArray &aBuf);
Error DecodeContext(SixLowPanContext &aSixLowPanContext, const ByteArray &aBuf);

} // namespace internal

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_COMMISSIONER_IMPL_INTERNAL_HPP_
