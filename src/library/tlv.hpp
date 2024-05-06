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
 *   This file includes definitions for Thread TLVs.
 */

#ifndef OT_COMM_LIBRARY_TLV_HPP_
#define OT_COMM_LIBRARY_TLV_HPP_

#include <limits>
#include <list>
#include <map>
#include <memory>

#include <stdint.h>

#include <commissioner/defines.hpp>
#include <commissioner/error.hpp>

namespace ot {

namespace commissioner {

namespace tlv {

class Tlv;

static constexpr uint8_t kEscapeLength =
    0xFF; ///< This length value indicates the actual length is of two-bytes length.
static const int8_t kStateReject  = -1;
static const int8_t kStateAccept  = 1;
static const int8_t kStatePending = 0;

enum class Scope : uint8_t
{
    kMeshCoP = 0,
    kThread,
    kMeshLink,
    kNetworkDiag,
};

enum class Type : uint8_t
{

    /*
     * Thread Network Layer Tlvs.
     */
    kThreadStatus                = 4,
    kThreadTimeout               = 11,
    kThreadIpv6Addresses         = 14,
    kThreadCommissionerSessionId = 15,
    kThreadCommissionerToken     = 63,
    kThreadCommissionerSignature = 64,

    /*
     * Thread MeshCoP TLVs.
     */

    // Network management TLVs
    kChannel                   = 0,
    kPanId                     = 1,
    kExtendedPanId             = 2,
    kNetworkName               = 3,
    kPSKc                      = 4,
    kNetworkMasterKey          = 5,
    kNetworkKeySequenceCounter = 6,
    kNetworkMeshLocalPrefix    = 7,
    kSteeringData              = 8,
    kBorderAgentLocator        = 9,
    kCommissionerId            = 10,
    kCommissionerSessionId     = 11,
    kActiveTimestamp           = 14,
    kCommissionerUdpPort       = 15,
    kSecurityPolicy            = 12,
    kPendingTimestamp          = 51,
    kDelayTimer                = 52,
    kChannelMask               = 53,

    // MeshCoP Protocol Command TLVs
    kGet                     = 13,
    kState                   = 16,
    kJoinerDtlsEncapsulation = 17,
    kJoinerUdpPort           = 18,
    kJoinerIID               = 19,
    kJoinerRouterLocator     = 20,
    kJoinerRouterKEK         = 21,
    kCount                   = 54,
    kPeriod                  = 55,
    kScanDuration            = 56,
    kEnergyList              = 57,
    kSecureDissemination     = 58,

    // TMF Provisioning and Discovery TLVs
    kProvisioningURL          = 32,
    kVendorName               = 33,
    kVendorModel              = 34,
    kVendorSWVersion          = 35,
    kVendorData               = 36,
    kVendorStackVersion       = 37,
    kUdpEncapsulation         = 48,
    kIpv6Address              = 49,
    kDomainName               = 59,
    kDomainPrefix             = 60,
    kAeSteeringData           = 61,
    kNmkpSteeringData         = 62,
    kCommissionerToken        = 63,
    kCommissionerSignature    = 64,
    kAeUdpPort                = 65,
    kNmkpUdpPort              = 66,
    kTriHostname              = 67,
    kRegistrarIpv6Address     = 68,
    kRegistrarHostname        = 69,
    kCommissionerPenSignature = 70,
    kDiscoveryRequest         = 128,
    kDiscoveryResponse        = 129,

    // TMF Network Diagnositcs TLVs
    kNetworkDiagExtMacAddress           = 0,
    kNetworkDiagMacAddress              = 1,
    kNetworkDiagMode                    = 2,
    kNetworkDiagTimeout                 = 3,
    kNetworkDiagConnectivity            = 4,
    kNetworkDiagRoute64                 = 5,
    kNetworkDiagLeaderData              = 6,
    kNetworkDiagNetworkData             = 7,
    kNetworkDiagIpv6Address             = 8,
    kNetworkDiagMacCounters             = 9,
    kNetworkDiagBatteryLevel            = 14,
    kNetworkDiagSupplyVoltage           = 15,
    kNetworkDiagChildTable              = 16,
    kNetworkDiagChannelPages            = 17,
    kNetworkDiagTypeList                = 18,
    kNetworkDiagMaxChildTimeout         = 19,
    kNetworkDiagLDevIDSubjectPubKeyInfo = 20,
    kNetworkDiagIDevIDCert              = 21,
    kNetworkDiagEui64                   = 23,
    kNetworkDiagVersion                 = 24,
    kNetworkDiagVendorName              = 25,
    kNetworkDiagVendorModel             = 26,
    kNetworkDiagVendorSWVersion         = 27,
    kNetworkDiagThreadStackVersion      = 28,
    kNetworkDiagChild                   = 29,
    kNetworkDiagChildIpv6Address        = 30,
    kNetworkDiagRouterNeighbor          = 31,
    kNetworkDiagAnswer                  = 32,
    kNetworkDiagQueryID                 = 33,
    kNetworkDiagMleCounters             = 34,
};

using TlvPtr      = std::shared_ptr<Tlv>;
using TlvSet      = std::map<tlv::Type, tlv::TlvPtr>;
using TlvList     = std::list<tlv::Tlv>;
using TlvTypeList = std::list<tlv::Type>;

class Tlv
{
public:
    explicit Tlv(Type aType, Scope aScope = Scope::kMeshCoP);
    Tlv(Type aType, const ByteArray &aValue, Scope aScope = Scope::kMeshCoP);
    Tlv(Type aType, const std::string &aValue, Scope aScope = Scope::kMeshCoP);
    Tlv(Type aType, int8_t aValue, Scope aScope = Scope::kMeshCoP);
    Tlv(Type aType, uint8_t aValue, Scope aScope = Scope::kMeshCoP);
    Tlv(Type aType, uint16_t aValue, Scope aScope = Scope::kMeshCoP);
    Tlv(Type aType, uint32_t aValue, Scope aScope = Scope::kMeshCoP);
    Tlv(Type aType, uint64_t aValue, Scope aScope = Scope::kMeshCoP);

    void          Serialize(ByteArray &aBuf) const;
    static TlvPtr Deserialize(Error &aError, size_t &aOffset, const ByteArray &aBuf, Scope aScope = Scope::kMeshCoP);

    bool     IsValid() const;
    Type     GetType() const;
    void     SetValue(const uint8_t *aBuf, size_t aLength);
    void     SetValue(const ByteArray &aValue);
    uint16_t GetLength() const;
    uint16_t GetTotalLength() const;

    // It is the caller that make sure the tlv is valid.
    int8_t           GetValueAsInt8() const;
    uint16_t         GetValueAsUint8() const;
    uint16_t         GetValueAsUint16() const;
    std::string      GetValueAsString() const;
    const ByteArray &GetValue() const;
    ByteArray       &GetValue();

private:
    Scope     mScope = Scope::kMeshCoP;
    Type      mType;
    ByteArray mValue;
};

Error  GetTlvSet(TlvSet &aTlvSet, const ByteArray &aBuf, Scope aScope = Scope::kMeshCoP);
TlvPtr GetTlv(tlv::Type aTlvType, const ByteArray &aBuf, Scope aScope = Scope::kMeshCoP);
bool   IsDatasetParameter(bool aIsActiveDataset, tlv::Type aTlvType);

} // namespace tlv

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_LIBRARY_TLV_HPP_
