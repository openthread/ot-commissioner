/*
 *    Copyright (c) 2020, The OpenThread Commissioner Authors.
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
 *   The file implements entities stored with registry.
 */

#include "registry_entries.hpp"

#include "common/utils.hpp"

using nlohmann::json;

namespace ot {

namespace commissioner {

namespace persistent_storage {

namespace {
/**
 * This Exception class represents Network Data
 * and Configuration conversion errors from JSON file.
 *
 * We need this because the nlohmann::json library uses
 * exception exclusively.
 *
 */
class JsonException : public std::invalid_argument
{
public:
    explicit JsonException(Error aError)
        : std::invalid_argument(aError.GetMessage())
        , mError(aError)
    {
    }

    Error GetError() const { return mError; }

private:
    Error mError;
};
} // namespace

#define SuccessOrThrow(aError)                                                   \
    do                                                                           \
    {                                                                            \
        if (aError != ::ot::commissioner::ErrorCode::kNone)                      \
        {                                                                        \
            throw ::ot::commissioner::persistent_storage::JsonException(aError); \
        }                                                                        \
    } while (false)

void to_json(json &aJson, const RegistrarId &aValue)
{
    aJson = aValue.mId;
}

void from_json(const json &aJson, RegistrarId &aValue)
{
    aValue.mId = aJson.get<unsigned int>();
}

void to_json(json &aJson, const DomainId &aValue)
{
    aJson = aValue.mId;
}

void from_json(const json &aJson, DomainId &aValue)
{
    aValue.mId = aJson.get<unsigned int>();
}

void to_json(json &aJson, const NetworkId &aValue)
{
    aJson = aValue.mId;
}

void from_json(const json &aJson, NetworkId &aValue)
{
    aValue.mId = aJson.get<unsigned int>();
}

void to_json(json &aJson, const BorderRouterId &aValue)
{
    aJson = aValue.mId;
}

void from_json(const json &aJson, BorderRouterId &aValue)
{
    aValue.mId = aJson.get<unsigned int>();
}

void to_json(json &aJson, const Registrar &aValue)
{
    aJson = json{
        {JSON_ID, aValue.mId}, {JSON_ADDR, aValue.mAddr}, {JSON_PORT, aValue.mPort}, {JSON_DOMAINS, aValue.mDomains}};
}

void from_json(const json &aJson, Registrar &aValue)
{
    aJson.at(JSON_ID).get_to(aValue.mId);
    aJson.at(JSON_ADDR).get_to(aValue.mAddr);
    aJson.at(JSON_PORT).get_to(aValue.mPort);
    aJson.at(JSON_DOMAINS).get_to(aValue.mDomains);
}

void to_json(json &aJson, const Domain &aValue)
{
    aJson = json{{JSON_ID, aValue.mId}, {JSON_NAME, aValue.mName}};
}

void from_json(const json &aJson, Domain &aValue)
{
    aJson.at(JSON_ID).get_to(aValue.mId);
    aJson.at(JSON_NAME).get_to(aValue.mName);
}

void to_json(json &aJson, const Network &aValue)
{
    aJson = json{{JSON_ID, aValue.mId},
                 {JSON_DOM_REF, aValue.mDomainId},
                 {JSON_NAME, aValue.mName},
                 {JSON_PAN, aValue.mPan},
                 {JSON_XPAN, (std::string)aValue.mXpan},
                 {JSON_CHANNEL, aValue.mChannel},
                 {JSON_MLP, aValue.mMlp},
                 {JSON_CCM, aValue.mCcm}};
}

void from_json(const json &aJson, Network &aValue)
{
    aJson.at(JSON_ID).get_to(aValue.mId);
    aJson.at(JSON_DOM_REF).get_to(aValue.mDomainId);
    aJson.at(JSON_NAME).get_to(aValue.mName);
    aJson.at(JSON_PAN).get_to(aValue.mPan);

    std::string xpanStr;
    aJson.at(JSON_XPAN).get_to(xpanStr);
    SuccessOrThrow(aValue.mXpan.FromHex(xpanStr));

    aJson.at(JSON_CHANNEL).get_to(aValue.mChannel);
    aJson.at(JSON_MLP).get_to(aValue.mMlp);
    aJson.at(JSON_CCM).get_to(aValue.mCcm);
}

void to_json(json &aJson, const BorderRouter &aValue)
{
    aJson               = json{};
    aJson[JSON_ID]      = aValue.mId;
    aJson[JSON_NWK_REF] = aValue.mNetworkId;
    if (aValue.mAgent.mPresentFlags & BorderAgent::kAddrBit)
    {
        aJson[JSON_ADDR] = aValue.mAgent.mAddr;
    }
    if (aValue.mAgent.mPresentFlags & BorderAgent::kPortBit)
    {
        aJson[JSON_PORT] = aValue.mAgent.mPort;
    }
    if (aValue.mAgent.mPresentFlags & BorderAgent::kThreadVersionBit)
    {
        aJson[JSON_THREAD_VERSION] = aValue.mAgent.mThreadVersion;
    }
    if (aValue.mAgent.mPresentFlags & BorderAgent::kStateBit)
    {
        uint32_t value           = aValue.mAgent.mState;
        aJson[JSON_STATE_BITMAP] = value;
    }
    if (aValue.mAgent.mPresentFlags & BorderAgent::kVendorNameBit)
    {
        aJson[JSON_VENDOR_NAME] = aValue.mAgent.mVendorName;
    }
    if (aValue.mAgent.mPresentFlags & BorderAgent::kModelNameBit)
    {
        aJson[JSON_MODEL_NAME] = aValue.mAgent.mModelName;
    }
    if (aValue.mAgent.mPresentFlags & BorderAgent::kActiveTimestampBit)
    {
        aJson[JSON_ACTIVE_TIMESTAMP] = aValue.mAgent.mActiveTimestamp.Encode();
    }
    if (aValue.mAgent.mPresentFlags & BorderAgent::kPartitionIdBit)
    {
        aJson[JSON_PARTITION_ID] = aValue.mAgent.mPartitionId;
    }
    if (aValue.mAgent.mPresentFlags & BorderAgent::kVendorDataBit)
    {
        aJson[JSON_VENDOR_DATA] = aValue.mAgent.mVendorData;
    }
    if (aValue.mAgent.mPresentFlags & BorderAgent::kVendorOuiBit)
    {
        aJson[JSON_VENDOR_OUI] = ::ot::commissioner::utils::Hex(aValue.mAgent.mVendorOui);
    }
    if (aValue.mAgent.mPresentFlags & BorderAgent::kBbrSeqNumberBit)
    {
        aJson[JSON_BBR_SEQ_NUMBER] = aValue.mAgent.mBbrSeqNumber;
    }
    if (aValue.mAgent.mPresentFlags & BorderAgent::kBbrPortBit)
    {
        aJson[JSON_BBR_PORT] = aValue.mAgent.mBbrPort;
    }
    if (aValue.mAgent.mPresentFlags & BorderAgent::kServiceNameBit)
    {
        aJson[JSON_SERVICE_NAME] = aValue.mAgent.mServiceName;
    }
    if (aValue.mAgent.mPresentFlags & BorderAgent::kUpdateTimestampBit)
    {
        // Note that UnixTime::operator std::string() does formatting
        // here using UnixTime::kFmtString
        aJson[JSON_UPDATE_TIMESTAMP] = (std::string)aValue.mAgent.mUpdateTimestamp;
    }
}

void from_json(const json &aJson, BorderRouter &aValue)
{
    aJson.at(JSON_ID).get_to(aValue.mId);
    aValue.mAgent.mPresentFlags = 0;
    if (aJson.contains(JSON_ADDR))
    {
        aJson.at(JSON_ADDR).get_to(aValue.mAgent.mAddr);
        aValue.mAgent.mPresentFlags |= BorderAgent::kAddrBit;
    }
    if (aJson.contains(JSON_PORT))
    {
        aJson.at(JSON_PORT).get_to(aValue.mAgent.mPort);
        aValue.mAgent.mPresentFlags |= BorderAgent::kPortBit;
    }
    if (aJson.contains(JSON_THREAD_VERSION))
    {
        aJson.at(JSON_THREAD_VERSION).get_to(aValue.mAgent.mThreadVersion);
        aValue.mAgent.mPresentFlags |= BorderAgent::kThreadVersionBit;
    }
    if (aJson.contains(JSON_STATE_BITMAP))
    {
        uint32_t value;
        aJson.at(JSON_STATE_BITMAP).get_to(value);
        aValue.mAgent.mState = BorderAgent::State(value);
        aValue.mAgent.mPresentFlags |= BorderAgent::kStateBit;
    }
    if (aJson.contains(JSON_NWK_REF))
    {
        aJson.at(JSON_NWK_REF).get_to(aValue.mNetworkId);
    }
    if (aJson.contains(JSON_VENDOR_NAME))
    {
        aJson.at(JSON_VENDOR_NAME).get_to(aValue.mAgent.mVendorName);
        aValue.mAgent.mPresentFlags |= BorderAgent::kVendorNameBit;
    }
    if (aJson.contains(JSON_MODEL_NAME))
    {
        aJson.at(JSON_MODEL_NAME).get_to(aValue.mAgent.mModelName);
        aValue.mAgent.mPresentFlags |= BorderAgent::kModelNameBit;
    }
    if (aJson.contains(JSON_ACTIVE_TIMESTAMP))
    {
        uint64_t value;
        aJson.at(JSON_ACTIVE_TIMESTAMP).get_to(value);
        aValue.mAgent.mActiveTimestamp.Decode(value);
        aValue.mAgent.mPresentFlags |= BorderAgent::kActiveTimestampBit;
    }
    if (aJson.contains(JSON_PARTITION_ID))
    {
        aJson.at(JSON_PARTITION_ID).get_to(aValue.mAgent.mPartitionId);
        aValue.mAgent.mPresentFlags |= BorderAgent::kPartitionIdBit;
    }
    if (aJson.contains(JSON_VENDOR_DATA))
    {
        aJson.at(JSON_VENDOR_DATA).get_to(aValue.mAgent.mVendorData);
        aValue.mAgent.mPresentFlags |= BorderAgent::kVendorDataBit;
    }
    if (aJson.contains(JSON_VENDOR_OUI))
    {
        Error       error;
        std::string value;
        aJson.at(JSON_VENDOR_OUI).get_to(value);
        error = ot::commissioner::utils::Hex(aValue.mAgent.mVendorOui, value);
        ASSERT(error.GetCode() == ErrorCode::kNone);
        aValue.mAgent.mPresentFlags |= BorderAgent::kVendorOuiBit;
    }
    if (aJson.contains(JSON_BBR_SEQ_NUMBER))
    {
        aJson.at(JSON_BBR_SEQ_NUMBER).get_to(aValue.mAgent.mBbrSeqNumber);
        aValue.mAgent.mPresentFlags |= BorderAgent::kBbrSeqNumberBit;
    }
    if (aJson.contains(JSON_BBR_PORT))
    {
        aJson.at(JSON_BBR_PORT).get_to(aValue.mAgent.mBbrPort);
        aValue.mAgent.mPresentFlags |= BorderAgent::kBbrPortBit;
    }
    if (aJson.contains(JSON_SERVICE_NAME))
    {
        aJson.at(JSON_SERVICE_NAME).get_to(aValue.mAgent.mServiceName);
        aValue.mAgent.mPresentFlags |= BorderAgent::kServiceNameBit;
    }
    if (aJson.contains(JSON_UPDATE_TIMESTAMP))
    {
        std::string timeStr;
        aJson.at(JSON_UPDATE_TIMESTAMP).get_to(timeStr);
        UnixTime timeStamp;
        if (UnixTime::FromString(timeStamp, timeStr).GetCode() == ErrorCode::kNone && timeStamp.mTime != 0)
        {
            aValue.mAgent.mUpdateTimestamp.mTime = timeStamp.mTime;
            aValue.mAgent.mPresentFlags |= BorderAgent::kUpdateTimestampBit;
        }
    }
}

RegistrarId::RegistrarId(unsigned int aValue)
    : mId(aValue)
{
}

RegistrarId::RegistrarId()
    : RegistrarId(EMPTY_ID)
{
}

DomainId::DomainId(unsigned int aValue)
    : mId(aValue)
{
}

DomainId::DomainId()
    : DomainId(EMPTY_ID)
{
}

NetworkId::NetworkId(unsigned int aValue)
    : mId(aValue)
{
}

NetworkId::NetworkId()
    : NetworkId(EMPTY_ID)
{
}

BorderRouterId::BorderRouterId(unsigned int aValue)
    : mId(aValue)
{
}

BorderRouterId::BorderRouterId()
    : BorderRouterId(EMPTY_ID)
{
}

Registrar::Registrar(RegistrarId const &             aId,
                     std::string const &             aAddr,
                     unsigned int const              aPort,
                     std::vector<std::string> const &aDomains)
    : mId(aId)
    , mAddr(aAddr)
    , mPort(aPort)
    , mDomains(aDomains)
{
}

Registrar::Registrar()
    : Registrar(EMPTY_ID, "", 0, {})
{
}

Domain::Domain(DomainId const &aId, std::string const &aName)
    : mId(aId)
    , mName(aName)
{
}

Domain::Domain()
    : Domain(EMPTY_ID, "")
{
}

Network::Network(NetworkId const &  aId,
                 DomainId const &   aDomainId,
                 std::string const &aName,
                 XpanId const &     aXpan,
                 unsigned int const aChannel,
                 std::string const &aPan,
                 std::string const &aMlp,
                 int const          aCcm)
    : mId(aId)
    , mDomainId(aDomainId)
    , mName(aName)
    , mXpan(aXpan)
    , mChannel(aChannel)
    , mPan(aPan)
    , mMlp(aMlp)
    , mCcm(aCcm)
{
}

Network::Network()
    : Network(EMPTY_ID, EMPTY_ID, "", XpanId{}, 0, "", "", -1)
{
}

BorderRouter::BorderRouter()
    : BorderRouter(EMPTY_ID, EMPTY_ID, {})
{
}

BorderRouter::BorderRouter(BorderRouterId const &aId, NetworkId const &aNetworkId, BorderAgent const &aAgent)
    : mId{aId}
    , mNetworkId{aNetworkId}
    , mAgent{aAgent}
{
}

BorderRouter::~BorderRouter()
{
}

} // namespace persistent_storage

} // namespace commissioner

} // namespace ot
