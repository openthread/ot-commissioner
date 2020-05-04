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
 *   The file implements CLI interpreter.
 */

#include "app/cli/interpreter.hpp"

#include <limits>

#include <string.h>

#include "app/file_util.hpp"
#include "app/json.hpp"
#include "common/utils.hpp"

#if defined(SuccessOrExit)
#undef SuccessOrExit
#define SuccessOrExit(aError, ...)    \
    do                                \
    {                                 \
        if ((aError) != Error::kNone) \
        {                             \
            __VA_ARGS__;              \
            goto exit;                \
        }                             \
    } while (false)
#endif // defined(SuccessOrExit)

namespace ot {

namespace commissioner {

const std::map<std::string, Interpreter::Evaluator> &Interpreter::mEvaluatorMap = *new std::map<std::string, Evaluator>{
    {"start", &Interpreter::ProcessStart},
    {"stop", &Interpreter::ProcessStop},
    {"active", &Interpreter::ProcessActive},
    {"token", &Interpreter::ProcessToken},
    {"network", &Interpreter::ProcessNetwork},
    {"sessionid", &Interpreter::ProcessSessionId},
    {"borderagent", &Interpreter::ProcessBorderAgent},
    {"joiner", &Interpreter::ProcessJoiner},
    {"commdataset", &Interpreter::ProcessCommDataset},
    {"opdataset", &Interpreter::ProcessOpDataset},
    {"bbrdataset", &Interpreter::ProcessBbrDataset},
    {"reenroll", &Interpreter::ProcessReenroll},
    {"domainreset", &Interpreter::ProcessDomainReset},
    {"migrate", &Interpreter::ProcessMigrate},
    {"mlr", &Interpreter::ProcessMlr},
    {"announce", &Interpreter::ProcessAnnounce},
    {"panid", &Interpreter::ProcessPanId},
    {"energy", &Interpreter::ProcessEnergy},
    {"exit", &Interpreter::ProcessExit},
    {"help", &Interpreter::ProcessHelp},
};

const std::map<std::string, std::string> &Interpreter::mUsageMap = *new std::map<std::string, std::string>{
    {"start", "start <border-agent-addr> <border-agent-port>"},
    {"stop", "stop"},
    {"active", "active"},
    {"token", "token request <registrar-addr> <registrar-port>\n"
              "token print\n"
              "token set <signed-token-hex-string-file> <signer-cert-pem-file>"},
    {"network", "network save <network-data-file>\n"
                "network sync"},
    {"sessionid", "sessionid"},
    {"borderagent", "borderagent discover [<timeout-in-milliseconds>]\n"
                    "borderagent get locator\n"
                    "borderagent get meshlocaladdr"},
    {"joiner", "joiner enable (meshcop|ae|nmkp) <joiner-eui64> [<joiner-password>] [<provisioning-url>]\n"
               "joiner enableall (meshcop|ae|nmkp) [<joiner-password>] [<provisioning-url>]\n"
               "joiner disable (meshcop|ae|nmkp) <joiner-eui64>\n"
               "joiner disableall (meshcop|ae|nmkp)\n"
               "joiner getport (meshcop|ae|nmkp)\n"
               "joiner setport (meshcop|ae|nmkp) <joiner-udp-port>"},
    {"commdataset", "commdataset get\n"
                    "commdataset set '<commissioner-dataset-in-json-string>'"},
    {"opdataset", "opdataset get activetimestamp\n"
                  "opdataset get channel\n"
                  "opdataset set channel <page> <channel> <delay-in-milliseconds>\n"
                  "opdataset get channelmask\n"
                  "opdataset set channelmask (<page> <channel-mask>)...\n"
                  "opdataset get xpanid\n"
                  "opdataset set xpanid <extended-pan-id>\n"
                  "opdataset get meshlocalprefix\n"
                  "opdataset set meshlocalprefix <prefix> <delay-in-milliseconds>\n"
                  "opdataset get networkmasterkey\n"
                  "opdataset set networkmasterkey <network-master-key> <delay-in-milliseconds>\n"
                  "opdataset get networkname\n"
                  "opdataset set networkname <network-name>\n"
                  "opdataset get panid\n"
                  "opdataset set panid <panid> <delay-in-milliseconds>\n"
                  "opdataset get pskc\n"
                  "opdataset set pskc <PSKc>\n"
                  "opdataset get securitypolicy\n"
                  "opdataset set securitypolicy <rotation-timer> <flags-hex>\n"
                  "opdataset get active\n"
                  "opdataset set active '<active-dataset-in-json-string>'\n"
                  "opdataset get pending\n"
                  "opdataset set pending '<pending-dataset-in-json-string>'"},
    {"bbrdataset", "bbrdataset get trihostname\n"
                   "bbrdataset set trihostname <TRI-hostname>\n"
                   "bbrdataset get reghostname\n"
                   "bbrdataset set reghostname <registrar-hostname>\n"
                   "bbrdataset get regaddr\n"
                   "bbrdataset get\n"
                   "bbrdataset set '<bbr-dataset-in-json-string>'"},
    {"reenroll", "reenroll <device-addr>"},
    {"domainreset", "domainreset <device-addr>"},
    {"migrate", "migrate <device-addr> <designated-network-name>"},
    {"mlr", "mlr (<multicast-addr>)+ <timeout-in-seconds>"},
    {"announce", "announce <channel-mask> <count> <period> <dst-addr>"},
    {"panid", "panid query <channel-mask> <panid> <dst-addr>\n"
              "panid conflict <panid>"},
    {"energy", "energy scan <channel-mask> <count> <period> <scan-duration> <dst-addr>\n"
               "energy report [<dst-addr>]"},
    {"help", "help [<command>]"},
};

template <typename T> static std::string ToHex(T aInteger)
{
    return "0x" + utils::Hex(utils::Encode(aInteger));
};

template <typename T> static Error ParseInteger(T &aInteger, const std::string &aStr)
{
    Error    error;
    uint64_t integer;
    char *   endPtr = nullptr;

    integer = strtoull(aStr.c_str(), &endPtr, 0);

    VerifyOrExit(endPtr != nullptr && endPtr > aStr.c_str(), error = Error::kFailed);

    aInteger = integer;
    error    = Error::kNone;

exit:
    return error;
}

std::string ToLower(const std::string &aStr)
{
    std::string ret = aStr;
    for (auto &c : ret)
    {
        c = tolower(c);
    }
    return ret;
}

static inline bool CaseInsensitiveEqual(const std::string &aLhs, const std::string &aRhs)
{
    return ToLower(aLhs) == ToLower(aRhs);
}

Error Interpreter::Init(const std::string &aConfigFile)
{
    Error error = Error::kNone;

    std::string configJson;
    Config      config;

    SuccessOrExit(error = ReadFile(configJson, aConfigFile));
    SuccessOrExit(error = ConfigFromJson(config, configJson));

    mCommissioner = CommissionerApp::Create(config);

    VerifyOrExit(mCommissioner != nullptr, error = Error::kInvalidArgs);

exit:
    return error;
}

void Interpreter::Run()
{
    Expression expr;

    VerifyOrExit(mCommissioner != nullptr);

    while (!mShouldExit)
    {
        Print(Eval(Read()));
    }

exit:
    return;
}

void Interpreter::AbortCommand()
{
    if (mCommissioner->IsActive())
    {
        mCommissioner->AbortRequests();
    }
    else
    {
        mCommissioner->Stop();
    }
}

Interpreter::Expression Interpreter::Read()
{
    return ParseExpression(mConsole.Read());
}

Interpreter::Value Interpreter::Eval(const Expression &aExpr)
{
    Value                                            value;
    std::map<std::string, Evaluator>::const_iterator evaluator;

    VerifyOrExit(!aExpr.empty(), value = Error::kNone);

    evaluator = mEvaluatorMap.find(ToLower(aExpr.front()));
    if (evaluator == mEvaluatorMap.end())
    {
        ExitNow(value = {Error::kInvalidArgs, "invalid commands: " + aExpr.front() + "; type 'help' for all commands"});
    }

    value = evaluator->second(this, aExpr);

exit:
    return value;
}

void Interpreter::Print(const Value &aValue)
{
    std::string    output;
    Console::Color color;

    if (aValue.mError == Error::kNone)
    {
        output = aValue.mMessage.empty() ? "" : (aValue.mMessage + "\n");
        output += "[done]";
        color = Console::Color::kGreen;
    }
    else
    {
        output = aValue.mMessage.empty() ? "" : (aValue.mMessage + "\n");
        output += "[failed]: " + ErrorToString(aValue.mError);
        color = Console::Color::kRed;
    }

    mConsole.Write(output, color);
}

Interpreter::Expression Interpreter::ParseExpression(const std::string &aLiteral)
{
    Expression expr;

    bool inSingleQuotes = false;
    auto begin          = aLiteral.end();
    for (auto c = aLiteral.begin(); c != aLiteral.end(); ++c)
    {
        if (inSingleQuotes)
        {
            if (*c == '\'')
            {
                VerifyOrDie(begin != aLiteral.end());
                expr.emplace_back(begin, c);
                begin          = aLiteral.end();
                inSingleQuotes = false;
            }
        }
        else
        {
            if (isspace(*c))
            {
                if (begin != aLiteral.end())
                {
                    expr.emplace_back(begin, c);
                    begin = aLiteral.end();
                }
            }
            else if (*c == '\'')
            {
                inSingleQuotes = true;
                begin          = c + 1;
            }
            else if (begin == aLiteral.end())
            {
                begin = c;
            }
        }
    }

    if (begin != aLiteral.end())
    {
        expr.emplace_back(begin, aLiteral.end());
    }

    return expr;
}

Interpreter::Value Interpreter::ProcessStart(const Expression &aExpr)
{
    Error       error = Error::kInvalidArgs;
    std::string msg;
    std::string existingCommissionerId;
    uint16_t    port;

    VerifyOrExit(aExpr.size() >= 3, msg = Usage(aExpr));
    SuccessOrExit(ParseInteger(port, aExpr[2]), msg = aExpr[2]);
    SuccessOrExit(error = mCommissioner->Start(existingCommissionerId, aExpr[1], port));

exit:
    if (!existingCommissionerId.empty())
    {
        msg = "there is an existing active commissioner: " + existingCommissionerId;
    }
    return {error, msg};
}

Interpreter::Value Interpreter::ProcessStop(const Expression &)
{
    mCommissioner->Stop();
    return Error::kNone;
}

Interpreter::Value Interpreter::ProcessActive(const Expression &)
{
    return {Error::kNone, mCommissioner->IsActive() ? "true" : "false"};
}

Interpreter::Value Interpreter::ProcessToken(const Expression &aExpr)
{
    Error       error = Error::kInvalidArgs;
    std::string msg;

    VerifyOrExit(aExpr.size() >= 2, msg = Usage(aExpr));

    if (CaseInsensitiveEqual(aExpr[1], "request"))
    {
        uint16_t port;
        VerifyOrExit(aExpr.size() >= 4, msg = Usage(aExpr));
        SuccessOrExit(ParseInteger(port, aExpr[3]), msg = aExpr[3]);
        SuccessOrExit(error = mCommissioner->RequestToken(aExpr[2], port));
    }
    else if (CaseInsensitiveEqual(aExpr[1], "print"))
    {
        auto signedToken = mCommissioner->GetToken();
        VerifyOrExit(!signedToken.empty(), {
            error = Error::kNotFound;
            msg   = "commissioner token not set";
        });
        error = Error::kNone;
        msg   = utils::Hex(signedToken);
    }
    else if (CaseInsensitiveEqual(aExpr[1], "set"))
    {
        ByteArray signedToken, signerCert;
        VerifyOrExit(aExpr.size() >= 4, msg = Usage(aExpr));
        SuccessOrExit(error = ReadHexStringFile(signedToken, aExpr[2]));
        SuccessOrExit(error = ReadPemFile(signerCert, aExpr[3]));

        SuccessOrExit(error = mCommissioner->SetToken(signedToken, signerCert));
    }
    else
    {
        ExitNow(msg = aExpr[1]);
    }

exit:
    return {error, msg};
}

Interpreter::Value Interpreter::ProcessNetwork(const Expression &aExpr)
{
    Error       error = Error::kInvalidArgs;
    std::string msg;

    VerifyOrExit(aExpr.size() >= 2, msg = Usage(aExpr));

    if (CaseInsensitiveEqual(aExpr[1], "save"))
    {
        VerifyOrExit(aExpr.size() >= 3, msg = Usage(aExpr));
        SuccessOrExit(error = mCommissioner->SaveNetworkData(aExpr[2]));
    }
    else if (CaseInsensitiveEqual(aExpr[1], "sync"))
    {
        SuccessOrExit(error = mCommissioner->SyncNetworkData());
    }
    else
    {
        ExitNow(msg = aExpr[1]);
    }

exit:
    return {error, msg};
}

Interpreter::Value Interpreter::ProcessSessionId(const Expression &)
{
    Value    value;
    uint16_t sessionId;

    SuccessOrExit(value.mError = mCommissioner->GetSessionId(sessionId));
    value.mMessage = std::to_string(sessionId);

exit:
    return value;
}

Interpreter::Value Interpreter::ProcessBorderAgent(const Expression &aExpr)
{
    Error       error = Error::kInvalidArgs;
    std::string msg;

    VerifyOrExit(aExpr.size() >= 2, msg = Usage(aExpr));

    if (CaseInsensitiveEqual(aExpr[1], "discover"))
    {
        uint64_t timeout = 4000;

        if (aExpr.size() >= 3)
        {
            SuccessOrExit(ParseInteger(timeout, aExpr[2]), msg = aExpr[2]);
        }

        SuccessOrExit(error = DiscoverBorderAgent(BorderAgentHandler, static_cast<size_t>(timeout)));

        ExitNow(error = Error::kNone);
    }
    else if (CaseInsensitiveEqual(aExpr[1], "get"))
    {
        VerifyOrExit(aExpr.size() >= 3, msg = Usage(aExpr));

        if (CaseInsensitiveEqual(aExpr[2], "locator"))
        {
            uint16_t locator;
            SuccessOrExit(error = mCommissioner->GetBorderAgentLocator(locator));
            msg = ToHex(locator);
        }
        else if (CaseInsensitiveEqual(aExpr[2], "meshlocaladdr"))
        {
            uint16_t    locator;
            std::string meshLocalPrefix;
            std::string meshLocalAddr;
            SuccessOrExit(error = mCommissioner->GetBorderAgentLocator(locator));
            SuccessOrExit(error = mCommissioner->GetMeshLocalPrefix(meshLocalPrefix));
            SuccessOrExit(error = Commissioner::GetMeshLocalAddr(meshLocalAddr, meshLocalPrefix, locator));
            msg = meshLocalAddr;
        }
        else
        {
            msg = aExpr[2];
        }
    }
    else
    {
        msg = aExpr[1];
    }

exit:
    return {error, msg};
}

Interpreter::Value Interpreter::ProcessJoiner(const Expression &aExpr)
{
    Error       error = Error::kInvalidArgs;
    std::string msg;

    JoinerType type;

    VerifyOrExit(aExpr.size() >= 3, msg = Usage(aExpr));
    SuccessOrExit(GetJoinerType(type, aExpr[2]), msg = aExpr[2]);

    if (CaseInsensitiveEqual(aExpr[1], "enable"))
    {
        uint64_t    eui64;
        std::string pskd;
        std::string provisioningUrl;

        VerifyOrExit(aExpr.size() >= (type == JoinerType::kMeshCoP ? 5 : 4), msg = Usage(aExpr));
        SuccessOrExit(ParseInteger(eui64, aExpr[3]), msg = aExpr[3]);
        if (type == JoinerType::kMeshCoP)
        {
            pskd = aExpr[4];
            if (aExpr.size() >= 6)
            {
                provisioningUrl = aExpr[5];
            }
        }

        SuccessOrExit(error = mCommissioner->EnableJoiner(type, eui64, pskd, provisioningUrl));
    }
    else if (CaseInsensitiveEqual(aExpr[1], "enableall"))
    {
        std::string pskd;
        std::string provisioningUrl;
        if (type == JoinerType::kMeshCoP)
        {
            VerifyOrExit(aExpr.size() >= 4, msg = Usage(aExpr));
            pskd = aExpr[3];
            if (aExpr.size() >= 5)
            {
                provisioningUrl = aExpr[4];
            }
        }

        SuccessOrExit(error = mCommissioner->EnableAllJoiners(type, pskd, provisioningUrl));
    }
    else if (CaseInsensitiveEqual(aExpr[1], "disable"))
    {
        uint64_t eui64;
        VerifyOrExit(aExpr.size() >= 4, msg = Usage(aExpr));
        SuccessOrExit(ParseInteger(eui64, aExpr[3]), msg = aExpr[3]);
        SuccessOrExit(error = mCommissioner->DisableJoiner(type, eui64));
    }
    else if (CaseInsensitiveEqual(aExpr[1], "disableall"))
    {
        SuccessOrExit(error = mCommissioner->DisableAllJoiners(type));
    }
    else if (CaseInsensitiveEqual(aExpr[1], "getport"))
    {
        uint16_t port;
        SuccessOrExit(error = mCommissioner->GetJoinerUdpPort(port, type));
        msg = std::to_string(port);
    }
    else if (CaseInsensitiveEqual(aExpr[1], "setport"))
    {
        uint16_t port;
        VerifyOrExit(aExpr.size() >= 4, msg = Usage(aExpr));
        SuccessOrExit(ParseInteger(port, aExpr[3]), msg = aExpr[3]);
        SuccessOrExit(error = mCommissioner->SetJoinerUdpPort(type, port));
    }
    else
    {
        msg = aExpr[1];
    }

exit:
    return {error, msg};
}

Interpreter::Value Interpreter::ProcessCommDataset(const Expression &aExpr)
{
    Error       error = Error::kInvalidArgs;
    std::string msg;

    VerifyOrExit(aExpr.size() >= 2, msg = Usage(aExpr));

    if (CaseInsensitiveEqual(aExpr[1], "get"))
    {
        CommissionerDataset dataset;
        SuccessOrExit(error = mCommissioner->GetCommissionerDataset(dataset, 0xFFFF));
        msg = CommissionerDatasetToJson(dataset);
    }
    else if (CaseInsensitiveEqual(aExpr[1], "set"))
    {
        CommissionerDataset dataset;
        VerifyOrExit(aExpr.size() >= 3, msg = Usage(aExpr));
        SuccessOrExit(error = CommissionerDatasetFromJson(dataset, aExpr[2]));
        SuccessOrExit(error = mCommissioner->SetCommissionerDataset(dataset));
    }
    else
    {
        msg = aExpr[1];
    }

exit:
    return {error, msg};
}

Interpreter::Value Interpreter::ProcessOpDataset(const Expression &aExpr)
{
    Error       error = Error::kInvalidArgs;
    std::string msg;

    bool IsSet;

    VerifyOrExit(aExpr.size() >= 3, msg = Usage(aExpr));
    if (CaseInsensitiveEqual(aExpr[1], "get"))
    {
        IsSet = false;
    }
    else if (CaseInsensitiveEqual(aExpr[1], "set"))
    {
        IsSet = true;
    }
    else
    {
        ExitNow(msg = aExpr[1]);
    }

    if (CaseInsensitiveEqual(aExpr[2], "activetimestamp"))
    {
        Timestamp activeTimestamp;
        VerifyOrExit(!IsSet, msg = "can't set activetimestamp");
        SuccessOrExit(error = mCommissioner->GetActiveTimestamp(activeTimestamp));
        msg = ToString(activeTimestamp);
    }
    else if (CaseInsensitiveEqual(aExpr[2], "channel"))
    {
        Channel channel;
        if (IsSet)
        {
            uint32_t delay;
            VerifyOrExit(aExpr.size() >= 6, msg = Usage(aExpr));
            SuccessOrExit(ParseInteger(channel.mPage, aExpr[3]), msg = aExpr[3]);
            SuccessOrExit(ParseInteger(channel.mNumber, aExpr[4]), msg = aExpr[4]);
            SuccessOrExit(ParseInteger(delay, aExpr[5]), msg = aExpr[5]);
            SuccessOrExit(error = mCommissioner->SetChannel(channel, CommissionerApp::MilliSeconds(delay)));
        }
        else
        {
            SuccessOrExit(error = mCommissioner->GetChannel(channel));
            msg = ToString(channel);
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "channelmask"))
    {
        ChannelMask channelMask;
        if (IsSet)
        {
            SuccessOrExit(ParseChannelMask(channelMask, aExpr, 3), msg = Usage(aExpr));
            SuccessOrExit(error = mCommissioner->SetChannelMask(channelMask));
        }
        else
        {
            SuccessOrExit(error = mCommissioner->GetChannelMask(channelMask));
            msg = ToString(channelMask);
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "xpanid"))
    {
        ByteArray xpanid;
        if (IsSet)
        {
            VerifyOrExit(aExpr.size() >= 4, msg = Usage(aExpr));
            SuccessOrExit(utils::Hex(xpanid, aExpr[3]), msg = aExpr[3]);
            SuccessOrExit(error = mCommissioner->SetExtendedPanId(xpanid));
        }
        else
        {
            SuccessOrExit(error = mCommissioner->GetExtendedPanId(xpanid));
            msg = utils::Hex(xpanid);
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "meshlocalprefix"))
    {
        std::string prefix;
        if (IsSet)
        {
            uint32_t delay;
            VerifyOrExit(aExpr.size() >= 5, msg = Usage(aExpr));
            SuccessOrExit(ParseInteger(delay, aExpr[4]), msg = aExpr[4]);
            SuccessOrExit(error = mCommissioner->SetMeshLocalPrefix(aExpr[3], CommissionerApp::MilliSeconds(delay)));
        }
        else
        {
            SuccessOrExit(error = mCommissioner->GetMeshLocalPrefix(prefix));
            msg = prefix;
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "networkmasterkey"))
    {
        ByteArray masterKey;
        if (IsSet)
        {
            uint32_t delay;
            VerifyOrExit(aExpr.size() >= 5, msg = Usage(aExpr));
            SuccessOrExit(utils::Hex(masterKey, aExpr[3]), msg = aExpr[3]);
            SuccessOrExit(ParseInteger(delay, aExpr[4]), msg = aExpr[4]);
            SuccessOrExit(error = mCommissioner->SetNetworkMasterKey(masterKey, CommissionerApp::MilliSeconds(delay)));
        }
        else
        {
            SuccessOrExit(error = mCommissioner->GetNetworkMasterKey(masterKey));
            msg = utils::Hex(masterKey);
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "networkname"))
    {
        std::string networkName;
        if (IsSet)
        {
            VerifyOrExit(aExpr.size() >= 4, msg = Usage(aExpr));
            SuccessOrExit(error = mCommissioner->SetNetworkName(aExpr[3]));
        }
        else
        {
            SuccessOrExit(error = mCommissioner->GetNetworkName(networkName));
            msg = networkName;
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "panid"))
    {
        uint16_t panid;
        if (IsSet)
        {
            uint32_t delay;
            VerifyOrExit(aExpr.size() >= 5, msg = Usage(aExpr));
            SuccessOrExit(ParseInteger(panid, aExpr[3]), msg = aExpr[3]);
            SuccessOrExit(ParseInteger(delay, aExpr[4]), msg = aExpr[4]);
            SuccessOrExit(error = mCommissioner->SetPanId(panid, CommissionerApp::MilliSeconds(delay)));
        }
        else
        {
            SuccessOrExit(error = mCommissioner->GetPanId(panid));
            msg = std::to_string(panid);
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "pskc"))
    {
        ByteArray pskc;
        if (IsSet)
        {
            VerifyOrExit(aExpr.size() >= 4, msg = Usage(aExpr));
            SuccessOrExit(utils::Hex(pskc, aExpr[3]), msg = aExpr[3]);
            SuccessOrExit(error = mCommissioner->SetPSKc(pskc));
        }
        else
        {
            SuccessOrExit(error = mCommissioner->GetPSKc(pskc));
            msg = utils::Hex(pskc);
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "securitypolicy"))
    {
        SecurityPolicy policy;
        if (IsSet)
        {
            VerifyOrExit(aExpr.size() >= 5, msg = Usage(aExpr));
            SuccessOrExit(ParseInteger(policy.mRotationTime, aExpr[3]), msg = aExpr[3]);
            SuccessOrExit(utils::Hex(policy.mFlags, aExpr[4]), msg = aExpr[4]);
            SuccessOrExit(error = mCommissioner->SetSecurityPolicy(policy));
        }
        else
        {
            SuccessOrExit(error = mCommissioner->GetSecurityPolicy(policy));
            msg = ToString(policy);
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "active"))
    {
        ActiveOperationalDataset dataset;
        if (IsSet)
        {
            VerifyOrExit(aExpr.size() >= 4, msg = Usage(aExpr));
            SuccessOrExit(error = ActiveDatasetFromJson(dataset, aExpr[3]));
            SuccessOrExit(error = mCommissioner->SetActiveDataset(dataset));
        }
        else
        {
            SuccessOrExit(error = mCommissioner->GetActiveDataset(dataset, 0xFFFF));
            msg = ActiveDatasetToJson(dataset);
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "pending"))
    {
        PendingOperationalDataset dataset;
        if (IsSet)
        {
            VerifyOrExit(aExpr.size() >= 4, msg = Usage(aExpr));
            SuccessOrExit(error = PendingDatasetFromJson(dataset, aExpr[3]));
            SuccessOrExit(error = mCommissioner->SetPendingDataset(dataset));
        }
        else
        {
            SuccessOrExit(error = mCommissioner->GetPendingDataset(dataset, 0xFFFF));
            msg = PendingDatasetToJson(dataset);
        }
    }
    else
    {
        ExitNow(msg = aExpr[2]);
    }

exit:
    return {error, msg};
}

Interpreter::Value Interpreter::ProcessBbrDataset(const Expression &aExpr)
{
    Error       error = Error::kInvalidArgs;
    std::string msg;

    bool IsSet;

    VerifyOrExit(aExpr.size() >= 2, msg = Usage(aExpr));
    if (CaseInsensitiveEqual(aExpr[1], "get"))
    {
        IsSet = false;
    }
    else if (CaseInsensitiveEqual(aExpr[1], "set"))
    {
        IsSet = true;
    }
    else
    {
        ExitNow(msg = aExpr[1]);
    }

    if (aExpr.size() == 2 && !IsSet)
    {
        BbrDataset dataset;
        SuccessOrExit(error = mCommissioner->GetBbrDataset(dataset, 0xFFFF));
        ExitNow(msg = BbrDatasetToJson(dataset));
    }

    VerifyOrExit(aExpr.size() >= 3, msg = Usage(aExpr));

    if (CaseInsensitiveEqual(aExpr[2], "trihostname"))
    {
        std::string triHostname;
        if (IsSet)
        {
            VerifyOrExit(aExpr.size() >= 4, msg = Usage(aExpr));
            SuccessOrExit(error = mCommissioner->SetTriHostname(aExpr[3]));
        }
        else
        {
            SuccessOrExit(error = mCommissioner->GetTriHostname(triHostname));
            msg = triHostname;
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "reghostname"))
    {
        std::string regHostname;
        if (IsSet)
        {
            VerifyOrExit(aExpr.size() >= 4, msg = Usage(aExpr));
            SuccessOrExit(error = mCommissioner->SetRegistrarHostname(aExpr[3]));
        }
        else
        {
            SuccessOrExit(error = mCommissioner->GetRegistrarHostname(regHostname));
            msg = regHostname;
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "regaddr"))
    {
        std::string regAddr;
        VerifyOrExit(!IsSet, msg = "cannot set registrar address");
        SuccessOrExit(error = mCommissioner->GetRegistrarIpv6Addr(regAddr));
        msg = regAddr;
    }
    else
    {
        if (IsSet)
        {
            BbrDataset dataset;
            SuccessOrExit(error = BbrDatasetFromJson(dataset, aExpr[2]));
            SuccessOrExit(error = mCommissioner->SetBbrDataset(dataset));
        }
        else
        {
            ExitNow(msg = aExpr[2]);
        }
    }

exit:
    return {error, msg};
}

Interpreter::Value Interpreter::ProcessReenroll(const Expression &aExpr)
{
    Error       error = Error::kInvalidArgs;
    std::string msg;

    VerifyOrExit(aExpr.size() >= 2, msg = Usage(aExpr));
    SuccessOrExit(error = mCommissioner->Reenroll(aExpr[1]));

exit:
    return {error, msg};
}

Interpreter::Value Interpreter::ProcessDomainReset(const Expression &aExpr)
{
    Error       error = Error::kInvalidArgs;
    std::string msg;

    VerifyOrExit(aExpr.size() >= 2, msg = Usage(aExpr));
    SuccessOrExit(error = mCommissioner->DomainReset(aExpr[1]));

exit:
    return {error, msg};
}

Interpreter::Value Interpreter::ProcessMigrate(const Expression &aExpr)
{
    Error       error = Error::kInvalidArgs;
    std::string msg;

    VerifyOrExit(aExpr.size() >= 3, msg = Usage(aExpr));
    SuccessOrExit(error = mCommissioner->Migrate(aExpr[1], aExpr[2]));

exit:
    return {error, msg};
}

Interpreter::Value Interpreter::ProcessMlr(const Expression &aExpr)
{
    Error       error = Error::kInvalidArgs;
    std::string msg;

    uint32_t timeout;

    VerifyOrExit(aExpr.size() >= 3, msg = Usage(aExpr));
    SuccessOrExit(ParseInteger(timeout, aExpr.back()), msg = aExpr.back());
    SuccessOrExit(error = mCommissioner->RegisterMulticastListener({aExpr.begin() + 1, aExpr.end() - 1},
                                                                   CommissionerApp::Seconds(timeout)));

exit:
    return {error, msg};
}

Interpreter::Value Interpreter::ProcessAnnounce(const Expression &aExpr)
{
    Error       error = Error::kInvalidArgs;
    std::string msg;

    uint32_t channelMask;
    uint8_t  count;
    uint16_t period;

    VerifyOrExit(aExpr.size() >= 5, msg = Usage(aExpr));
    SuccessOrExit(ParseInteger(channelMask, aExpr[1]), msg = aExpr[1]);
    SuccessOrExit(ParseInteger(count, aExpr[2]), msg = aExpr[2]);
    SuccessOrExit(ParseInteger(period, aExpr[3]), msg = aExpr[3]);
    SuccessOrExit(
        error = mCommissioner->AnnounceBegin(channelMask, count, CommissionerApp::MilliSeconds(period), aExpr[4]));

exit:
    return {error, msg};
}

Interpreter::Value Interpreter::ProcessPanId(const Expression &aExpr)
{
    Error       error = Error::kInvalidArgs;
    std::string msg;

    VerifyOrExit(aExpr.size() >= 2, msg = Usage(aExpr));

    if (CaseInsensitiveEqual(aExpr[1], "query"))
    {
        uint32_t channelMask;
        uint16_t panId;
        VerifyOrExit(aExpr.size() >= 5, msg = Usage(aExpr));
        SuccessOrExit(ParseInteger(channelMask, aExpr[2]), msg = aExpr[2]);
        SuccessOrExit(ParseInteger(panId, aExpr[3]), msg = aExpr[3]);
        SuccessOrExit(error = mCommissioner->PanIdQuery(channelMask, panId, aExpr[4]));
    }
    else if (CaseInsensitiveEqual(aExpr[1], "conflict"))
    {
        uint16_t panId;
        bool     conflict;
        VerifyOrExit(aExpr.size() >= 3, msg = Usage(aExpr));
        SuccessOrExit(ParseInteger(panId, aExpr[2]), msg = aExpr[2]);
        conflict = mCommissioner->HasPanIdConflict(panId);
        error    = Error::kNone;
        msg      = std::to_string(conflict);
    }
    else
    {
        msg = aExpr[1];
    }

exit:
    return {error, msg};
}

Interpreter::Value Interpreter::ProcessEnergy(const Expression &aExpr)
{
    Error       error = Error::kInvalidArgs;
    std::string msg;

    VerifyOrExit(aExpr.size() >= 2, msg = Usage(aExpr));

    if (CaseInsensitiveEqual(aExpr[1], "scan"))
    {
        uint32_t channelMask;
        uint8_t  count;
        uint16_t period;
        uint16_t scanDuration;
        VerifyOrExit(aExpr.size() >= 7, msg = Usage(aExpr));
        SuccessOrExit(ParseInteger(channelMask, aExpr[2]), msg = aExpr[2]);
        SuccessOrExit(ParseInteger(count, aExpr[3]), msg = aExpr[3]);
        SuccessOrExit(ParseInteger(period, aExpr[4]), msg = aExpr[4]);
        SuccessOrExit(ParseInteger(scanDuration, aExpr[5]), msg = aExpr[5]);
        SuccessOrExit(error = mCommissioner->EnergyScan(channelMask, count, period, scanDuration, aExpr[6]));
    }
    else if (CaseInsensitiveEqual(aExpr[1], "report"))
    {
        if (aExpr.size() >= 3)
        {
            const EnergyReport *report = nullptr;
            Address             dstAddr;
            SuccessOrExit(dstAddr.Set(aExpr[2]), msg = aExpr[2]);
            report = mCommissioner->GetEnergyReport(dstAddr);
            error  = Error::kNone;
            if (report == nullptr)
            {
                msg = "null";
            }
            else
            {
                msg = EnergyReportToJson(*report);
            }
        }
        else
        {
            error        = Error::kNone;
            auto reports = mCommissioner->GetAllEnergyReports();
            if (reports.empty())
            {
                msg = "null";
            }
            else
            {
                msg = EnergyReportMapToJson(reports);
            }
        }
    }
    else
    {
        msg = aExpr[1];
    }

exit:
    return {error, msg};
}

Interpreter::Value Interpreter::ProcessExit(const Expression &)
{
    mCommissioner->Stop();

    mShouldExit = true;

    return Error::kNone;
}

Interpreter::Value Interpreter::ProcessHelp(const Expression &aExpr)
{
    Error       error = Error::kNone;
    std::string msg;

    if (aExpr.size() == 1)
    {
        for (auto &kv : mEvaluatorMap)
        {
            msg += kv.first + "\n";
        }
        msg += "\ntype 'help <command>' for help of specific command.";
    }
    else
    {
        std::string usage = Usage({aExpr[1]});
        VerifyOrExit(!usage.empty(), {
            error = Error::kInvalidArgs;
            msg   = aExpr[1];
        });
        msg = "usage:\n" + usage;
    }

exit:
    return {error, msg};
}

void Interpreter::BorderAgentHandler(const BorderAgent *aBorderAgent, const std::string *aErrorMessage)
{
    if (aErrorMessage != nullptr)
    {
        Console::Write(*aErrorMessage, Console::Color::kRed);
    }
    else
    {
        VerifyOrDie(aBorderAgent != nullptr);
        Console::Write(ToString(*aBorderAgent), Console::Color::kGreen);
    }
}

const std::string Interpreter::Usage(Expression aExpr)
{
    VerifyOrDie(aExpr.size() >= 1);
    auto usage = mUsageMap.find(aExpr[0]);
    return usage != mUsageMap.end() ? usage->second : "";
}

Error Interpreter::GetJoinerType(JoinerType &aType, const std::string &aStr)
{
    Error error = Error::kNone;
    if (CaseInsensitiveEqual(aStr, "meshcop"))
    {
        aType = JoinerType::kMeshCoP;
    }
    else if (CaseInsensitiveEqual(aStr, "ae"))
    {
        aType = JoinerType::kAE;
    }
    else if (CaseInsensitiveEqual(aStr, "nmkp"))
    {
        aType = JoinerType::kNMKP;
    }
    else
    {
        error = Error::kInvalidArgs;
    }
    return error;
}

Error Interpreter::ParseChannelMask(ChannelMask &aChannelMask, const Expression &aExpr, size_t aIndex)
{
    Error error = Error::kNone;

    VerifyOrExit(aExpr.size() >= aIndex && ((aExpr.size() - aIndex) % 2 == 0), error = Error::kInvalidArgs);

    for (size_t i = aIndex; i < aExpr.size(); i += 2)
    {
        ChannelMaskEntry entry;
        SuccessOrExit(error = ParseInteger(entry.mPage, aExpr[i]), error = Error::kInvalidArgs);
        SuccessOrExit(error = utils::Hex(entry.mMasks, aExpr[i + 1]), error = Error::kInvalidArgs);
        VerifyOrExit(entry.mMasks.size() <= std::numeric_limits<uint8_t>::max(), error = Error::kInvalidArgs);
        aChannelMask.emplace_back(entry);
    }

exit:
    return error;
}

std::string Interpreter::ToString(const Timestamp &aTimestamp)
{
    std::string ret;
    ret += "seconds=" + std::to_string(aTimestamp.mSeconds) + "\n";
    ret += "ticks=" + std::to_string(aTimestamp.mTicks) + "\n";
    ret += "u=" + std::to_string(aTimestamp.mU);
    return ret;
}

std::string Interpreter::ToString(const Channel &aChannel)
{
    std::string ret;
    ret += "page=" + std::to_string(aChannel.mPage) + "\n";
    ret += "channel=" + std::to_string(aChannel.mNumber);
    return ret;
}

std::string Interpreter::ToString(const ChannelMask &aChannelMask)
{
    std::string ret;
    for (const auto &entry : aChannelMask)
    {
        ret += "page=" + std::to_string(entry.mPage) + "\n";
        ret += "masks=" + utils::Hex(entry.mMasks) + "\n";
    }
    if (!ret.empty())
    {
        ret.pop_back();
    }
    return ret;
}

std::string Interpreter::ToString(const SecurityPolicy &aSecurityPolicy)
{
    std::string ret;
    ret += "rotationTime=" + std::to_string(aSecurityPolicy.mRotationTime) + "\n";
    ret += "flags=" + utils::Hex(aSecurityPolicy.mFlags);
    return ret;
}

std::string Interpreter::ToString(const EnergyReport &aReport)
{
    std::string ret;
    ret += ToString(aReport.mChannelMask) + "\n";
    ret += "energyList=" + utils::Hex(aReport.mEnergyList);
    return ret;
}

std::string Interpreter::ToString(const BorderAgent &aBorderAgent)
{
    std::string ret;

#define BUILD_STRING(name, nameStr)                             \
    if (aBorderAgent.mPresentFlags & BorderAgent::k##name##Bit) \
    {                                                           \
        ret += std::string(#name "=") + nameStr + "\n";         \
    };

    BUILD_STRING(Addr, aBorderAgent.mAddr);
    BUILD_STRING(Port, std::to_string(aBorderAgent.mPort));
    BUILD_STRING(ThreadVersion, aBorderAgent.mThreadVersion);
    if (aBorderAgent.mPresentFlags & BorderAgent::kStateBit)
    {
        ret += ToString(aBorderAgent.mState) + "\n";
    }
    BUILD_STRING(NetworkName, aBorderAgent.mNetworkName);
    BUILD_STRING(ExtendedPanId, ToHex(aBorderAgent.mExtendedPanId));
    BUILD_STRING(VendorName, aBorderAgent.mVendorName);
    BUILD_STRING(ModelName, aBorderAgent.mModelName);

    // TODO(wgtdkp): more fields

#undef TO_STRING

    return ret;
}

std::string Interpreter::ToString(const BorderAgent::State &aState)
{
    std::string ret;
    ret += "State.ConnectionMode=" + std::to_string(aState.mConnectionMode) + "(" +
           BaConnModeToString(aState.mConnectionMode) + ")\n";
    ret += "State.ThreadIfStatus=" + std::to_string(aState.mThreadIfStatus) + "(" +
           BaThreadIfStatusToString(aState.mThreadIfStatus) + ")\n";
    ret += "State.Availability=" + std::to_string(aState.mAvailability) + "(" +
           BaAvailabilityToString(aState.mAvailability) + ")\n";
    ret += "State.BbrIsActive=" + std::to_string(aState.mBbrIsActive) + "\n";
    ret += "State.BbrIsPrimary=" + std::to_string(aState.mBbrIsPrimary);
    return ret;
}

std::string Interpreter::BaConnModeToString(uint32_t aConnMode)
{
    switch (aConnMode)
    {
    case 0:
        return "disallowed";
    case 1:
        return "PSKc";
    case 2:
        return "PSKd";
    case 3:
        return "vendor";
    case 4:
        return "X.509";
    default:
        return "reserved";
    }
}

std::string Interpreter::BaThreadIfStatusToString(uint32_t aIfStatus)
{
    switch (aIfStatus)
    {
    case 0:
        return "uninitialized";
    case 1:
        return "inactive";
    case 2:
        return "active";
    default:
        return "reserved";
    }
}

std::string Interpreter::BaAvailabilityToString(uint32_t aAvailability)
{
    switch (aAvailability)
    {
    case 0:
        return "low";
    case 1:
        return "high";
    default:
        return "reserved";
    }
}

} // namespace commissioner

} // namespace ot
