/*
 *    Copyright (c) 2019, The OpenThread Authors.
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
 *   The file implements the config json parser;
 */

#include "json.hpp"

#include <iostream>

#include <nlohmann/json.hpp>

#include "commissioner_app.hpp"
#include <utils.hpp>

/**
 * This overrides how to serialize/deserialize a ByteArray.
 * Since standard JSON does not support hexadecimal, we represent a ByteArray as a hex string.
 */
namespace nlohmann {
template <> struct adl_serializer<ot::commissioner::ByteArray>
{
    static void to_json(json &aJson, const ot::commissioner::ByteArray &aBuf)
    {
        aJson = ot::commissioner::utils::Hex(aBuf);
    }

    static void from_json(const json &aJson, ot::commissioner::ByteArray &aBuf)
    {
        // FIXME(wgtdkp): handle the failure
        IgnoreError(::ot::commissioner::utils::Hex(aBuf, aJson.get<std::string>()));
    }
};
} // namespace nlohmann

namespace ot {

namespace commissioner {

using Json = nlohmann::json;

/**
 * Strip C-style comments in a string.
 */
static std::string StripComments(const std::string &aStr)
{
    std::string ret;
    bool        inInlineComment = false;
    bool        inBlockComment  = false;

    for (size_t i = 0; i < aStr.size();)
    {
        int c     = aStr[i];
        int nextc = i + 1 < aStr.size() ? aStr[i + 1] : '\0';

        if (inBlockComment)
        {
            if (c == '*' && nextc == '/')
            {
                inBlockComment = false;
                i += 2;
            }
            else
            {
                ++i;
            }
        }
        else if (inInlineComment)
        {
            if (c == '\r' && nextc == '\n')
            {
                inInlineComment = false;
                i += 2;
            }
            else if (c == '\n')
            {
                inInlineComment = false;
                ++i;
            }
            else
            {
                ++i;
            }
        }
        else
        {
            if (c == '/' && nextc == '*')
            {
                inBlockComment = true;
                i += 2;
            }
            else if (c == '/' && nextc == '/')
            {
                inInlineComment = true;
                i += 2;
            }
            else
            {
                ret.push_back(c);
                ++i;
            }
        }
    }

    return ret;
}

// For a json value of logging level which doesn't match
// any of below listed values, it will be defaulted to
// the first one in below map, that is LogLevel::kOff.
NLOHMANN_JSON_SERIALIZE_ENUM(LogLevel,
                             {
                                 {LogLevel::kOff, "off"},
                                 {LogLevel::kCritical, "critical"},
                                 {LogLevel::kError, "error"},
                                 {LogLevel::kWarn, "warn"},
                                 {LogLevel::kInfo, "info"},
                                 {LogLevel::kDebug, "debug"},
                             });

static void from_json(const Json &aJson, AppConfig &aAppConfig)
{
#define TEST_AND_SET(name)                         \
    if (aJson.contains(#name))                     \
    {                                              \
        aAppConfig.mConfig.m##name = aJson[#name]; \
    };

    TEST_AND_SET(Id);
    TEST_AND_SET(EnableCcm);
    TEST_AND_SET(LogLevel);
    TEST_AND_SET(EnableDtlsDebugLogging);

    TEST_AND_SET(KeepAliveInterval);
    TEST_AND_SET(MaxConnectionNum);

#undef TEST_AND_SET

#define TEST_AND_SET(name)                 \
    if (aJson.contains(#name))             \
    {                                      \
        aAppConfig.m##name = aJson[#name]; \
    };

    TEST_AND_SET(LogFile);
    TEST_AND_SET(PSKc);
    TEST_AND_SET(PrivateKeyFile);
    TEST_AND_SET(CertificateFile);
    TEST_AND_SET(TrustAnchorFile);

#undef TEST_AND_SET
}

static void to_json(Json &aJson, const AppConfig &aAppConfig)
{
#define SET_JSON_VALUE(name) aJson[#name] = aAppConfig.mConfig.m##name

    SET_JSON_VALUE(Id);
    SET_JSON_VALUE(EnableCcm);
    SET_JSON_VALUE(LogLevel);
    SET_JSON_VALUE(EnableDtlsDebugLogging);

    SET_JSON_VALUE(KeepAliveInterval);
    SET_JSON_VALUE(MaxConnectionNum);

#undef SET_JSON_VALUE

#define SET_JSON_VALUE(name) aJson[#name] = aAppConfig.m##name

    SET_JSON_VALUE(LogFile);
    SET_JSON_VALUE(PSKc);
    SET_JSON_VALUE(PrivateKeyFile);
    SET_JSON_VALUE(CertificateFile);
    SET_JSON_VALUE(TrustAnchorFile);

#undef SET_JSON_VALUE
}

static void to_json(Json &aJson, const CommissionerDataset &aDataset)
{
#define TEST_AND_SET(name)                                          \
    if (aDataset.mPresentFlags & CommissionerDataset::k##name##Bit) \
    {                                                               \
        aJson[#name] = aDataset.m##name;                            \
    };

    TEST_AND_SET(BorderAgentLocator);
    TEST_AND_SET(SessionId);
    TEST_AND_SET(SteeringData);
    TEST_AND_SET(AeSteeringData);
    TEST_AND_SET(NmkpSteeringData);
    TEST_AND_SET(JoinerUdpPort);
    TEST_AND_SET(AeUdpPort);
    TEST_AND_SET(NmkpUdpPort);

#undef TEST_AND_SET
}

static void from_json(const Json &aJson, CommissionerDataset &aDataset)
{
#define TEST_AND_SET(name)                                                    \
    if (aJson.contains(#name))                                                \
    {                                                                         \
        aDataset.m##name = aJson.at(#name).get<decltype(aDataset.m##name)>(); \
        aDataset.mPresentFlags |= CommissionerDataset::k##name##Bit;          \
    };

    // Ignore BorderAgentLocator & CommissionerSessionId

    TEST_AND_SET(SteeringData);
    TEST_AND_SET(AeSteeringData);
    TEST_AND_SET(NmkpSteeringData);
    TEST_AND_SET(JoinerUdpPort);
    TEST_AND_SET(AeUdpPort);
    TEST_AND_SET(NmkpUdpPort);

#undef TEST_AND_SET
}

static void to_json(Json &aJson, const BbrDataset &aDataset)
{
#define TEST_AND_SET(name)                                 \
    if (aDataset.mPresentFlags & BbrDataset::k##name##Bit) \
    {                                                      \
        aJson[#name] = aDataset.m##name;                   \
    };

    TEST_AND_SET(TriHostname);
    TEST_AND_SET(RegistrarHostname);
    TEST_AND_SET(RegistrarIpv6Addr);

#undef TEST_AND_SET
}

static void from_json(const Json &aJson, BbrDataset &aDataset)
{
#define TEST_AND_SET(name)                                                    \
    if (aJson.contains(#name))                                                \
    {                                                                         \
        aDataset.m##name = aJson.at(#name).get<decltype(aDataset.m##name)>(); \
        aDataset.mPresentFlags |= BbrDataset::k##name##Bit;                   \
    };

    TEST_AND_SET(TriHostname);
    TEST_AND_SET(RegistrarHostname);
    TEST_AND_SET(RegistrarIpv6Addr);

#undef TEST_AND_SET
}

static void to_json(Json &aJson, const Timestamp &aTimestamp)
{
#define SET(name) aJson[#name] = aTimestamp.m##name;

    SET(Seconds);
    SET(Ticks);
    SET(U);

#undef SET
}

static void from_json(const Json &aJson, Timestamp &aTimestamp)
{
#define SET(name) aTimestamp.m##name = aJson.at(#name).get<uint64_t>();

    SET(Seconds);
    SET(Ticks);
    SET(U);

#undef SET
}

static void to_json(Json &aJson, const Channel &aChannel)
{
#define SET(name) aJson[#name] = aChannel.m##name;

    SET(Page);
    SET(Number);

#undef SET
}

static void from_json(const Json &aJson, Channel &aChannel)
{
#define SET(name) aJson.at(#name).get_to(aChannel.m##name);

    SET(Page);
    SET(Number);

#undef SET
}

static void to_json(Json &aJson, const ChannelMaskEntry &aChannelMaskEntry)
{
#define SET(name) aJson[#name] = aChannelMaskEntry.m##name;

    SET(Page);
    SET(Masks);

#undef SET
}

static void from_json(const Json &aJson, ChannelMaskEntry &aChannelMaskEntry)
{
#define SET(name) aJson.at(#name).get_to(aChannelMaskEntry.m##name);

    SET(Page);
    SET(Masks);

#undef SET
}

static void to_json(Json &aJson, const SecurityPolicy &aSecurityPolicy)
{
#define SET(name) aJson[#name] = aSecurityPolicy.m##name;

    SET(RotationTime);
    SET(Flags);

#undef SET
}

static void from_json(const Json &aJson, SecurityPolicy &aSecurityPolicy)
{
#define SET(name) aJson.at(#name).get_to(aSecurityPolicy.m##name);

    SET(RotationTime);
    SET(Flags);

#undef SET
}

static void to_json(Json &aJson, const ActiveOperationalDataset &aDataset)
{
#define TEST_AND_SET(name)                                               \
    if (aDataset.mPresentFlags & ActiveOperationalDataset::k##name##Bit) \
    {                                                                    \
        aJson[#name] = aDataset.m##name;                                 \
    };

    TEST_AND_SET(ActiveTimestamp);
    TEST_AND_SET(Channel);
    TEST_AND_SET(ChannelMask);
    TEST_AND_SET(ExtendedPanId);

    if (aDataset.mPresentFlags & ActiveOperationalDataset::kMeshLocalPrefixBit)
    {
        aJson["MeshLocalPrefix"] = Ipv6PrefixToString(aDataset.mMeshLocalPrefix);
    };

    TEST_AND_SET(NetworkMasterKey);
    TEST_AND_SET(NetworkName);
    TEST_AND_SET(PanId);
    TEST_AND_SET(PSKc);
    TEST_AND_SET(SecurityPolicy);

#undef TEST_AND_SET
}

static void from_json(const Json &aJson, ActiveOperationalDataset &aDataset)
{
#define TEST_AND_SET(name)                                                    \
    if (aJson.contains(#name))                                                \
    {                                                                         \
        aDataset.m##name = aJson.at(#name).get<decltype(aDataset.m##name)>(); \
        aDataset.mPresentFlags |= ActiveOperationalDataset::k##name##Bit;     \
    };

    TEST_AND_SET(ActiveTimestamp);
    TEST_AND_SET(Channel);
    TEST_AND_SET(ChannelMask);
    TEST_AND_SET(ExtendedPanId);

    if (aJson.contains("MeshLocalPrefix"))
    {
        std::string prefix = aJson["MeshLocalPrefix"];
        if (Ipv6PrefixFromString(aDataset.mMeshLocalPrefix, prefix) != Error::kNone)
        {
            throw std::exception();
        }
        aDataset.mPresentFlags |= ActiveOperationalDataset::kMeshLocalPrefixBit;
    };

    TEST_AND_SET(NetworkMasterKey);
    TEST_AND_SET(NetworkName);
    TEST_AND_SET(PanId);
    TEST_AND_SET(PSKc);
    TEST_AND_SET(SecurityPolicy);

#undef TEST_AND_SET
}

static void to_json(Json &aJson, const PendingOperationalDataset &aDataset)
{
    to_json(aJson, static_cast<const ActiveOperationalDataset &>(aDataset));

#define TEST_AND_SET(name)                                                \
    if (aDataset.mPresentFlags & PendingOperationalDataset::k##name##Bit) \
    {                                                                     \
        aJson[#name] = aDataset.m##name;                                  \
    };

    TEST_AND_SET(PendingTimestamp);
    TEST_AND_SET(DelayTimer);

#undef TEST_AND_SET
}

static void from_json(const Json &aJson, PendingOperationalDataset &aDataset)
{
    from_json(aJson, static_cast<ActiveOperationalDataset &>(aDataset));

#define TEST_AND_SET(name)                                                    \
    if (aJson.contains(#name))                                                \
    {                                                                         \
        aDataset.m##name = aJson.at(#name).get<decltype(aDataset.m##name)>(); \
        aDataset.mPresentFlags |= PendingOperationalDataset::k##name##Bit;    \
    };

    TEST_AND_SET(PendingTimestamp);
    TEST_AND_SET(DelayTimer);

#undef TEST_AND_SET
}

static void to_json(Json &aJson, const NetworkData &aNetworkData)
{
#define SET(name) aJson[#name] = aNetworkData.m##name

    SET(ActiveDataset);
    SET(PendingDataset);
    SET(CommDataset);
    SET(BbrDataset);

#undef SET
}

static void from_json(const Json &aJson, NetworkData &aNetworkData)
{
#define TEST_AND_SET(name)                                                            \
    if (aJson.contains(#name))                                                        \
    {                                                                                 \
        aNetworkData.m##name = aJson.at(#name).get<decltype(aNetworkData.m##name)>(); \
    };

    TEST_AND_SET(ActiveDataset);
    TEST_AND_SET(PendingDataset);
    TEST_AND_SET(CommDataset);
    TEST_AND_SET(BbrDataset);

#undef TEST_AND_SET
}

static void to_json(Json &aJson, const EnergyReport &aEnergyReport)
{
#define SET(name) aJson[#name] = aEnergyReport.m##name

    SET(ChannelMask);
    SET(EnergyList);

#undef SET
}

Error NetworkDataFromJson(NetworkData &aNetworkData, const std::string &aJson)
{
    try
    {
        aNetworkData = Json::parse(StripComments(aJson));
        return Error::kNone;
    } catch (std::exception &e)
    {
        return Error::kBadFormat;
    }
}

std::string NetworkDataToJson(const NetworkData &aNetworkData)
{
    Json json = aNetworkData;
    return json.dump(/* indent */ 4);
}

Error CommissionerDatasetFromJson(CommissionerDataset &aDataset, const std::string &aJson)
{
    try
    {
        aDataset = Json::parse(StripComments(aJson));
        return Error::kNone;
    } catch (std::exception &e)
    {
        return Error::kBadFormat;
    }
}

std::string CommissionerDatasetToJson(const CommissionerDataset &aDataset)
{
    Json json = aDataset;
    return json.dump(/* indent */ 4);
}

Error BbrDatasetFromJson(BbrDataset &aDataset, const std::string &aJson)
{
    try
    {
        aDataset = Json::parse(StripComments(aJson));
        return Error::kNone;
    } catch (std::exception &e)
    {
        return Error::kBadFormat;
    }
}

std::string BbrDatasetToJson(const BbrDataset &aDataset)
{
    Json json = aDataset;
    return json.dump(/* indent */ 4);
}

Error ActiveDatasetFromJson(ActiveOperationalDataset &aDataset, const std::string &aJson)
{
    try
    {
        aDataset = Json::parse(StripComments(aJson));
        return Error::kNone;
    } catch (std::exception &e)
    {
        return Error::kBadFormat;
    }
}

std::string ActiveDatasetToJson(const ActiveOperationalDataset &aDataset)
{
    Json json = aDataset;
    return json.dump(/* indent */ 4);
}

Error PendingDatasetFromJson(PendingOperationalDataset &aDataset, const std::string &aJson)
{
    try
    {
        aDataset = Json::parse(StripComments(aJson));
        return Error::kNone;
    } catch (std::exception &e)
    {
        return Error::kBadFormat;
    }
}

std::string PendingDatasetToJson(const PendingOperationalDataset &aDataset)
{
    Json json = aDataset;
    return json.dump(/* indent */ 4);
}

Error AppConfigFromJson(AppConfig &aAppConfig, const std::string &aJson)
{
    try
    {
        aAppConfig = Json::parse(StripComments(aJson));
        return Error::kNone;
    } catch (std::exception &e)
    {
        return Error::kBadFormat;
    }
}

std::string AppConfigToJson(const AppConfig &aAppConfig)
{
    Json json = aAppConfig;
    return json.dump(/* indent */ 4);
}

std::string EnergyReportToJson(const EnergyReport &aEnergyReport)
{
    Json json = aEnergyReport;
    return json.dump(/* indent */ 4);
}

std::string EnergyReportMapToJson(const EnergyReportMap &aEnergyReportMap)
{
    Json json;

    // Manually create json object because the key is not std::string and
    // the JSON library will map `aEnergyReportMap` into JSON array.
    for (auto &kv : aEnergyReportMap)
    {
        auto &deviceAddr = kv.first;
        auto &report     = kv.second;

        ASSERT(deviceAddr.IsValid());
        Error       error;
        std::string addr;
        error = deviceAddr.ToString(addr);
        ASSERT(error == Error::kNone);

        json[addr] = report;
    }
    return json.dump(/* indent */ 4);
}

} // namespace commissioner

} // namespace ot
