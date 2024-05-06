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
 *   The file implements Thread TLV.
 */

#include "library/tlv.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <set>
#include <string>

#include "commissioner/defines.hpp"
#include "commissioner/error.hpp"
#include "common/error_macros.hpp"
#include "common/logging.hpp"
#include "common/utils.hpp"

namespace ot {

namespace commissioner {

namespace tlv {

Tlv::Tlv(Type aType, Scope aScope)
    : mScope(aScope)
    , mType(aType)
{
}

Tlv::Tlv(Type aType, const ByteArray &aValue, Scope aScope)
    : mScope(aScope)
    , mType(aType)
    , mValue(aValue)
{
}

Tlv::Tlv(Type aType, const std::string &aValue, Scope aScope)
    : mScope(aScope)
    , mType(aType)
    , mValue(aValue.begin(), aValue.end())
{
}

Tlv::Tlv(Type aType, int8_t aValue, Scope aScope)
    : mScope(aScope)
    , mType(aType)
{
    mValue.emplace_back(aValue);
}

Tlv::Tlv(Type aType, uint8_t aValue, Scope aScope)
    : mScope(aScope)
    , mType(aType)
{
    mValue.emplace_back(aValue);
}

Tlv::Tlv(Type aType, uint16_t aValue, Scope aScope)
    : mScope(aScope)
    , mType(aType)
{
    mValue = utils::Encode(aValue);
}

Tlv::Tlv(Type aType, uint32_t aValue, Scope aScope)
    : mScope(aScope)
    , mType(aType)
{
    mValue = utils::Encode(aValue);
}

Tlv::Tlv(Type aType, uint64_t aValue, Scope aScope)
    : mScope(aScope)
    , mType(aType)
{
    mValue = utils::Encode(aValue);
}

bool IsExtendedTlv(Type aType)
{
    switch (aType)
    {
    case Type::kUdpEncapsulation:
    case Type::kCommissionerToken:
    case Type::kJoinerDtlsEncapsulation:
        return true;
    default:
        return false;
    }
}

void Tlv::Serialize(ByteArray &aBuf) const
{
    ASSERT(IsValid());

    utils::Encode(aBuf, utils::to_underlying(GetType()));

    // TODO(wgtdkp): Thread 1.2 allows extended TLVs use
    // the base TLV format if its length does not exceed
    // 254 bytes. But OpenThread currently does not support
    // this kind of encoding.
    if (IsExtendedTlv(mType))
    {
        // if (GetLength() >= kEscapeLength) {
        utils::Encode(aBuf, kEscapeLength);
        utils::Encode(aBuf, GetLength());
    }
    else
    {
        utils::Encode(aBuf, static_cast<uint8_t>(GetLength()));
    }

    aBuf.insert(aBuf.end(), mValue.begin(), mValue.end());
}

TlvPtr Tlv::Deserialize(Error &aError, size_t &aOffset, const ByteArray &aBuf, Scope aScope)
{
    Error    error;
    size_t   offset = aOffset;
    uint8_t  type;
    uint16_t length;
    TlvPtr   tlv = nullptr;

    VerifyOrExit(offset + 2 <= aBuf.size(), error = ERROR_BAD_FORMAT("premature end of TLV"));
    type   = aBuf[offset++];
    length = aBuf[offset++];
    if (length == kEscapeLength)
    {
        VerifyOrExit(offset + 2 <= aBuf.size(),
                     error = ERROR_BAD_FORMAT("premature end of Extended TLV(type={})", type));

        length = (aBuf[offset++] << 8) & 0xFF00;
        length |= (aBuf[offset++]) & 0x00FF;
    }

    VerifyOrExit(offset + length <= aBuf.size(),
                 error = ERROR_BAD_FORMAT("premature end of TLV(type={}, length={})", type, length));

    tlv = std::make_shared<Tlv>(utils::from_underlying<Type>(type), aScope);
    tlv->SetValue(&aBuf[offset], length);

    offset += length;

exit:
    if (error != ErrorCode::kNone)
    {
        tlv = nullptr;
    }
    else
    {
        aOffset = offset;
    }

    aError = error;
    return tlv;
}

bool Tlv::IsValid() const
{
    auto length = GetLength();

    // The length is cutted off.
    if (length != mValue.size())
    {
        return false;
    }

    if (mScope == Scope::kThread)
    {
        switch (mType)
        {
        // Thread Network Layer TLVs
        case Type::kThreadStatus:
            return length == 1;
        case Type::kThreadTimeout:
            return length == 4;
        case Type::kThreadIpv6Addresses:
            return (length % 16 == 0) && (length / 16 >= 1 && length / 16 <= 15);
        case Type::kThreadCommissionerSessionId:
            return length == 2;
        case Type::kThreadCommissionerToken:
            return true;
        case Type::kThreadCommissionerSignature:
            return length < kEscapeLength;
        default:
            return false;
        }
    }
    else if (mScope == Scope::kMeshLink)
    {
        return false;
    }
    else if (mScope == Scope::kNetworkDiag)
    {
        switch (mType)
        {
        // Network disgnostic layer TLVs
        case Type::kNetworkDiagExtMacAddress:
<<<<<<< HEAD
            return length == 8;
        case Type::kNetworkDiagMacAddress:
            return length == 2;
=======
            return length >= 8;
        case Type::kNetworkDiagMacAddress:
            return length >= 2;
>>>>>>> 20da654 ([tlv] add new tlvs for network diagnostic)
        case Type::kNetworkDiagMode:
            return length == 1;
        case Type::kNetworkDiagTimeout:
            return length == 4;
        case Type::kNetworkDiagRoute64:
            return length >= 4;
        case Type::kNetworkDiagLeaderData:
            return length == 8;
        case Type::kNetworkDiagNetworkData:
            return true;
        case Type::kNetworkDiagIpv6Address:
            return (length % 16 == 0) && (length / 16 >= 1 && length / 16 <= 15);
        case Type::kNetworkDiagMacCounters:
            return length <= 36;
        case Type::kNetworkDiagBatteryLevel:
            return length == 1;
        case Type::kNetworkDiagSupplyVoltage:
            return length == 2;
        case Type::kNetworkDiagChildTable:
            return true; // list of 0 or more child entry data
        case Type::kNetworkDiagChannelPages:
            return length >= 1; // 1 or more 8-bit integers
        case Type::kNetworkDiagTypeList:
            return length >= 1; // 1 or more 8-bit integers
        case Type::kNetworkDiagMaxChildTimeout:
            return length == 4;
        case Type::kNetworkDiagLDevIDSubjectPubKeyInfo:
            return true;
        case Type::kNetworkDiagIDevIDCert:
            return true;
        case Type::kNetworkDiagEui64:
            return length == 8;
        case Type::kNetworkDiagVersion:
            return length == 2;
        case Type::kNetworkDiagVendorName:
            return length <= 4;
        case Type::kNetworkDiagVendorModel:
            return length <= 4;
        case Type::kNetworkDiagVendorSWVersion:
            return length <= 2;
        case Type::kNetworkDiagChild:
            return length <= 43;
        case Type::kNetworkDiagChildIpv6Address:
            return (length % 16 == 0) && (length / 16 >= 1 && length / 16 <= 15);
        case Type::kNetworkDiagRouterNeighbor:
            return length <= 24;
        case Type::kNetworkDiagAnswer:
            return length == 2;
        case Type::kNetworkDiagQueryID:
            return length == 2;
        case Type::kNetworkDiagMleCounters:
            return length <= 66;
        default:
            return false;
        }
    }

    switch (mType)
    {
    // Network Management TLVs
    case Type::kChannel:
        return length == 3;
    case Type::kPanId:
        return length == 2;
    case Type::kExtendedPanId:
        return length == 8;
    case Type::kNetworkName:
        return length <= 16;
    case Type::kPSKc:
        return length <= 16;
    case Type::kNetworkMasterKey:
        return length == 16;
    case Type::kNetworkKeySequenceCounter:
        return length == 4;
    case Type::kNetworkMeshLocalPrefix:
        return length == 8;
    case Type::kSteeringData:
        return length <= 16;
    case Type::kBorderAgentLocator:
        return length == 2;
    case Type::kCommissionerId:
        return length <= 64;
    case Type::kCommissionerSessionId:
        return length == 2;
    case Type::kActiveTimestamp:
        return length == 8;
    case Type::kCommissionerUdpPort:
        return length == 2;
    case Type::kSecurityPolicy:
        return length == 3 || length == 4;
    case Type::kPendingTimestamp:
        return length == 8;
    case Type::kDelayTimer:
        return length == 4;
    case Type::kChannelMask:
        return length < kEscapeLength;

    // MeshCoP Protocol Command TLVs
    case Type::kGet:
        return length < kEscapeLength;
    case Type::kState:
        return length == 1;
    case Type::kJoinerDtlsEncapsulation:
        return true;
    case Type::kJoinerUdpPort:
        return length == 2;
    case Type::kJoinerIID:
        return length == 8;
    case Type::kJoinerRouterLocator:
        return length == 2;
    case Type::kJoinerRouterKEK:
        return length == kJoinerRouterKekLength;
    case Type::kCount:
        return length == 1;
    case Type::kPeriod:
        return length == 2;
    case Type::kScanDuration:
        return length == 2;
    case Type::kEnergyList:
        return length < kEscapeLength;
    case Type::kSecureDissemination:
        return length < kEscapeLength;

    // TMF Provisioning and Discovery TLVs
    case Type::kProvisioningURL:
        return length <= 64;
    case Type::kVendorName:
        return length <= 32;
    case Type::kVendorModel:
        return length <= 32;
    case Type::kVendorSWVersion:
        return length <= 16;
    case Type::kVendorData:
        return length <= 64;
    case Type::kVendorStackVersion:
        return length < kEscapeLength;
    case Type::kUdpEncapsulation:
        return length >= 4;
    case Type::kIpv6Address:
        return length == 16;
    case Type::kDomainName:
        return length <= 16;
    case Type::kDomainPrefix:
        return true; // Reserved.
    case Type::kAeSteeringData:
        return length <= 16;
    case Type::kNmkpSteeringData:
        return length <= 16;
    case Type::kCommissionerToken:
        return true;
    case Type::kCommissionerSignature:
        return length < kEscapeLength;
    case Type::kAeUdpPort:
        return length == 2;
    case Type::kNmkpUdpPort:
        return length == 2;
    case Type::kTriHostname:
        return length < kEscapeLength;
    case Type::kRegistrarIpv6Address:
        return length == 16;
    case Type::kRegistrarHostname:
        return length < kEscapeLength;
    case Type::kCommissionerPenSignature:
        return length < kEscapeLength;
    case Type::kDiscoveryRequest:
        return length == 2;
    case Type::kDiscoveryResponse:
        return length == 2;

    default:
        return false;
    }
}

Type Tlv::GetType() const
{
    return mType;
}

void Tlv::SetValue(const uint8_t *aBuf, size_t aLength)
{
    mValue.assign(aBuf, aBuf + aLength);
}

void Tlv::SetValue(const ByteArray &aValue)
{
    mValue = aValue;
}

uint16_t Tlv::GetLength() const
{
    return static_cast<uint16_t>(mValue.size());
}

uint16_t Tlv::GetTotalLength() const
{
    uint16_t valueLength = GetLength();
    return sizeof(Type) + (valueLength >= kEscapeLength ? 3 : 1) + valueLength;
}

int8_t Tlv::GetValueAsInt8() const
{
    VerifyOrDie(mValue.size() >= sizeof(int8_t));
    return static_cast<int8_t>(mValue[0]);
}

uint16_t Tlv::GetValueAsUint8() const
{
    VerifyOrDie(mValue.size() >= sizeof(uint8_t));
    return utils::Decode<uint8_t>(mValue);
}

uint16_t Tlv::GetValueAsUint16() const
{
    VerifyOrDie(mValue.size() >= sizeof(uint16_t));
    return utils::Decode<uint16_t>(mValue);
}

std::string Tlv::GetValueAsString() const
{
    return std::string{mValue.begin(), mValue.end()};
}

const ByteArray &Tlv::GetValue() const
{
    return mValue;
}
ByteArray &Tlv::GetValue()
{
    return mValue;
}

Error GetTlvSet(TlvSet &aTlvSet, const ByteArray &aBuf, Scope aScope)
{
    Error  error;
    size_t offset = 0;

    while (offset < aBuf.size())
    {
        auto tlv = tlv::Tlv::Deserialize(error, offset, aBuf, aScope);
        SuccessOrExit(error);
        VerifyOrDie(tlv != nullptr);

        if (tlv->IsValid())
        {
            aTlvSet[tlv->GetType()] = tlv;
        }
        else
        {
            // Drop invalid TLVs
            LOG_WARN(LOG_REGION_COAP, "dropping invalid/unknown TLV(type={}, value={})",
                     utils::to_underlying(tlv->GetType()), utils::Hex(tlv->GetValue()));
        }
    }

exit:
    return error;
}

TlvPtr GetTlv(tlv::Type aTlvType, const ByteArray &aBuf, Scope aScope)
{
    TlvSet tlvSet;
    TlvPtr ret = nullptr;

    SuccessOrExit(GetTlvSet(tlvSet, aBuf, aScope));

    ret = tlvSet[aTlvType];

exit:
    return ret;
}

bool IsDatasetParameter(bool aIsActiveDataset, tlv::Type aTlvType)
{
    static const std::set<tlv::Type> kActiveSet = {tlv::Type::kActiveTimestamp,
                                                   tlv::Type::kChannel,
                                                   tlv::Type::kChannelMask,
                                                   tlv::Type::kExtendedPanId,
                                                   tlv::Type::kNetworkMeshLocalPrefix,
                                                   tlv::Type::kNetworkMasterKey,
                                                   tlv::Type::kNetworkName,
                                                   tlv::Type::kPanId,
                                                   tlv::Type::kPSKc,
                                                   tlv::Type::kSecurityPolicy};

    static const std::set<tlv::Type> kPendingSet = {
        tlv::Type::kActiveTimestamp,  tlv::Type::kChannel,       tlv::Type::kChannelMask,
        tlv::Type::kDelayTimer,       tlv::Type::kExtendedPanId, tlv::Type::kNetworkMeshLocalPrefix,
        tlv::Type::kNetworkMasterKey, tlv::Type::kNetworkName,   tlv::Type::kPanId,
        tlv::Type::kPendingTimestamp, tlv::Type::kPSKc,          tlv::Type::kSecurityPolicy};

    return (aIsActiveDataset ? kActiveSet : kPendingSet).count(aTlvType) != 0;
}

} // namespace tlv

} // namespace commissioner

} // namespace ot
