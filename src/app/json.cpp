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
 *   This file defines the json parser of Network Data and Commissioner configuration.
 *
 */

#include "app/json.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>

#include "app/border_agent.hpp"
#include "app/commissioner_app.hpp"
#include "app/file_logger.hpp"
#include "app/file_util.hpp"
#include "commissioner/commissioner.hpp"
#include "commissioner/defines.hpp"
#include "commissioner/error.hpp"
#include "commissioner/network_data.hpp"
#include "common/error_macros.hpp"
#include "common/utils.hpp"
#include "nlohmann/json.hpp"

namespace ot {

namespace commissioner {

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

} // namespace commissioner

} // namespace ot

#define SuccessOrThrow(aError)                               \
    do                                                       \
    {                                                        \
        if (aError != ::ot::commissioner::ErrorCode::kNone)  \
        {                                                    \
            throw ::ot::commissioner::JsonException(aError); \
        }                                                    \
    } while (false)

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
        SuccessOrThrow(::ot::commissioner::utils::Hex(aBuf, aJson.get<std::string>()));
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

static void from_json(const Json &aJson, Config &aConfig)
{
#define SET_IF_PRESENT(name)            \
    if (aJson.contains(#name))          \
    {                                   \
        aConfig.m##name = aJson[#name]; \
    };

    SET_IF_PRESENT(DomainName);
    SET_IF_PRESENT(Id);
    SET_IF_PRESENT(EnableCcm);
    SET_IF_PRESENT(EnableDtlsDebugLogging);

    SET_IF_PRESENT(KeepAliveInterval);
    SET_IF_PRESENT(MaxConnectionNum);

#undef SET_IF_PRESENT

    // The default log level is LogLevel::kInfo.
    LogLevel logLevel = LogLevel::kInfo;

    if (aJson.contains("LogLevel"))
    {
        logLevel = aJson["LogLevel"];
    }

    if (aJson.contains("LogFile"))
    {
        std::shared_ptr<FileLogger> logger;

        SuccessOrThrow(FileLogger::Create(logger, aJson["LogFile"], logLevel));
        aConfig.mLogger = logger;
    }

    if (aJson.contains("PSKc"))
    {
        SuccessOrThrow(utils::Hex(aConfig.mPSKc, aJson["PSKc"]));
    }

    if (aJson.contains("PrivateKeyFile"))
    {
        SuccessOrThrow(ReadPemFile(aConfig.mPrivateKey, aJson["PrivateKeyFile"]));
    }

    if (aJson.contains("CertificateFile"))
    {
        SuccessOrThrow(ReadPemFile(aConfig.mCertificate, aJson["CertificateFile"]));
    }

    if (aJson.contains("TrustAnchorFile"))
    {
        SuccessOrThrow(ReadPemFile(aConfig.mTrustAnchor, aJson["TrustAnchorFile"]));
    }

    if (aJson.contains("ComTokFile"))
    {
        SuccessOrThrow(ReadHexStringFile(aConfig.mCommissionerToken, aJson["ComTokFile"]));
    }

    if (aJson.contains("ThreadSMRoot"))
    {
        aConfig.mThreadSMRoot = aJson["ThreadSMRoot"];
    }
}

static void to_json(Json &aJson, const CommissionerDataset &aDataset)
{
#define SET_IF_PRESENT(name)                                        \
    if (aDataset.mPresentFlags & CommissionerDataset::k##name##Bit) \
    {                                                               \
        aJson[#name] = aDataset.m##name;                            \
    };

    SET_IF_PRESENT(BorderAgentLocator);
    SET_IF_PRESENT(SessionId);
    SET_IF_PRESENT(SteeringData);
    SET_IF_PRESENT(AeSteeringData);
    SET_IF_PRESENT(NmkpSteeringData);
    SET_IF_PRESENT(JoinerUdpPort);
    SET_IF_PRESENT(AeUdpPort);
    SET_IF_PRESENT(NmkpUdpPort);

#undef SET_IF_PRESENT
}

static void from_json(const Json &aJson, CommissionerDataset &aDataset)
{
#define SET_IF_PRESENT(name)                                                  \
    if (aJson.contains(#name))                                                \
    {                                                                         \
        aDataset.m##name = aJson.at(#name).get<decltype(aDataset.m##name)>(); \
        aDataset.mPresentFlags |= CommissionerDataset::k##name##Bit;          \
    };

    // Ignore BorderAgentLocator & CommissionerSessionId

    SET_IF_PRESENT(SteeringData);
    SET_IF_PRESENT(AeSteeringData);
    SET_IF_PRESENT(NmkpSteeringData);
    SET_IF_PRESENT(JoinerUdpPort);
    SET_IF_PRESENT(AeUdpPort);
    SET_IF_PRESENT(NmkpUdpPort);

#undef SET_IF_PRESENT
}

static void to_json(Json &aJson, const BbrDataset &aDataset)
{
#define SET_IF_PRESENT(name)                               \
    if (aDataset.mPresentFlags & BbrDataset::k##name##Bit) \
    {                                                      \
        aJson[#name] = aDataset.m##name;                   \
    };

    SET_IF_PRESENT(TriHostname);
    SET_IF_PRESENT(RegistrarHostname);
    SET_IF_PRESENT(RegistrarIpv6Addr);

#undef SET_IF_PRESENT
}

static void from_json(const Json &aJson, BbrDataset &aDataset)
{
#define SET_IF_PRESENT(name)                                                  \
    if (aJson.contains(#name))                                                \
    {                                                                         \
        aDataset.m##name = aJson.at(#name).get<decltype(aDataset.m##name)>(); \
        aDataset.mPresentFlags |= BbrDataset::k##name##Bit;                   \
    };

    SET_IF_PRESENT(TriHostname);
    SET_IF_PRESENT(RegistrarHostname);
    SET_IF_PRESENT(RegistrarIpv6Addr);

#undef SET_IF_PRESENT
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
#define SET_IF_PRESENT(name)                                             \
    if (aDataset.mPresentFlags & ActiveOperationalDataset::k##name##Bit) \
    {                                                                    \
        aJson[#name] = aDataset.m##name;                                 \
    };

    SET_IF_PRESENT(ActiveTimestamp);
    SET_IF_PRESENT(NetworkName);
    SET_IF_PRESENT(Channel);
    SET_IF_PRESENT(ChannelMask);

    if (aDataset.mPresentFlags & ActiveOperationalDataset::kExtendedPanIdBit)
    {
        aJson["ExtendedPanId"] = utils::Hex(aDataset.mExtendedPanId);
    }

    if (aDataset.mPresentFlags & ActiveOperationalDataset::kPanIdBit)
    {
        aJson["PanId"] = utils::Hex(aDataset.mPanId);
    }

    if (aDataset.mPresentFlags & ActiveOperationalDataset::kMeshLocalPrefixBit)
    {
        aJson["MeshLocalPrefix"] = Ipv6PrefixToString(aDataset.mMeshLocalPrefix);
    };

    SET_IF_PRESENT(NetworkMasterKey);
    SET_IF_PRESENT(PSKc);
    SET_IF_PRESENT(SecurityPolicy);

#undef SET_IF_PRESENT
}

static void from_json(const Json &aJson, ActiveOperationalDataset &aDataset)
{
#define SET_IF_PRESENT(name)                                                  \
    if (aJson.contains(#name))                                                \
    {                                                                         \
        aDataset.m##name = aJson.at(#name).get<decltype(aDataset.m##name)>(); \
        aDataset.mPresentFlags |= ActiveOperationalDataset::k##name##Bit;     \
    };

    SET_IF_PRESENT(ActiveTimestamp);
    SET_IF_PRESENT(NetworkName);
    SET_IF_PRESENT(Channel);
    SET_IF_PRESENT(ChannelMask);

    if (aJson.contains("ExtendedPanId"))
    {
        SuccessOrThrow(utils::Hex(aDataset.mExtendedPanId, aJson["ExtendedPanId"]));
        aDataset.mPresentFlags |= ActiveOperationalDataset::kExtendedPanIdBit;
    }

    if (aJson.contains("PanId"))
    {
        SuccessOrThrow(utils::ParseInteger(aDataset.mPanId, aJson["PanId"]));
        aDataset.mPresentFlags |= ActiveOperationalDataset::kPanIdBit;
    }

    if (aJson.contains("MeshLocalPrefix"))
    {
        std::string prefix = aJson["MeshLocalPrefix"];

        SuccessOrThrow(Ipv6PrefixFromString(aDataset.mMeshLocalPrefix, prefix));
        aDataset.mPresentFlags |= ActiveOperationalDataset::kMeshLocalPrefixBit;
    };

    SET_IF_PRESENT(NetworkMasterKey);
    SET_IF_PRESENT(PSKc);
    SET_IF_PRESENT(SecurityPolicy);

#undef SET_IF_PRESENT
}

static void to_json(Json &aJson, const PendingOperationalDataset &aDataset)
{
    to_json(aJson, static_cast<const ActiveOperationalDataset &>(aDataset));

#define SET_IF_PRESENT(name)                                              \
    if (aDataset.mPresentFlags & PendingOperationalDataset::k##name##Bit) \
    {                                                                     \
        aJson[#name] = aDataset.m##name;                                  \
    };

    SET_IF_PRESENT(PendingTimestamp);
    SET_IF_PRESENT(DelayTimer);

#undef SET_IF_PRESENT
}

static void from_json(const Json &aJson, PendingOperationalDataset &aDataset)
{
    from_json(aJson, static_cast<ActiveOperationalDataset &>(aDataset));

#define SET_IF_PRESENT(name)                                                  \
    if (aJson.contains(#name))                                                \
    {                                                                         \
        aDataset.m##name = aJson.at(#name).get<decltype(aDataset.m##name)>(); \
        aDataset.mPresentFlags |= PendingOperationalDataset::k##name##Bit;    \
    };

    SET_IF_PRESENT(PendingTimestamp);
    SET_IF_PRESENT(DelayTimer);

#undef SET_IF_PRESENT
}

static void to_json(Json &aJson, const JsonNetworkData &aJsonNetworkData)
{
#define SET(name) aJson[#name] = aJsonNetworkData.m##name

    SET(ActiveDataset);
    SET(PendingDataset);
    SET(CommDataset);
    SET(BbrDataset);

#undef SET
}

static void from_json(const Json &aJson, JsonNetworkData &aJsonNetworkData)
{
#define SET_IF_PRESENT(name)                                                                  \
    if (aJson.contains(#name))                                                                \
    {                                                                                         \
        aJsonNetworkData.m##name = aJson.at(#name).get<decltype(aJsonNetworkData.m##name)>(); \
    };

    SET_IF_PRESENT(ActiveDataset);
    SET_IF_PRESENT(PendingDataset);
    SET_IF_PRESENT(CommDataset);
    SET_IF_PRESENT(BbrDataset);

#undef SET_IF_PRESENT
}

static void to_json(Json &aJson, const EnergyReport &aEnergyReport)
{
#define SET(name) aJson[#name] = aEnergyReport.m##name

    SET(ChannelMask);
    SET(EnergyList);

#undef SET
}

Error NetworkDataFromJson(JsonNetworkData &aJsonNetworkData, const std::string &aJson)
{
    Error error;
    Json  json;

    try
    {
        json = Json::parse(StripComments(aJson));
        from_json(json, aJsonNetworkData);
    } catch (JsonException &e)
    {
        error = e.GetError();
    } catch (std::exception &e)
    {
        error = {ErrorCode::kInvalidArgs, e.what()};
    }

    return error;
}

std::string NetworkDataToJson(const JsonNetworkData &aJsonNetworkData)
{
    Json json;
    to_json(json, aJsonNetworkData);
    return json.dump(JSON_INDENT_DEFAULT);
}

Error CommissionerDatasetFromJson(CommissionerDataset &aDataset, const std::string &aJson)
{
    Error error;

    aDataset = CommissionerDataset{};

    try
    {
        Json json = Json::parse(StripComments(aJson));
        if (json.is_null())
        {
            error = ERROR_NOT_FOUND("Empty commissioner dataset JSON");
        }
        else
        {
            aDataset = json;
        }
    } catch (JsonException &e)
    {
        error = e.GetError();
    } catch (std::exception &e)
    {
        error = {ErrorCode::kInvalidArgs, e.what()};
    }

    return error;
}

std::string CommissionerDatasetToJson(const CommissionerDataset &aDataset)
{
    Json json = aDataset;
    return json.dump(JSON_INDENT_DEFAULT);
}

Error BbrDatasetFromJson(BbrDataset &aDataset, const std::string &aJson)
{
    Error error;

    aDataset = BbrDataset{};

    try
    {
        Json json = Json::parse(StripComments(aJson));
        if (json.is_null())
        {
            error = ERROR_NOT_FOUND("Empty BBR dataset from JSON");
        }
        else
        {
            aDataset = json;
        }
    } catch (JsonException &e)
    {
        error = e.GetError();
    } catch (std::exception &e)
    {
        error = {ErrorCode::kInvalidArgs, e.what()};
    }

    return error;
}

std::string BbrDatasetToJson(const BbrDataset &aDataset)
{
    Json json = aDataset;
    return json.dump(JSON_INDENT_DEFAULT);
}

Error ActiveDatasetFromJson(ActiveOperationalDataset &aDataset, const std::string &aJson)
{
    Error error;

    aDataset = ActiveOperationalDataset{};

    try
    {
        Json json = Json::parse(StripComments(aJson));
        if (json.is_null())
        {
            error = ERROR_NOT_FOUND("Empty active operational dataset JSON");
        }
        else
        {
            aDataset = json;
        }

    } catch (JsonException &e)
    {
        error = e.GetError();
    } catch (std::exception &e)
    {
        error = {ErrorCode::kInvalidArgs, e.what()};
    }

    return error;
}

std::string ActiveDatasetToJson(const ActiveOperationalDataset &aDataset)
{
    Json json = aDataset;
    return json.dump(JSON_INDENT_DEFAULT);
}

Error PendingDatasetFromJson(PendingOperationalDataset &aDataset, const std::string &aJson)
{
    Error error;

    aDataset = PendingOperationalDataset{};

    try
    {
        Json json = Json::parse(StripComments(aJson));
        if (json.is_null())
        {
            error = ERROR_NOT_FOUND("Empty pending operational dataset from JSON");
        }
        else
        {
            aDataset = json;
        }
    } catch (JsonException &e)
    {
        error = e.GetError();
    } catch (std::exception &e)
    {
        error = {ErrorCode::kInvalidArgs, e.what()};
    }

    return error;
}

std::string PendingDatasetToJson(const PendingOperationalDataset &aDataset)
{
    Json json = aDataset;
    return json.dump(JSON_INDENT_DEFAULT);
}

Error ConfigFromJson(Config &aConfig, const std::string &aJson)
{
    Error error;

    try
    {
        aConfig = Json::parse(StripComments(aJson));
    } catch (JsonException &e)
    {
        error = e.GetError();
    } catch (std::exception &e)
    {
        error = {ErrorCode::kInvalidArgs, e.what()};
    }

    return error;
}

std::string EnergyReportToJson(const EnergyReport &aEnergyReport)
{
    Json json = aEnergyReport;
    return json.dump(JSON_INDENT_DEFAULT);
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

        VerifyOrDie(deviceAddr.IsValid());
        json[deviceAddr.ToString()] = report;
    }
    return json.dump(JSON_INDENT_DEFAULT);
}

static void to_json(Json &aJson, const NetDiagData &aNetDiagData)
{
#define SET_IF_PRESENT(name)                                    \
    if (aNetDiagData.mPresentFlags & NetDiagData::k##name##Bit) \
    {                                                           \
        aJson[#name] = aNetDiagData.m##name;                    \
    };

    SET_IF_PRESENT(ExtMacAddr);
    SET_IF_PRESENT(MacAddr);
    SET_IF_PRESENT(Timeout);
    SET_IF_PRESENT(BatteryLevel);
    SET_IF_PRESENT(SupplyVoltage);
    SET_IF_PRESENT(ChannelPages);
    SET_IF_PRESENT(TypeList);
#undef SET_IF_PRESENT
    if (aNetDiagData.mPresentFlags & NetDiagData::kMacCountersBit)
    {
        aJson["MacCounters"] = MacCountersToJson(aNetDiagData.mMacCounters);
    }
    if (aNetDiagData.mPresentFlags & NetDiagData::kConnectivityBit)
    {
        aJson["Connectivity"] = ConnectivityToJson(aNetDiagData.mConnectivity);
    }
}

std::string NetDiagDataToJson(const NetDiagData &aNetDiagData)
{
    Json json;
    to_json(json, aNetDiagData);
    return json.dump(JSON_INDENT_DEFAULT);
}

static void to_json(Json &aJson, const MacCounters &aMacCounters)
{
#define SET(name) aJson[#name] = aMacCounters.m##name;
    SET(IfInUnknownProtos);
    SET(IfInErrors);
    SET(IfOutErrors);
    SET(IfInUcastPkts);
    SET(IfInBroadcastPkts);
    SET(IfInDiscards);
    SET(IfOutUcastPkts);
    SET(IfOutBroadcastPkts);
    SET(IfOutDiscards);
#undef SET
}

std::string MacCountersToJson(const MacCounters &aMacCounters)
{
    Json json;
    to_json(json, aMacCounters);
    return json.dump(JSON_INDENT_DEFAULT);
}

static void to_json(Json &aJson, const Connectivity &aConnectivity)
{
#define SET(name) aJson[#name] = aConnectivity.m##name;
    SET(ParentPriority);
    SET(LinkQuality3);
    SET(LinkQuality2);
    SET(LinkQuality1);
    SET(LeaderCost);
    SET(IdSequence);
    SET(ActiveRouters);
#undef SET

    if (aConnectivity.mPresentFlags & Connectivity::kRxOffChildBufferSizeBit)
    {
        aJson["RxOffChildBufferSize"] = aConnectivity.mRxOffChildBufferSize;
    }

    if (aConnectivity.mPresentFlags & Connectivity::kRxOffChildDatagramCountBit)
    {
        aJson["RxOffChildDatagramCount"] = aConnectivity.mRxOffChildDatagramCount;
    }
}

std::string ConnectivityToJson(const Connectivity &aConnectivity)
{
    Json json;
    to_json(json, aConnectivity);
    return json.dump(JSON_INDENT_DEFAULT);
}

Error JsonFromFile(std::string &aJson, const std::string &aPath)
{
    Error       error;
    std::string jsonStr;

    SuccessOrExit(error = ReadFile(jsonStr, aPath));
    try
    {
        Json json = Json::parse(StripComments(jsonStr));
        aJson     = json.dump(JSON_INDENT_DEFAULT);
    } catch (JsonException &e)
    {
        error = {ErrorCode::kBadFormat, e.GetError().GetMessage()};
    } catch (std::exception &e)
    {
        error = {ErrorCode::kBadFormat, e.what()};
    }
exit:
    return error;
}

static void from_json(const nlohmann::json &aJson, BorderAgent::State &aState)
{
    uint32_t stateVal = aJson.get<uint32_t>();
    aState            = BorderAgent::State(stateVal);
}

void BorderAgentFromJson(BorderAgent &aAgent, const nlohmann::json &aJson)
{
    BorderAgent agent;

#define SET_IF_PRESENT(field)                                                  \
    do                                                                         \
    {                                                                          \
        if (aJson.contains(#field))                                            \
        {                                                                      \
            agent.mPresentFlags |= BorderAgent::k##field##Bit;                 \
            agent.m##field = aJson.at(#field).get<decltype(agent.m##field)>(); \
        }                                                                      \
    } while (false)

    SET_IF_PRESENT(Addr);
    SET_IF_PRESENT(Port);
    SET_IF_PRESENT(ThreadVersion);
    SET_IF_PRESENT(State);
    SET_IF_PRESENT(NetworkName);

    if (aJson.contains("ExtendedPanId"))
    {
        SuccessOrThrow(utils::ParseInteger(agent.mExtendedPanId, aJson["ExtendedPanId"]));
        agent.mPresentFlags |= BorderAgent::kExtendedPanIdBit;
    }

    SET_IF_PRESENT(VendorName);
    SET_IF_PRESENT(ModelName);
    SET_IF_PRESENT(ActiveTimestamp);
    SET_IF_PRESENT(PartitionId);
    SET_IF_PRESENT(VendorData);
    SET_IF_PRESENT(VendorOui);
    SET_IF_PRESENT(DomainName);
    SET_IF_PRESENT(BbrSeqNumber);
    SET_IF_PRESENT(BbrPort);
    SET_IF_PRESENT(ServiceName);

#undef SET_IF_PRESENT

    aAgent = agent;
}

static void to_json(nlohmann::json &aJson, const BorderAgent::State &aState) { aJson = static_cast<uint32_t>(aState); }

void BorderAgentToJson(const BorderAgent &aAgent, nlohmann::json &aJson)
{
    BorderAgent agent;

#define SET_IF_PRESENT(field)                                  \
    do                                                         \
    {                                                          \
        if (aAgent.mPresentFlags & BorderAgent::k##field##Bit) \
        {                                                      \
            aJson[#field] = aAgent.m##field;                   \
        }                                                      \
    } while (false)

    SET_IF_PRESENT(Addr);
    SET_IF_PRESENT(Port);
    SET_IF_PRESENT(ThreadVersion);
    SET_IF_PRESENT(State);
    SET_IF_PRESENT(NetworkName);

    if (aAgent.mPresentFlags & BorderAgent::kExtendedPanIdBit)
    {
        aJson["ExtendedPanId"] = utils::Hex(aAgent.mExtendedPanId);
    }

    SET_IF_PRESENT(VendorName);
    SET_IF_PRESENT(ModelName);
    SET_IF_PRESENT(ActiveTimestamp);
    SET_IF_PRESENT(PartitionId);
    SET_IF_PRESENT(VendorData);
    SET_IF_PRESENT(VendorOui);
    SET_IF_PRESENT(DomainName);
    SET_IF_PRESENT(BbrSeqNumber);
    SET_IF_PRESENT(BbrPort);
    SET_IF_PRESENT(ServiceName);

#undef SET_IF_PRESENT
}

} // namespace commissioner

} // namespace ot

#undef SuccessOrThrow
