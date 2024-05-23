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
 *   This file defines the types of Thread Network Diagnostic TLVs (Type-Length-Value) used for network diagnostics.
 */

#ifndef OT_COMM_NETWORK_DIAG_TLVS_HPP_
#define OT_COMM_NETWORK_DIAG_TLVS_HPP_

#include <cstdint>
#include <set>

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
 * @brief Set of TLV types commonly used in diagnostic get requests and queries.
 */
static const std::set<NetworkDiagTlvType> kCommonDiagnosticTlvs = {
    NetworkDiagTlvType::kNetworkDiagExtMacAddress,
    NetworkDiagTlvType::kNetworkDiagMacAddress,
    NetworkDiagTlvType::kNetworkDiagMode,
    NetworkDiagTlvType::kNetworkDiagTimeout,
    NetworkDiagTlvType::kNetworkDiagConnectivity,
    NetworkDiagTlvType::kNetworkDiagRoute64,
    NetworkDiagTlvType::kNetworkDiagLeaderData,
    NetworkDiagTlvType::kNetworkDiagNetworkData,
    NetworkDiagTlvType::kNetworkDiagIpv6Address,
    NetworkDiagTlvType::kNetworkDiagMacCounters,
    NetworkDiagTlvType::kNetworkDiagBatteryLevel,
    NetworkDiagTlvType::kNetworkDiagSupplyVoltage,
    NetworkDiagTlvType::kNetworkDiagChildTable,
    NetworkDiagTlvType::kNetworkDiagChannelPages,
    NetworkDiagTlvType::kNetworkDiagMaxChildTimeout,
    NetworkDiagTlvType::kNetworkDiagLDevIDSubjectPubKeyInfo,
    NetworkDiagTlvType::kNetworkDiagIDevIDCert,
    NetworkDiagTlvType::kNetworkDiagEui64,
    NetworkDiagTlvType::kNetworkDiagVersion,
    NetworkDiagTlvType::kNetworkDiagVendorName,
    NetworkDiagTlvType::kNetworkDiagVendorModel,
    NetworkDiagTlvType::kNetworkDiagVendorSWVersion,
    NetworkDiagTlvType::kNetworkDiagThreadStackVersion,
    NetworkDiagTlvType::kNetworkDiagMleCounters
};

/**
 * @brief Set of TLV types allowed in a Diagnostic Get Request.
 */
static const std::set<NetworkDiagTlvType> kDiagnosticGetRequestTlvs = kCommonDiagnosticTlvs;

/**
 * @brief Set of TLV types allowed in a Diagnostic Get Query.
 */
static std::set<NetworkDiagTlvType> kDiagnosticGetQueryTlvs = kCommonDiagnosticTlvs;
//kDiagnosticGetQueryTlvs.insert(NetworkDiagTlvType::kNetworkDiagChild);
//kDiagnosticGetQueryTlvs.insert(NetworkDiagTlvType::kNetworkDiagChildIpv6Address);
//kDiagnosticGetQueryTlvs.insert(NetworkDiagTlvType::kNetworkDiagRouterNeighbor);
//kDiagnosticGetQueryTlvs.insert(NetworkDiagTlvType::kNetworkDiagAnswer);
//kDiagnosticGetQueryTlvs.insert(NetworkDiagTlvType::kNetworkDiagQueryID);

static const std::set<NetworkDiagTlvType> kDiagnosticGetResetTlvs = {
    NetworkDiagTlvType::kNetworkDiagMacCounters,
    NetworkDiagTlvType::kNetworkDiagMleCounters    
};

/**
 * Type alias of type list
 */
using DiagTlvTypeList = std::vector<NetworkDiagTlvType>;

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_NETWORK_DIAG_TLVS_HPP_
