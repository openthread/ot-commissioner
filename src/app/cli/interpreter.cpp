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

#include <string.h>

#include <limits>

#include "app/file_util.hpp"
#include "app/json.hpp"
#include "common/error_macros.hpp"
#include "common/utils.hpp"

#if defined(SuccessOrExit)
#undef SuccessOrExit
#define SuccessOrExit(aError, ...) \
    do                             \
    {                              \
        if ((aError).NoError())    \
        {                          \
            __VA_ARGS__;           \
            goto exit;             \
        }                          \
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

    VerifyOrExit(endPtr != nullptr && endPtr > aStr.c_str(),
                 error = ERROR_INVALID_ARGS("{} is not a valid integer", aStr));

    aInteger = integer;

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
    Error error;

    std::string configJson;
    Config      config;

    SuccessOrExit(error = ReadFile(configJson, aConfigFile));
    SuccessOrExit(error = ConfigFromJson(config, configJson));
    SuccessOrExit(error = CommissionerApp::Create(mCommissioner, config));

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

    VerifyOrExit(!aExpr.empty(), value = ERROR_NONE);

    evaluator = mEvaluatorMap.find(ToLower(aExpr.front()));
    if (evaluator == mEvaluatorMap.end())
    {
        ExitNow(value = ERROR_INVALID_ARGS("invalid commands: {}; type 'help' for all commands", aExpr.front()));
    }

    value = evaluator->second(this, aExpr);

exit:
    return value;
}

void Interpreter::Print(const Value &aValue)
{
    std::string output = aValue.ToString();

    if (!output.empty())
    {
        output += "\n";
    }
    output += aValue.NoError() ? "[done]" : "[failed]";
    auto color = aValue.NoError() ? Console::Color::kGreen : Console::Color::kRed;

    mConsole.Write(output, color);
}

std::string Interpreter::Value::ToString() const
{
    return NoError() ? mData : mError.ToString();
}

bool Interpreter::Value::NoError() const
{
    return mError.NoError();
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
                ASSERT(begin != aLiteral.end());
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
    Error       error;
    uint16_t    port;
    std::string existingCommissionerId;

    VerifyOrExit(aExpr.size() >= 3, error = ERROR_INVALID_ARGS("too few arguments"));
    SuccessOrExit(error = ParseInteger(port, aExpr[2]));
    SuccessOrExit(error = mCommissioner->Start(existingCommissionerId, aExpr[1], port));

exit:
    if (!existingCommissionerId.empty())
    {
        ASSERT(!error.NoError());
        error.SetMessage("there is an existing active commissioner: " + existingCommissionerId);
    }
    return error;
}

Interpreter::Value Interpreter::ProcessStop(const Expression &)
{
    mCommissioner->Stop();
    return ERROR_NONE;
}

Interpreter::Value Interpreter::ProcessActive(const Expression &)
{
    return std::string{mCommissioner->IsActive() ? "true" : "false"};
}

Interpreter::Value Interpreter::ProcessToken(const Expression &aExpr)
{
    Value value;

    VerifyOrExit(aExpr.size() >= 2, value = ERROR_INVALID_ARGS("too few arguments"));

    if (CaseInsensitiveEqual(aExpr[1], "request"))
    {
        uint16_t port;
        VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS("too few arguments"));
        SuccessOrExit(value = ParseInteger(port, aExpr[3]));
        SuccessOrExit(value = mCommissioner->RequestToken(aExpr[2], port));
    }
    else if (CaseInsensitiveEqual(aExpr[1], "print"))
    {
        auto signedToken = mCommissioner->GetToken();
        VerifyOrExit(!signedToken.empty(), value = ERROR_NOT_FOUND("no valid Commissioner Token found"));
        value = utils::Hex(signedToken);
    }
    else if (CaseInsensitiveEqual(aExpr[1], "set"))
    {
        ByteArray signedToken, signerCert;
        VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS("too few arguments"));
        SuccessOrExit(value = ReadHexStringFile(signedToken, aExpr[2]));
        SuccessOrExit(value = ReadPemFile(signerCert, aExpr[3]));

        SuccessOrExit(value = mCommissioner->SetToken(signedToken, signerCert));
    }
    else
    {
        ExitNow(value = ERROR_INVALID_COMMAND("{} is not a valid sub-command", aExpr[1]));
    }

exit:
    return value;
}

Interpreter::Value Interpreter::ProcessNetwork(const Expression &aExpr)
{
    Value value;

    VerifyOrExit(aExpr.size() >= 2, value = ERROR_INVALID_ARGS("too few arguments"));

    if (CaseInsensitiveEqual(aExpr[1], "save"))
    {
        VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS("too few arguments"));
        SuccessOrExit(value = mCommissioner->SaveNetworkData(aExpr[2]));
    }
    else if (CaseInsensitiveEqual(aExpr[1], "sync"))
    {
        SuccessOrExit(value = mCommissioner->SyncNetworkData());
    }
    else
    {
        ExitNow(value = ERROR_INVALID_COMMAND("{} is not a valid sub-command", aExpr[1]));
    }

exit:
    return value;
}

Interpreter::Value Interpreter::ProcessSessionId(const Expression &)
{
    Value    value;
    uint16_t sessionId;

    SuccessOrExit(value = mCommissioner->GetSessionId(sessionId));
    value = std::to_string(sessionId);

exit:
    return value;
}

Interpreter::Value Interpreter::ProcessBorderAgent(const Expression &aExpr)
{
    Value value;

    VerifyOrExit(aExpr.size() >= 2, value = ERROR_INVALID_ARGS("too few arguments"));

    if (CaseInsensitiveEqual(aExpr[1], "discover"))
    {
        uint64_t timeout = 4000;

        if (aExpr.size() >= 3)
        {
            SuccessOrExit(value = ParseInteger(timeout, aExpr[2]));
        }

        SuccessOrExit(value = DiscoverBorderAgent(BorderAgentHandler, static_cast<size_t>(timeout)));
    }
    else if (CaseInsensitiveEqual(aExpr[1], "get"))
    {
        VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS("too few arguments"));

        if (CaseInsensitiveEqual(aExpr[2], "locator"))
        {
            uint16_t locator;
            SuccessOrExit(value = mCommissioner->GetBorderAgentLocator(locator));
            value = ToHex(locator);
        }
        else if (CaseInsensitiveEqual(aExpr[2], "meshlocaladdr"))
        {
            uint16_t    locator;
            std::string meshLocalPrefix;
            std::string meshLocalAddr;
            SuccessOrExit(value = mCommissioner->GetBorderAgentLocator(locator));
            SuccessOrExit(value = mCommissioner->GetMeshLocalPrefix(meshLocalPrefix));
            SuccessOrExit(value = Commissioner::GetMeshLocalAddr(meshLocalAddr, meshLocalPrefix, locator));
            value = meshLocalAddr;
        }
        else
        {
            value = ERROR_INVALID_ARGS("{} is not a valid border agent field", aExpr[2]);
        }
    }
    else
    {
        value = ERROR_INVALID_COMMAND("{} is not a valid sub-command", aExpr[1]);
    }

exit:
    return value;
}

Interpreter::Value Interpreter::ProcessJoiner(const Expression &aExpr)
{
    Value      value;
    JoinerType type;

    VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS("too few arguments"));
    SuccessOrExit(value = GetJoinerType(type, aExpr[2]));

    if (CaseInsensitiveEqual(aExpr[1], "enable"))
    {
        uint64_t    eui64;
        ByteArray   pskd;
        std::string provisioningUrl;

        VerifyOrExit(aExpr.size() >= (type == JoinerType::kMeshCoP ? 5 : 4),
                     value = ERROR_INVALID_ARGS("too few arguments"));
        SuccessOrExit(value = ParseInteger(eui64, aExpr[3]));
        if (type == JoinerType::kMeshCoP)
        {
            pskd = {aExpr[4].begin(), aExpr[4].end()};
            if (aExpr.size() >= 6)
            {
                provisioningUrl = aExpr[5];
            }
        }

        SuccessOrExit(value = mCommissioner->EnableJoiner(type, eui64, pskd, provisioningUrl));
    }
    else if (CaseInsensitiveEqual(aExpr[1], "enableall"))
    {
        ByteArray   pskd;
        std::string provisioningUrl;
        if (type == JoinerType::kMeshCoP)
        {
            VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS("too few arguments"));
            pskd = {aExpr[3].begin(), aExpr[3].end()};
            if (aExpr.size() >= 5)
            {
                provisioningUrl = aExpr[4];
            }
        }

        SuccessOrExit(value = mCommissioner->EnableAllJoiners(type, pskd, provisioningUrl));
    }
    else if (CaseInsensitiveEqual(aExpr[1], "disable"))
    {
        uint64_t eui64;
        VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS("too few arguments"));
        SuccessOrExit(value = ParseInteger(eui64, aExpr[3]));
        SuccessOrExit(value = mCommissioner->DisableJoiner(type, eui64));
    }
    else if (CaseInsensitiveEqual(aExpr[1], "disableall"))
    {
        SuccessOrExit(value = mCommissioner->DisableAllJoiners(type));
    }
    else if (CaseInsensitiveEqual(aExpr[1], "getport"))
    {
        uint16_t port;
        SuccessOrExit(value = mCommissioner->GetJoinerUdpPort(port, type));
        value = std::to_string(port);
    }
    else if (CaseInsensitiveEqual(aExpr[1], "setport"))
    {
        uint16_t port;
        VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS("too few arguments"));
        SuccessOrExit(value = ParseInteger(port, aExpr[3]));
        SuccessOrExit(value = mCommissioner->SetJoinerUdpPort(type, port));
    }
    else
    {
        value = ERROR_INVALID_COMMAND("{} is not a valid sub-command", aExpr[1]);
    }

exit:
    return value;
}

Interpreter::Value Interpreter::ProcessCommDataset(const Expression &aExpr)
{
    Value value;

    VerifyOrExit(aExpr.size() >= 2, value = ERROR_INVALID_ARGS("too few arguments"));

    if (CaseInsensitiveEqual(aExpr[1], "get"))
    {
        CommissionerDataset dataset;
        SuccessOrExit(value = mCommissioner->GetCommissionerDataset(dataset, 0xFFFF));
        value = CommissionerDatasetToJson(dataset);
    }
    else if (CaseInsensitiveEqual(aExpr[1], "set"))
    {
        CommissionerDataset dataset;
        VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS("too few arguments"));
        SuccessOrExit(value = CommissionerDatasetFromJson(dataset, aExpr[2]));
        SuccessOrExit(value = mCommissioner->SetCommissionerDataset(dataset));
    }
    else
    {
        value = ERROR_INVALID_COMMAND("{} is not a valid sub-command", aExpr[1]);
    }

exit:
    return value;
}

Interpreter::Value Interpreter::ProcessOpDataset(const Expression &aExpr)
{
    Value value;
    bool  IsSet;

    VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS("too few arguments"));
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
        ExitNow(value = ERROR_INVALID_COMMAND("{} is not a valid sub-command", aExpr[1]));
    }

    if (CaseInsensitiveEqual(aExpr[2], "activetimestamp"))
    {
        Timestamp activeTimestamp;
        VerifyOrExit(!IsSet, value = ERROR_INVALID_ARGS("cannot set activetimestamp"));
        SuccessOrExit(value = mCommissioner->GetActiveTimestamp(activeTimestamp));
        value = ToString(activeTimestamp);
    }
    else if (CaseInsensitiveEqual(aExpr[2], "channel"))
    {
        Channel channel;
        if (IsSet)
        {
            uint32_t delay;
            VerifyOrExit(aExpr.size() >= 6, value = ERROR_INVALID_ARGS("too few arguments"));
            SuccessOrExit(value = ParseInteger(channel.mPage, aExpr[3]));
            SuccessOrExit(value = ParseInteger(channel.mNumber, aExpr[4]));
            SuccessOrExit(value = ParseInteger(delay, aExpr[5]));
            SuccessOrExit(value = mCommissioner->SetChannel(channel, CommissionerApp::MilliSeconds(delay)));
        }
        else
        {
            SuccessOrExit(value = mCommissioner->GetChannel(channel));
            value = ToString(channel);
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "channelmask"))
    {
        ChannelMask channelMask;
        if (IsSet)
        {
            SuccessOrExit(value = ParseChannelMask(channelMask, aExpr, 3));
            SuccessOrExit(value = mCommissioner->SetChannelMask(channelMask));
        }
        else
        {
            SuccessOrExit(value = mCommissioner->GetChannelMask(channelMask));
            value = ToString(channelMask);
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "xpanid"))
    {
        ByteArray xpanid;
        if (IsSet)
        {
            VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS("too few arguments"));
            SuccessOrExit(value = utils::Hex(xpanid, aExpr[3]));
            SuccessOrExit(value = mCommissioner->SetExtendedPanId(xpanid));
        }
        else
        {
            SuccessOrExit(value = mCommissioner->GetExtendedPanId(xpanid));
            value = utils::Hex(xpanid);
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "meshlocalprefix"))
    {
        std::string prefix;
        if (IsSet)
        {
            uint32_t delay;
            VerifyOrExit(aExpr.size() >= 5, value = ERROR_INVALID_ARGS("too few arguments"));
            SuccessOrExit(value = ParseInteger(delay, aExpr[4]));
            SuccessOrExit(value = mCommissioner->SetMeshLocalPrefix(aExpr[3], CommissionerApp::MilliSeconds(delay)));
        }
        else
        {
            SuccessOrExit(value = mCommissioner->GetMeshLocalPrefix(prefix));
            value = prefix;
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "networkmasterkey"))
    {
        ByteArray masterKey;
        if (IsSet)
        {
            uint32_t delay;
            VerifyOrExit(aExpr.size() >= 5, value = ERROR_INVALID_ARGS("too few arguments"));
            SuccessOrExit(value = utils::Hex(masterKey, aExpr[3]));
            SuccessOrExit(value = ParseInteger(delay, aExpr[4]));
            SuccessOrExit(value = mCommissioner->SetNetworkMasterKey(masterKey, CommissionerApp::MilliSeconds(delay)));
        }
        else
        {
            SuccessOrExit(value = mCommissioner->GetNetworkMasterKey(masterKey));
            value = utils::Hex(masterKey);
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "networkname"))
    {
        std::string networkName;
        if (IsSet)
        {
            VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS("too few arguments"));
            SuccessOrExit(value = mCommissioner->SetNetworkName(aExpr[3]));
        }
        else
        {
            SuccessOrExit(value = mCommissioner->GetNetworkName(networkName));
            value = networkName;
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "panid"))
    {
        uint16_t panid;
        if (IsSet)
        {
            uint32_t delay;
            VerifyOrExit(aExpr.size() >= 5, value = ERROR_INVALID_ARGS("too few arguments"));
            SuccessOrExit(value = ParseInteger(panid, aExpr[3]));
            SuccessOrExit(value = ParseInteger(delay, aExpr[4]));
            SuccessOrExit(value = mCommissioner->SetPanId(panid, CommissionerApp::MilliSeconds(delay)));
        }
        else
        {
            SuccessOrExit(value = mCommissioner->GetPanId(panid));
            value = std::to_string(panid);
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "pskc"))
    {
        ByteArray pskc;
        if (IsSet)
        {
            VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS("too few arguments"));
            SuccessOrExit(value = utils::Hex(pskc, aExpr[3]));
            SuccessOrExit(value = mCommissioner->SetPSKc(pskc));
        }
        else
        {
            SuccessOrExit(value = mCommissioner->GetPSKc(pskc));
            value = utils::Hex(pskc);
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "securitypolicy"))
    {
        SecurityPolicy policy;
        if (IsSet)
        {
            VerifyOrExit(aExpr.size() >= 5, value = ERROR_INVALID_ARGS("too few arguments"));
            SuccessOrExit(value = ParseInteger(policy.mRotationTime, aExpr[3]));
            SuccessOrExit(value = utils::Hex(policy.mFlags, aExpr[4]));
            SuccessOrExit(value = mCommissioner->SetSecurityPolicy(policy));
        }
        else
        {
            SuccessOrExit(value = mCommissioner->GetSecurityPolicy(policy));
            value = ToString(policy);
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "active"))
    {
        ActiveOperationalDataset dataset;
        if (IsSet)
        {
            VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS("too few arguments"));
            SuccessOrExit(value = ActiveDatasetFromJson(dataset, aExpr[3]));
            SuccessOrExit(value = mCommissioner->SetActiveDataset(dataset));
        }
        else
        {
            SuccessOrExit(value = mCommissioner->GetActiveDataset(dataset, 0xFFFF));
            value = ActiveDatasetToJson(dataset);
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "pending"))
    {
        PendingOperationalDataset dataset;
        if (IsSet)
        {
            VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS("too few arguments"));
            SuccessOrExit(value = PendingDatasetFromJson(dataset, aExpr[3]));
            SuccessOrExit(value = mCommissioner->SetPendingDataset(dataset));
        }
        else
        {
            SuccessOrExit(value = mCommissioner->GetPendingDataset(dataset, 0xFFFF));
            value = PendingDatasetToJson(dataset);
        }
    }
    else
    {
        value = ERROR_INVALID_ARGS("{} is not a valid Operational Dataset field", aExpr[2]);
    }

exit:
    return value;
}

Interpreter::Value Interpreter::ProcessBbrDataset(const Expression &aExpr)
{
    Value value;
    bool  IsSet;

    VerifyOrExit(aExpr.size() >= 2, value = ERROR_INVALID_ARGS("too few arguments"));
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
        ExitNow(value = ERROR_INVALID_COMMAND("{} is not a valid sub-command", aExpr[1]));
    }

    if (aExpr.size() == 2 && !IsSet)
    {
        BbrDataset dataset;
        SuccessOrExit(value = mCommissioner->GetBbrDataset(dataset, 0xFFFF));
        ExitNow(value = BbrDatasetToJson(dataset));
    }

    VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS("too few arguments"));

    if (CaseInsensitiveEqual(aExpr[2], "trihostname"))
    {
        std::string triHostname;
        if (IsSet)
        {
            VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS("too few arguments"));
            SuccessOrExit(value = mCommissioner->SetTriHostname(aExpr[3]));
        }
        else
        {
            SuccessOrExit(value = mCommissioner->GetTriHostname(triHostname));
            value = triHostname;
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "reghostname"))
    {
        std::string regHostname;
        if (IsSet)
        {
            VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS("too few arguments"));
            SuccessOrExit(value = mCommissioner->SetRegistrarHostname(aExpr[3]));
        }
        else
        {
            SuccessOrExit(value = mCommissioner->GetRegistrarHostname(regHostname));
            value = regHostname;
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "regaddr"))
    {
        std::string regAddr;
        VerifyOrExit(!IsSet, value = ERROR_INVALID_ARGS("cannot set  read-only Registrar Address"));
        SuccessOrExit(value = mCommissioner->GetRegistrarIpv6Addr(regAddr));
        value = regAddr;
    }
    else
    {
        if (IsSet)
        {
            BbrDataset dataset;
            SuccessOrExit(value = BbrDatasetFromJson(dataset, aExpr[2]));
            SuccessOrExit(value = mCommissioner->SetBbrDataset(dataset));
        }
        else
        {
            value = ERROR_INVALID_ARGS("{} is not a valid BBR Dataset field", aExpr[2]);
        }
    }

exit:
    return value;
}

Interpreter::Value Interpreter::ProcessReenroll(const Expression &aExpr)
{
    Value value;

    VerifyOrExit(aExpr.size() >= 2, value = ERROR_INVALID_ARGS("too few arguments"));
    SuccessOrExit(value = mCommissioner->Reenroll(aExpr[1]));

exit:
    return value;
}

Interpreter::Value Interpreter::ProcessDomainReset(const Expression &aExpr)
{
    Value value;

    VerifyOrExit(aExpr.size() >= 2, value = ERROR_INVALID_ARGS("too few arguments"));
    SuccessOrExit(value = mCommissioner->DomainReset(aExpr[1]));

exit:
    return value;
}

Interpreter::Value Interpreter::ProcessMigrate(const Expression &aExpr)
{
    Value value;

    VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS("too few arguments"));
    SuccessOrExit(value = mCommissioner->Migrate(aExpr[1], aExpr[2]));

exit:
    return value;
}

Interpreter::Value Interpreter::ProcessMlr(const Expression &aExpr)
{
    Value value;

    uint32_t timeout;

    VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS("too few arguments"));
    SuccessOrExit(value = ParseInteger(timeout, aExpr.back()));
    SuccessOrExit(value = mCommissioner->RegisterMulticastListener({aExpr.begin() + 1, aExpr.end() - 1},
                                                                   CommissionerApp::Seconds(timeout)));

exit:
    return value;
}

Interpreter::Value Interpreter::ProcessAnnounce(const Expression &aExpr)
{
    Value value;

    uint32_t channelMask;
    uint8_t  count;
    uint16_t period;

    VerifyOrExit(aExpr.size() >= 5, value = ERROR_INVALID_ARGS("too few arguments"));
    SuccessOrExit(value = ParseInteger(channelMask, aExpr[1]));
    SuccessOrExit(value = ParseInteger(count, aExpr[2]));
    SuccessOrExit(value = ParseInteger(period, aExpr[3]));
    SuccessOrExit(
        value = mCommissioner->AnnounceBegin(channelMask, count, CommissionerApp::MilliSeconds(period), aExpr[4]));

exit:
    return value;
}

Interpreter::Value Interpreter::ProcessPanId(const Expression &aExpr)
{
    Value value;

    VerifyOrExit(aExpr.size() >= 2, value = ERROR_INVALID_ARGS("too few arguments"));

    if (CaseInsensitiveEqual(aExpr[1], "query"))
    {
        uint32_t channelMask;
        uint16_t panId;
        VerifyOrExit(aExpr.size() >= 5, value = ERROR_INVALID_ARGS("too few arguments"));
        SuccessOrExit(value = ParseInteger(channelMask, aExpr[2]));
        SuccessOrExit(value = ParseInteger(panId, aExpr[3]));
        SuccessOrExit(value = mCommissioner->PanIdQuery(channelMask, panId, aExpr[4]));
    }
    else if (CaseInsensitiveEqual(aExpr[1], "conflict"))
    {
        uint16_t panId;
        bool     conflict;
        VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS("too few arguments"));
        SuccessOrExit(value = ParseInteger(panId, aExpr[2]));
        conflict = mCommissioner->HasPanIdConflict(panId);
        value    = std::to_string(conflict);
    }
    else
    {
        value = ERROR_INVALID_COMMAND("{} is not a valid sub-command", aExpr[1]);
    }

exit:
    return value;
}

Interpreter::Value Interpreter::ProcessEnergy(const Expression &aExpr)
{
    Value value;

    VerifyOrExit(aExpr.size() >= 2, value = ERROR_INVALID_ARGS("too few arguments"));

    if (CaseInsensitiveEqual(aExpr[1], "scan"))
    {
        uint32_t channelMask;
        uint8_t  count;
        uint16_t period;
        uint16_t scanDuration;
        VerifyOrExit(aExpr.size() >= 7, value = ERROR_INVALID_ARGS("too few arguments"));
        SuccessOrExit(value = ParseInteger(channelMask, aExpr[2]));
        SuccessOrExit(value = ParseInteger(count, aExpr[3]));
        SuccessOrExit(value = ParseInteger(period, aExpr[4]));
        SuccessOrExit(value = ParseInteger(scanDuration, aExpr[5]));
        SuccessOrExit(value = mCommissioner->EnergyScan(channelMask, count, period, scanDuration, aExpr[6]));
    }
    else if (CaseInsensitiveEqual(aExpr[1], "report"))
    {
        if (aExpr.size() >= 3)
        {
            const EnergyReport *report = nullptr;
            Address             dstAddr;
            SuccessOrExit(value = dstAddr.Set(aExpr[2]));
            report = mCommissioner->GetEnergyReport(dstAddr);
            value  = report == nullptr ? "null" : EnergyReportToJson(*report);
        }
        else
        {
            auto reports = mCommissioner->GetAllEnergyReports();
            value        = reports.empty() ? "null" : EnergyReportMapToJson(reports);
        }
    }
    else
    {
        value = ERROR_INVALID_COMMAND("{} is not a valid sub-command", aExpr[1]);
    }

exit:
    return value;
}

Interpreter::Value Interpreter::ProcessExit(const Expression &)
{
    mCommissioner->Stop();

    mShouldExit = true;

    return ERROR_NONE;
}

Interpreter::Value Interpreter::ProcessHelp(const Expression &aExpr)
{
    Value value;

    if (aExpr.size() == 1)
    {
        std::string data;
        for (auto &kv : mEvaluatorMap)
        {
            data += kv.first + "\n";
        }
        data += "\ntype 'help <command>' for help of specific command.";
    }
    else
    {
        std::string usage = Usage({aExpr[1]});
        VerifyOrExit(!usage.empty(), value = ERROR_INVALID_ARGS("{} is not a valid command", aExpr[1]));
        value = "usage:\n" + usage;
    }

exit:
    return value;
}

void Interpreter::BorderAgentHandler(const BorderAgent *aBorderAgent, const std::string *aErrorMessage)
{
    if (aErrorMessage != nullptr)
    {
        Console::Write(*aErrorMessage, Console::Color::kRed);
    }
    else
    {
        ASSERT(aBorderAgent != nullptr);
        Console::Write(ToString(*aBorderAgent), Console::Color::kGreen);
    }
}

const std::string Interpreter::Usage(Expression aExpr)
{
    ASSERT(aExpr.size() >= 1);
    auto usage = mUsageMap.find(aExpr[0]);
    return usage != mUsageMap.end() ? usage->second : "";
}

Error Interpreter::GetJoinerType(JoinerType &aType, const std::string &aStr)
{
    Error error;

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
        error = ERROR_INVALID_ARGS("{} is not a valid joiner type", aStr);
    }

    return error;
}

Error Interpreter::ParseChannelMask(ChannelMask &aChannelMask, const Expression &aExpr, size_t aIndex)
{
    Error error;

    VerifyOrExit(aExpr.size() >= aIndex && ((aExpr.size() - aIndex) % 2 == 0),
                 error = ERROR_INVALID_ARGS("too few arguments"));

    for (size_t i = aIndex; i < aExpr.size(); i += 2)
    {
        ChannelMaskEntry entry;
        SuccessOrExit(error = ParseInteger(entry.mPage, aExpr[i]));
        SuccessOrExit(error = utils::Hex(entry.mMasks, aExpr[i + 1]));
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
