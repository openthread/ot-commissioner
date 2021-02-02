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
 *   The file implements CLI interpreter.
 */

#include "app/cli/interpreter.hpp"

#include <algorithm>
#include <functional>
#include <memory>
#include <sstream>
#include <string.h>
#include <thread>
#include <unistd.h>

#include <event2/event-config.h>
#include <event2/event.h>
#include <event2/event_struct.h>
#include <event2/util.h>
#include <mdns/mdns.h>

#include "app/cli/job_manager.hpp"
#include "app/json.hpp"
#include "app/ps/registry.hpp"
#include "app/sys_logger.hpp"
#include "common/address.hpp"
#include "common/error_macros.hpp"
#include "common/file_util.hpp"
#include "common/utils.hpp"

#define KEYWORD_NETWORK "--nwk"
#define KEYWORD_DOMAIN "--dom"
#define KEYWORD_EXPORT "--export"
#define KEYWORD_IMPORT "--import"

#define ALIAS_THIS "this"
#define ALIAS_NONE "none"
#define ALIAS_ALL "all"
#define ALIAS_OTHERS "others"

#define SYNTAX_NO_PARAM "keyword {} used with no parameter"
#define SYNTAX_UNKNOWN_KEY "unknown keyword {} encountered"
#define SYNTAX_MULTI_DOMAIN "multiple domain specification not allowed"
#define SYNTAX_MULTI_EXPORT "multiple files not allowed for --export"
#define SYNTAX_MULTI_IMPORT "multiple files not allowed for --import"
#define SYNTAX_NWK_DOM_MUTUAL "--nwk and --dom are mutually exclusive"
#define SYNTAX_EXP_IMP_MUTUAL "--export and --import are mutually exclusive"
#define SYNTAX_NOT_SUPPORTED "{} is not supported by the command"
#define SYNTAX_GROUP_ALIAS "{} must not combine with any other network alias"

#define RUNTIME_EMPTY_NIDS "specified alias(es) resolved to empty list of networks"
#define RUNTIME_AMBIGUOUS "network alias {} is ambiguous"

#define NOT_FOUND_STR "Failed to find registry entity of type: "
#define DOMAIN_STR "domain"
#define NETWORK_STR "network"

#define COLOR_ALIAS_FAILED Console::Color::kYellow

namespace ot {

namespace commissioner {

namespace {

struct FDGuard
{
    FDGuard()
        : mFD(-1)
    {
    }
    explicit FDGuard(int aFD)
        : mFD(aFD)
    {
    }
    // For the time being it is ennough to call close on all file descriptors to be protected: they're from the POSIX
    // environment. If we'd want support for Windows it would be necessary to add support for file descriptor close
    // functions (like, say, closesocket() or even mdns_socket_close()).
    ~FDGuard()
    {
        if (mFD > 0)
        {
            close(mFD);
        }
    }

    int mFD;
};

inline std::string ToString(const mdns_string_t &aString)
{
    return std::string(aString.str, aString.length);
}

inline ByteArray ToByteArray(const mdns_string_t &aString)
{
    return ByteArray{aString.str, aString.str + aString.length};
}

int HandleRecord(const struct sockaddr *from,
                 mdns_entry_type_t      entry,
                 uint16_t               type,
                 uint16_t               rclass,
                 uint32_t               ttl,
                 const void *           data,
                 size_t                 size,
                 size_t                 offset,
                 size_t                 length,
                 void *                 border_agent)
{
    // TODO [MP] Add fromAddr to error strings
    struct sockaddr_storage fromAddrStorage;
    Address                 fromAddr;
    std::string             fromAddrStr;
    std::string             entryType;
    char                    nameBuffer[256];

    BorderAgentOrErrorMsg &borderAgentOrErrorMsg = *reinterpret_cast<BorderAgentOrErrorMsg *>(border_agent);
    BorderAgent &          borderAgent           = borderAgentOrErrorMsg.mBorderAgent;
    Error &                error                 = borderAgentOrErrorMsg.mError;

    (void)rclass;
    (void)ttl;

    *reinterpret_cast<struct sockaddr *>(&fromAddrStorage) = *from;
    if (fromAddr.Set(fromAddrStorage) != ErrorCode::kNone)
    {
        ExitNow(error = ERROR_BAD_FORMAT("invalid source address of mDNS response"));
    }

    fromAddrStr = fromAddr.ToString();

    entryType = (entry == MDNS_ENTRYTYPE_ANSWER) ? "answer"
                                                 : ((entry == MDNS_ENTRYTYPE_AUTHORITY) ? "authority" : "additional");
    if (type == MDNS_RECORDTYPE_PTR)
    {
        mdns_string_t nameStr     = mdns_record_parse_ptr(data, size, offset, length, nameBuffer, sizeof(nameBuffer));
        borderAgent.mServiceName  = std::string(nameStr.str, nameStr.length);
        borderAgent.mPresentFlags = BorderAgent::kServiceNameBit;
        // TODO(wgtdkp): add debug logging.
    }
    else if (type == MDNS_RECORDTYPE_SRV)
    {
        mdns_record_srv_t server = mdns_record_parse_srv(data, size, offset, length, nameBuffer, sizeof(nameBuffer));

        borderAgent.mPort = server.port;
        borderAgent.mPresentFlags |= BorderAgent::kPortBit;
    }
    else if (type == MDNS_RECORDTYPE_A)
    {
        struct sockaddr_storage addrStorage;
        struct sockaddr_in      addr;
        std::string             addrStr;

        mdns_record_parse_a(data, size, offset, length, &addr);

        *reinterpret_cast<struct sockaddr_in *>(&addrStorage) = addr;
        if (fromAddr.Set(addrStorage) != ErrorCode::kNone)
        {
            ExitNow(error = ERROR_BAD_FORMAT("invalid IPv4 address in A record"));
        }

        addrStr = fromAddr.ToString();

        // We prefer AAAA (IPv6) address than A (IPv4) address.
        if (!(borderAgent.mPresentFlags & BorderAgent::kAddrBit))
        {
            borderAgent.mAddr = addrStr;
            borderAgent.mPresentFlags |= BorderAgent::kAddrBit;
        }
    }
    else if (type == MDNS_RECORDTYPE_AAAA)
    {
        struct sockaddr_storage addrStorage;
        struct sockaddr_in6     addr;
        std::string             addrStr;

        mdns_record_parse_aaaa(data, size, offset, length, &addr);

        *reinterpret_cast<struct sockaddr_in6 *>(&addrStorage) = addr;
        if (fromAddr.Set(addrStorage) != ErrorCode::kNone)
        {
            ExitNow(error = ERROR_BAD_FORMAT("invalid IPv6 address in AAAA record"));
        }

        addrStr = fromAddr.ToString();

        borderAgent.mAddr = addrStr;
        borderAgent.mPresentFlags |= BorderAgent::kAddrBit;
    }
    else if (type == MDNS_RECORDTYPE_TXT)
    {
        mdns_record_txt_t txtBuffer[128];
        size_t            parsed;

        parsed =
            mdns_record_parse_txt(data, size, offset, length, txtBuffer, sizeof(txtBuffer) / sizeof(mdns_record_txt_t));

        for (size_t i = 0; i < parsed; ++i)
        {
            auto key         = ToString(txtBuffer[i].key);
            auto value       = ToString(txtBuffer[i].value);
            auto binaryValue = ToByteArray(txtBuffer[i].value);

            if (key == "rv")
            {
                VerifyOrExit(value == "1", error = ERROR_BAD_FORMAT("value of TXT Key 'rv' is {} but not 1", value));
            }
            else if (key == "tv")
            {
                borderAgent.mThreadVersion = value;
                borderAgent.mPresentFlags |= BorderAgent::kThreadVersionBit;
            }
            else if (key == "sb")
            {
                auto &bitmap = binaryValue;
                if (bitmap.size() != 4)
                {
                    ExitNow(error = ERROR_BAD_FORMAT("value of TXT Key 'sb' is invalid: value={}", utils::Hex(bitmap)));
                }

                borderAgent.mState.mConnectionMode = (bitmap[3] & 0x07);
                borderAgent.mState.mThreadIfStatus = (bitmap[3] & 0x18) >> 3;
                borderAgent.mState.mAvailability   = (bitmap[3] & 0x60) >> 5;
                borderAgent.mState.mBbrIsActive    = (bitmap[3] & 0x80) >> 7;
                borderAgent.mState.mBbrIsPrimary   = (bitmap[2] & 0x01);
                borderAgent.mPresentFlags |= BorderAgent::kStateBit;
            }
            else if (key == "nn")
            {
                borderAgent.mNetworkName = value;
                borderAgent.mPresentFlags |= BorderAgent::kNetworkNameBit;
            }
            else if (key == "xp")
            {
                auto &extendPanId = binaryValue;
                if (extendPanId.size() != 8)
                {
                    ExitNow(error = ERROR_BAD_FORMAT("value of TXT Key 'xp' is invalid: value={}",
                                                     utils::Hex(extendPanId)));
                }
                else
                {
                    borderAgent.mExtendedPanId = utils::Decode<uint64_t>(extendPanId);
                    borderAgent.mPresentFlags |= BorderAgent::kExtendedPanIdBit;
                }
            }
            else if (key == "vn")
            {
                borderAgent.mVendorName = value;
                borderAgent.mPresentFlags |= BorderAgent::kVendorNameBit;
            }
            else if (key == "mn")
            {
                borderAgent.mModelName = value;
                borderAgent.mPresentFlags |= BorderAgent::kModelNameBit;
            }
            else if (key == "at")
            {
                auto &activeTimestamp = binaryValue;
                if (activeTimestamp.size() != 8)
                {
                    ExitNow(error = ERROR_BAD_FORMAT("value of TXT Key 'at' is invalid: value={}",
                                                     utils::Hex(activeTimestamp)));
                }
                else
                {
                    borderAgent.mActiveTimestamp = Timestamp::Decode(utils::Decode<uint64_t>(activeTimestamp));
                    borderAgent.mPresentFlags |= BorderAgent::kActiveTimestampBit;
                }
            }
            else if (key == "pt")
            {
                auto &partitionId = binaryValue;
                if (partitionId.size() != 4)
                {
                    ExitNow(error = ERROR_BAD_FORMAT("value of TXT Key 'pt' is invalid: value={}",
                                                     utils::Hex(partitionId)));
                }
                else
                {
                    borderAgent.mPartitionId = utils::Decode<uint32_t>(partitionId);
                    borderAgent.mPresentFlags |= BorderAgent::kPartitionIdBit;
                }
            }
            else if (key == "vd")
            {
                borderAgent.mVendorData = value;
                borderAgent.mPresentFlags |= BorderAgent::kVendorDataBit;
            }
            else if (key == "vo")
            {
                auto &oui = binaryValue;
                if (oui.size() != 3)
                {
                    ExitNow(error = ERROR_BAD_FORMAT("value of TXT Key 'vo' is invalid: value={}", utils::Hex(oui)));
                }
                else
                {
                    borderAgent.mVendorOui = oui;
                    borderAgent.mPresentFlags |= BorderAgent::kVendorOuiBit;
                }
            }
            else if (key == "dn")
            {
                borderAgent.mDomainName = value;
                borderAgent.mPresentFlags |= BorderAgent::kDomainNameBit;
            }
            else if (key == "sq")
            {
                auto &bbrSeqNum = binaryValue;
                if (bbrSeqNum.size() != 1)
                {
                    ExitNow(error =
                                ERROR_BAD_FORMAT("[mDNS] value of TXT Key 'sq' is invalid: {}", utils::Hex(bbrSeqNum)));
                }
                else
                {
                    borderAgent.mBbrSeqNumber = utils::Decode<uint8_t>(bbrSeqNum);
                    borderAgent.mPresentFlags |= BorderAgent::kBbrSeqNumberBit;
                }
            }
            else if (key == "bb")
            {
                auto &bbrPort = binaryValue;
                if (bbrPort.size() != 2)
                {
                    ExitNow(error =
                                ERROR_BAD_FORMAT("[mDNS] value of TXT Key 'bb' is invalid: {}", utils::Hex(bbrPort)));
                }
                else
                {
                    borderAgent.mBbrPort = utils::Decode<uint16_t>(bbrPort);
                    borderAgent.mPresentFlags |= BorderAgent::kBbrPortBit;
                }
            }
            else
            {
                // TODO(wgtdkp): add debug logging.
            }
        }
    }
    else
    {
        // TODO(wgtdkp): add debug logging.
    }

    if (borderAgent.mPresentFlags != 0)
    {
        // Actualize Timestamp
        borderAgent.mUpdateTimestamp.mTime = time(0);
        borderAgent.mPresentFlags |= BorderAgent::kUpdateTimestampBit;
    }
exit:
    return 0;
}

} // namespace

const std::map<std::string, Interpreter::Evaluator> &Interpreter::mEvaluatorMap = *new std::map<std::string, Evaluator>{
    {"config", &Interpreter::ProcessConfig},       {"start", &Interpreter::ProcessStart},
    {"stop", &Interpreter::ProcessStop},           {"active", &Interpreter::ProcessActive},
    {"token", &Interpreter::ProcessToken},         {"br", &Interpreter::ProcessBr},
    {"domain", &Interpreter::ProcessDomain},       {"network", &Interpreter::ProcessNetwork},
    {"sessionid", &Interpreter::ProcessSessionId}, {"borderagent", &Interpreter::ProcessBorderAgent},
    {"joiner", &Interpreter::ProcessJoiner},       {"commdataset", &Interpreter::ProcessCommDataset},
    {"opdataset", &Interpreter::ProcessOpDataset}, {"bbrdataset", &Interpreter::ProcessBbrDataset},
    {"reenroll", &Interpreter::ProcessReenroll},   {"domainreset", &Interpreter::ProcessDomainReset},
    {"migrate", &Interpreter::ProcessMigrate},     {"mlr", &Interpreter::ProcessMlr},
    {"announce", &Interpreter::ProcessAnnounce},   {"panid", &Interpreter::ProcessPanId},
    {"energy", &Interpreter::ProcessEnergy},       {"exit", &Interpreter::ProcessExit},
    {"help", &Interpreter::ProcessHelp},
};

const std::map<std::string, std::string> &Interpreter::mUsageMap = *new std::map<std::string, std::string>{
    {"config", "config get pskc\n"
               "config set pskc <pskc-hex-string>"},
    {"start", "start <border-agent-addr> <border-agent-port>"},
    {"stop", "stop"},
    {"active", "active"},
    {"token", "token request <registrar-addr> <registrar-port>\n"
              "token print\n"
              "token set <signed-token-hex-string-file> <signer-cert-pem-file>"},
    {"br", "br list [--nwk <network-alias-list> | --dom <domain-name>]\n"
           "br add <json-file-path>\n"
           "br delete (<br-record-id> | --nwk <network-alias-list> | --dom <domain-name>)\n"
           "br scan [--export <json-file-path>] [--timeout <ms>]\n"},
    {"domain", "domain list [--dom <domain-name>]"},
    {"network", "network save <network-data-file>\n"
                "network sync\n"
                "network list [--nwk <network-alias-list> | --dom <domain-name>]\n"
                "network select <xpan>|none\n"
                "network identify"},
    {"sessionid", "sessionid"},
    {"borderagent", "borderagent discover [<timeout-in-milliseconds>]\n"
                    "borderagent get locator"},
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

const std::vector<Interpreter::StringArray> &Interpreter::mMultiNetworkSyntax =
    *new std::vector<Interpreter::StringArray>{
        Interpreter::StringArray{"start"},
        Interpreter::StringArray{"stop"},
        Interpreter::StringArray{"active"},
        Interpreter::StringArray{"sessionid"},
        Interpreter::StringArray{"bbrdataset", "get"},
        Interpreter::StringArray{"commdataset", "get"},
        Interpreter::StringArray{"opdataset", "get", "active"},
        Interpreter::StringArray{"opdataset", "get", "pending"},
        Interpreter::StringArray{"opdataset", "set", "securitypolicy"},
        Interpreter::StringArray{"br", "list"},
        Interpreter::StringArray{"br", "delete"},
        Interpreter::StringArray{"domain", "list"},
        Interpreter::StringArray{"network", "list"},
    };

const std::vector<Interpreter::StringArray> &Interpreter::mMultiJobExecution =
    *new std::vector<Interpreter::StringArray>{
        Interpreter::StringArray{"start"},
        Interpreter::StringArray{"stop"},
        Interpreter::StringArray{"active"},
        Interpreter::StringArray{"sessionid"},
        Interpreter::StringArray{"bbrdataset", "get"},
        Interpreter::StringArray{"commdataset", "get"},
        Interpreter::StringArray{"opdataset", "get", "active"},
        Interpreter::StringArray{"opdataset", "get", "pending"},
        Interpreter::StringArray{"opdataset", "set", "securitypolicy"},
    };
const std::vector<Interpreter::StringArray> &Interpreter::mInactiveCommissionerExecution =
    *new std::vector<Interpreter::StringArray>{Interpreter::StringArray{"active"}};
const std::vector<Interpreter::StringArray> &Interpreter::mExportSyntax = *new std::vector<Interpreter::StringArray>{
    Interpreter::StringArray{"bbrdataset", "get"},
    Interpreter::StringArray{"commdataset", "get"},
    Interpreter::StringArray{"opdataset", "get", "active"},
    Interpreter::StringArray{"opdataset", "get", "pending"},
    Interpreter::StringArray{"br", "scan"},
};

const std::vector<Interpreter::StringArray> &Interpreter::mImportSyntax = *new std::vector<Interpreter::StringArray>{
    Interpreter::StringArray{"opdataset", "set", "active"},
    Interpreter::StringArray{"opdataset", "set", "pending"},
};

const std::map<std::string, Interpreter::JobEvaluator> &Interpreter::mJobEvaluatorMap =
    *new std::map<std::string, Interpreter::JobEvaluator>{
        {"start", &Interpreter::ProcessStartJob},
        {"stop", &Interpreter::ProcessStopJob},
        {"active", &Interpreter::ProcessActiveJob},
        {"sessionid", &Interpreter::ProcessSessionIdJob},
        {"commdataset", &Interpreter::ProcessCommDatasetJob},
        {"opdataset", &Interpreter::ProcessOpDatasetJob},
        {"bbrdataset", &Interpreter::ProcessBbrDatasetJob},
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

bool Interpreter::Context::HasGroupAlias()
{
    for (auto alias : mNwkAliases)
    {
        if (alias == ALIAS_ALL || alias == ALIAS_OTHERS)
            return true;
    }
    return false;
}

void Interpreter::Context::Cleanup()
{
    mNwkAliases.clear();
    mDomAliases.clear();
    mExportFiles.clear();
    mImportFiles.clear();
    mCommandKeys.clear();
}

Error Interpreter::Init(const std::string &aConfigFile, const std::string &aRegistryFile)
{
    Error error;

    std::string configJson;
    Config      config;

    if (!aConfigFile.empty())
    {
        SuccessOrExit(error = ReadFile(configJson, aConfigFile));
        SuccessOrExit(error = ConfigFromJson(config, configJson));
    }
    else
    {
        // Default to Non-CCM mode if no configuration file is provided.
        config.mEnableCcm = false;
        config.mPSKc.assign(kMaxPSKcLength, 0xff);
        config.mLogger = SysLogger::Create(LogLevel::kDebug);
    }
    mJobManager.reset(new JobManager(*this));
    SuccessOrExit(error = mJobManager->Init(config));
    mRegistry.reset(new Registry(aRegistryFile));
    VerifyOrExit(mRegistry != nullptr,
                 error = ERROR_OUT_OF_MEMORY("Failed to create registry for file '{}'", aRegistryFile));
    VerifyOrExit(mRegistry->open() == RegistryStatus::REG_SUCCESS, error = ERROR_IO_ERROR("registry failed to open"));
exit:
    return error;
}

void Interpreter::Run()
{
    Expression expr;

    VerifyOrExit(mJobManager != nullptr);

    while (!mShouldExit)
    {
        Print(Eval(Read()));
    }

exit:
    return;
}

void Interpreter::CancelCommand()
{
#if 0 // TODO: need to understand that Stop() on inactive commissioner
    if (mCommissioner->IsActive())
    {
        mCommissioner->CancelRequests();
    }
    else
    {
        mCommissioner->Stop();
    }
#endif
    mJobManager->CancelCommand();
}

Interpreter::Expression Interpreter::Read()
{
    return ParseExpression(Console::Read());
}

Interpreter::Value Interpreter::Eval(const Expression &aExpr)
{
    Expression retExpr;
    bool       supported;
    Value      value;

    // make sure first the command is known
    auto evaluator = mEvaluatorMap.find(ToLower(aExpr.front()));
    if (evaluator == mEvaluatorMap.end())
    {
        ExitNow(value = ERROR_INVALID_COMMAND("'{}' is not a valid command, type 'help' to list all commands",
                                              aExpr.front()));
    }
    // prepare for parsing next command
    mContext.Cleanup();

    SuccessOrExit(value = ReParseMultiNetworkSyntax(aExpr, retExpr));
    // export file specification must be single or omitted
    VerifyOrExit(mContext.mExportFiles.size() < 2, value = ERROR_INVALID_SYNTAX(SYNTAX_MULTI_EXPORT));
    // import file specification must be single or omitted
    VerifyOrExit(mContext.mImportFiles.size() < 2, value = ERROR_INVALID_SYNTAX(SYNTAX_MULTI_IMPORT));

    if (mContext.mExportFiles.size() > 0)
    {
        supported = IsFeatureSupported(mExportSyntax, retExpr);
        VerifyOrExit(supported, value = ERROR_INVALID_SYNTAX(SYNTAX_NOT_SUPPORTED, KEYWORD_EXPORT));
        // export and import must not be specified simultaneously
        VerifyOrExit(mContext.mImportFiles.size() == 0, value = ERROR_INVALID_SYNTAX(SYNTAX_EXP_IMP_MUTUAL));
    }
    else if (mContext.mImportFiles.size() > 0)
    {
        supported = IsFeatureSupported(mImportSyntax, retExpr);
        VerifyOrExit(supported, value = ERROR_INVALID_SYNTAX(SYNTAX_NOT_SUPPORTED, KEYWORD_IMPORT));
        // export and import must not be specified simultaneously
        VerifyOrExit(mContext.mExportFiles.size() == 0, value = ERROR_INVALID_SYNTAX(SYNTAX_EXP_IMP_MUTUAL));
        // let job manager take care of import data
        mJobManager->SetImportFile(mContext.mImportFiles.front());
    }

    if (IsMultiNetworkSyntax(aExpr))
    {
        XpanIdArray nids;

        SuccessOrExit(value = ValidateMultiNetworkSyntax(retExpr, nids));
        if (IsMultiJob(retExpr)) // asynchronous processing required
        {
            SuccessOrExit(value = mJobManager->PrepareJobs(retExpr, nids, mContext.HasGroupAlias()));
            mJobManager->RunJobs();
            value = mJobManager->CollectJobsValue();
        }
        else // synchronous processing possible
        {
            // with multi-network syntax in effect an import semantics
            // is allowed for job-based processing only (see mImportSyntax)
            ASSERT(mContext.mImportFiles.size() == 0);
            value = evaluator->second(this, retExpr);
        }
    }
    else
    {
        // handle single command using selected network
        if (mContext.mImportFiles.size() > 0)
        {
            SuccessOrExit(value = mJobManager->AppendImport(0, retExpr));
        }
        value = evaluator->second(this, retExpr);
    }
    // It is necessary to cleanup import file anyways
    mJobManager->CleanupJobs();
exit:
    // do not cleanup mContext here as the export information is
    // needed for post-processing resultant value
    ASSERT(mJobManager->IsClean());
    return value;
}

bool Interpreter::IsMultiNetworkSyntax(const Expression &aExpr)
{
    for (auto word : aExpr)
    {
        if (word == KEYWORD_NETWORK || word == KEYWORD_DOMAIN)
        {
            return true;
        }
    }
    return false;
}

bool Interpreter::IsMultiJob(const Expression &aExpr)
{
    return IsFeatureSupported(mMultiJobExecution, aExpr);
}

bool Interpreter::IsInactiveCommissionerAllowed(const Expression &aExpr)
{
    return IsFeatureSupported(mInactiveCommissionerExecution, aExpr);
}

Interpreter::Value Interpreter::ValidateMultiNetworkSyntax(const Expression &aExpr, XpanIdArray &aNids)
{
    Error error;
    bool  supported;

    if (mContext.mNwkAliases.size() > 0)
    {
        // verify if respective syntax supported by the current command
        supported = IsFeatureSupported(mMultiNetworkSyntax, aExpr);
        VerifyOrExit(supported, error = ERROR_INVALID_SYNTAX(SYNTAX_NOT_SUPPORTED, KEYWORD_NETWORK));
        // network and domain must not be specified simultaneously
        VerifyOrExit(mContext.mDomAliases.size() == 0, error = ERROR_INVALID_SYNTAX(SYNTAX_NWK_DOM_MUTUAL));
        // validate group alias usage; if used, it must be alone
        for (auto alias : mContext.mNwkAliases)
        {
            if (alias == ALIAS_ALL || alias == ALIAS_OTHERS)
            {
                VerifyOrExit(mContext.mNwkAliases.size() == 1, error = ERROR_INVALID_SYNTAX(SYNTAX_GROUP_ALIAS, alias));
            }
        }
        StringArray    unresolved;
        RegistryStatus status = mRegistry->get_network_xpans_by_aliases(mContext.mNwkAliases, aNids, unresolved);
        for (auto alias : unresolved)
        {
            PrintNetworkMessage(alias, "failed to resolve", COLOR_ALIAS_FAILED);
        }
        VerifyOrExit(status == RegistryStatus::REG_SUCCESS,
                     error = ERROR_IO_ERROR("aliases failed to resolve with status={}", status));
    }
    else if (mContext.mDomAliases.size() > 0)
    {
        // verify if respective syntax supported by the current command
        supported = IsFeatureSupported(mMultiNetworkSyntax, aExpr);
        VerifyOrExit(supported, error = ERROR_INVALID_SYNTAX(SYNTAX_NOT_SUPPORTED, KEYWORD_DOMAIN));
        // network and domain must not be specified simultaneously
        VerifyOrExit(mContext.mNwkAliases.size() == 0, error = ERROR_INVALID_SYNTAX(SYNTAX_NWK_DOM_MUTUAL));
        // domain must be single
        VerifyOrExit(mContext.mDomAliases.size() < 2, error = ERROR_INVALID_SYNTAX(SYNTAX_MULTI_DOMAIN));
        RegistryStatus status = mRegistry->get_network_xpans_in_domain(mContext.mDomAliases.front(), aNids);
        VerifyOrExit(status == RegistryStatus::REG_SUCCESS,
                     error = ERROR_IO_ERROR("domain '{}' failed to resolve with status={}",
                                            mContext.mDomAliases.front(), status));
    }
    else
    {
        // as we came here to evaluate multi-network command, either
        // network or domain list must have entries
        ASSERT(false); // must not hit this, ever
    }

    VerifyOrExit(aNids.size() > 0, error = ERROR_INVALID_ARGS(RUNTIME_EMPTY_NIDS));
exit:
    return error;
}

bool Interpreter::IsFeatureSupported(const std::vector<StringArray> &aArr, const Expression &aExpr) const
{
    for (auto commandCase : aArr)
    {
        if (commandCase.size() != aExpr.size())
            continue;

        bool matching = false;

        for (size_t idx = 0; idx < commandCase.size(); idx++)
        {
            matching = CaseInsensitiveEqual(commandCase[idx], aExpr[idx]);
            if (!matching)
                break;
        }
        if (matching)
            return true;
    }
    return false;
}

Error Interpreter::ReParseMultiNetworkSyntax(const Expression &aExpr, Expression &aRetExpr)
{
    enum
    {
        ST_COMMAND,
        ST_NETWORK,
        ST_DOMAIN,
        ST_EXPORT,
        ST_IMPORT,
        ST_CMD_KEYS
    };
    StringArray *arrays[] = {[ST_COMMAND]  = &aRetExpr,
                             [ST_NETWORK]  = &mContext.mNwkAliases,
                             [ST_DOMAIN]   = &mContext.mDomAliases,
                             [ST_EXPORT]   = &mContext.mExportFiles,
                             [ST_IMPORT]   = &mContext.mImportFiles,
                             [ST_CMD_KEYS] = &mContext.mCommandKeys};
    Error        error;

    uint8_t     state = ST_COMMAND;
    bool        inKey = false;
    std::string lastKey;

    for (auto word : aExpr)
    {
        std::string prefix = word.substr(0, 2);

        /*
         * Note: the syntax allows multiple specification of the same key
         *
         * Example: the cases below are parsed equivalently
         *  --nwk nid1 nid2 nid3 --export path
         *  --nwk nid1 --nwk nid2 nid3 --export path
         *  --nwk nid1 --export path --nwk nid2 --nwk nid3
         */

        if (prefix == "--")
        {
            VerifyOrExit(!inKey, error = ERROR_INVALID_SYNTAX(SYNTAX_NO_PARAM, lastKey));
            lastKey = word;
            inKey   = true;
            if (word == KEYWORD_NETWORK)
                state = ST_NETWORK;
            else if (word == KEYWORD_DOMAIN)
                state = ST_DOMAIN;
            else if (word == KEYWORD_EXPORT)
                state = ST_EXPORT;
            else if (word == KEYWORD_IMPORT)
                state = ST_IMPORT;
            else
            {
                // Some commands have command line parameters that are to be passed without any change including the
                // key.
                inKey = false;
                state = ST_CMD_KEYS;
                arrays[state]->push_back(word);
            }
        }
        else
        {
            inKey = false;
            // Note: avoid accidental conversion of the PSKd to lower case (see TS 8.2, Joining Device Credential
            // in the table 8.2
            arrays[state]->push_back(word);
        }
    }
    if (inKey) // test if we exit for() with trailing keyword
    {
        error = ERROR_INVALID_SYNTAX(SYNTAX_NO_PARAM, lastKey);
    }
exit:
    return error;
}

void Interpreter::Print(const Value &aValue)
{
    Error       error  = {ErrorCode::kUnknown, ""};
    std::string output = aValue.ToString();

    if (aValue.HasNoError() && mContext.mExportFiles.size() > 0)
    {
        for (auto path : mContext.mExportFiles)
        {
            std::string filePath = path;

            // Make sure folder path exists, if it's not creatable, print error and output to console instead
            error = RestoreFilePath(path);
            if (error != ERROR_NONE)
            {
                mConsole.Write(error.GetMessage(), Console::Color::kRed);
                break;
            }
#if 0
            error = PathExists(filePath);
            while (error != ERROR_NONE)
            {
                // TODO: suggest new file name by incrementing suffix
                //       file-name[-suffix].ext
                error = PathExists(filePath);
            }
#endif
            error = WriteFile(aValue.ToString(), filePath);
            if (error != ERROR_NONE)
            {
                std::string out = fmt::format(FMT_STRING("failed to write to '{}'\n"), filePath);
                mConsole.Write(out, Console::Color::kRed);
                break;
            }
        }
    }
    if (error == ERROR_NONE)
    {
        // value is written to file, no console output expected other than
        // [done]/[failed]
        output.clear();
    }
    if (!output.empty())
    {
        output += "\n";
    }
    output += aValue.HasNoError() ? "[done]" : "[failed]";
    auto color = aValue.HasNoError() ? Console::Color::kGreen : Console::Color::kRed;

    mConsole.Write(output, color);
}

void Interpreter::PrintNetworkMessage(uint64_t aNid, std::string aMessage, Console::Color aColor)
{
    std::string nidHex = xpan_id(aNid);
    PrintNetworkMessage(nidHex, aMessage, aColor);
}

void Interpreter::PrintNetworkMessage(std::string alias, std::string aMessage, Console::Color aColor)
{
    mConsole.Write(alias.append(": "));
    mConsole.Write(aMessage, aColor);
    mConsole.Write("\n");
}

std::string Interpreter::Value::ToString() const
{
    return HasNoError() ? mData : mError.ToString();
}

bool Interpreter::Value::HasNoError() const
{
    return mError == ErrorCode::kNone;
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

Interpreter::Value Interpreter::ProcessConfig(const Expression &aExpr)
{
    Value value;

    VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS("two few arguments"));
    VerifyOrExit(aExpr[2] == "pskc", value = ERROR_INVALID_ARGS("{} is not a valid property", aExpr[2]));
    if (aExpr[1] == "get")
    {
        value = mJobManager->GetDefaultConfigPSKc();
    }
    else if (aExpr[1] == "set")
    {
        ByteArray pskc;

        VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS("two few arguments"));
        SuccessOrExit(value = utils::Hex(pskc, aExpr[3]));
        SuccessOrExit(value = mJobManager->UpdateDefaultConfigPSKc(pskc));
    }
    else
    {
        ExitNow(value = ERROR_INVALID_COMMAND("{} is not a valid sub-command", aExpr[1]));
    }

exit:
    return value;
}

Interpreter::Value Interpreter::ProcessStart(const Expression &aExpr)
{
    Value              value;
    CommissionerAppPtr commissioner = nullptr;
    Expression         expr         = aExpr;
    BorderRouter       br;

    switch (aExpr.size())
    {
    case 1:
    {
        // starting currently selected network
        uint64_t       nid;
        RegistryStatus status = mRegistry->get_current_network_xpan(nid);
        VerifyOrExit(status == RegistryStatus::REG_SUCCESS, value = ERROR_IO_ERROR("getting selected network failed"));
        SuccessOrExit(value = mJobManager->GetSelectedCommissioner(commissioner));
        SuccessOrExit(value = mJobManager->MakeBorderRouterChoice(nid, br));
        expr.push_back(br.agent.mAddr);
        expr.push_back(std::to_string(br.agent.mPort));
        break;
    }
    case 2:
    {
        // starting newtork by br raw id (experimental, for dev tests only)
        persistent_storage::border_router_id rawid;

        SuccessOrExit(value = ParseInteger(rawid.id, expr[1]));
        VerifyOrExit(mRegistry->get_border_router(rawid, br) == RegistryStatus::REG_SUCCESS,
                     value = ERROR_NOT_FOUND("br[{}] not found", rawid.id));
        VerifyOrExit(mRegistry->set_current_network(br) == RegistryStatus::REG_SUCCESS,
                     value = ERROR_NOT_FOUND("network selection failed for nwk[{}]", br.nwk_id.id));
        SuccessOrExit(value = mJobManager->GetSelectedCommissioner(commissioner));
        expr.pop_back();
        expr.push_back(br.agent.mAddr);
        expr.push_back(std::to_string(br.agent.mPort));
        break;
    }
    case 3:
    {
        // starting network with explicit br_addr and br_port
        VerifyOrExit(mRegistry->forget_current_network() == RegistryStatus::REG_SUCCESS,
                     value = ERROR_IO_ERROR("failed to drop network selection"));
        SuccessOrExit(value = mJobManager->GetSelectedCommissioner(commissioner));
        break;
    }
    default:
        ExitNow(value = ERROR_INVALID_COMMAND("not a valid command syntax"));
    }

    value = ProcessStartJob(commissioner, expr);
exit:
    return value;
}

Interpreter::Value Interpreter::ProcessStartJob(CommissionerAppPtr &aCommissioner, const Expression &aExpr)
{
    Error       error;
    uint16_t    port;
    std::string existingCommissionerId;

    VerifyOrExit(aExpr.size() >= 3, error = ERROR_INVALID_ARGS("too few arguments"));
    SuccessOrExit(error = ParseInteger(port, aExpr[2]));
    SuccessOrExit(error = aCommissioner->Start(existingCommissionerId, aExpr[1], port));

exit:
    if (!existingCommissionerId.empty())
    {
        ASSERT(error != ErrorCode::kNone);
        error = Error{error.GetCode(), "there is an existing active commissioner: " + existingCommissionerId};
    }
    return error;
}

Interpreter::Value Interpreter::ProcessStop(const Expression &aExpr)
{
    Value              value;
    CommissionerAppPtr commissioner = nullptr;

    SuccessOrExit(value = mJobManager->GetSelectedCommissioner(commissioner));
    value = ProcessStopJob(commissioner, aExpr);
exit:
    return value;
}

Interpreter::Value Interpreter::ProcessStopJob(CommissionerAppPtr &aCommissioner, const Expression &)
{
    aCommissioner->Stop();
    return ERROR_NONE;
}

Interpreter::Value Interpreter::ProcessActive(const Expression &aExpr)
{
    Value              value;
    CommissionerAppPtr commissioner = nullptr;

    SuccessOrExit(value = mJobManager->GetSelectedCommissioner(commissioner));
    value = ProcessActiveJob(commissioner, aExpr);
exit:
    return value;
}

Interpreter::Value Interpreter::ProcessActiveJob(CommissionerAppPtr &aCommissioner, const Expression &)
{
    return std::string{aCommissioner->IsActive() ? "true" : "false"};
}

Interpreter::Value Interpreter::ProcessToken(const Expression &aExpr)
{
    Value              value;
    CommissionerAppPtr commissioner = nullptr;

    SuccessOrExit(value = mJobManager->GetSelectedCommissioner(commissioner));
    VerifyOrExit(aExpr.size() >= 2, value = ERROR_INVALID_ARGS("too few arguments"));

    if (CaseInsensitiveEqual(aExpr[1], "request"))
    {
        uint16_t port;
        VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS("too few arguments"));
        SuccessOrExit(value = ParseInteger(port, aExpr[3]));
        SuccessOrExit(value = commissioner->RequestToken(aExpr[2], port));
    }
    else if (CaseInsensitiveEqual(aExpr[1], "print"))
    {
        auto signedToken = commissioner->GetToken();
        VerifyOrExit(!signedToken.empty(), value = ERROR_NOT_FOUND("no valid Commissioner Token found"));
        value = utils::Hex(signedToken);
    }
    else if (CaseInsensitiveEqual(aExpr[1], "set"))
    {
        ByteArray signedToken;
        VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS("too few arguments"));
        SuccessOrExit(value = ReadHexStringFile(signedToken, aExpr[2]));

        SuccessOrExit(value = commissioner->SetToken(signedToken));
    }
    else
    {
        ExitNow(value = ERROR_INVALID_COMMAND("{} is not a valid sub-command", aExpr[1]));
    }

exit:
    return value;
}

Interpreter::Value Interpreter::ProcessBr(const Expression &aExpr)
{
    using namespace ot::commissioner::persistent_storage;

    Value value;

    VerifyOrExit(aExpr.size() >= 2, value = ERROR_INVALID_ARGS("too few arguments"));
    if (CaseInsensitiveEqual(aExpr[1], "list"))
    {
        nlohmann::json             json;
        std::vector<border_router> routers;
        std::vector<network>       networks;
        RegistryStatus             status;

        VerifyOrExit(aExpr.size() == 2, value = ERROR_INVALID_ARGS("too many arguments"));
        if (mContext.mDomAliases.size() > 0)
        {
            for (auto dom : mContext.mDomAliases)
            {
                std::vector<network> domNetworks;
                status = mRegistry->get_networks_in_domain(dom, domNetworks);
                if (status != RegistryStatus::REG_SUCCESS)
                {
                    PrintNetworkMessage(dom, "unrecognized domain", COLOR_ALIAS_FAILED);
                }
                else
                {
                    networks.insert(networks.end(), domNetworks.begin(), domNetworks.end());
                }
            }
        }
        else if (mContext.mNwkAliases.size() > 0)
        {
            StringArray unresolved;
            status = mRegistry->get_networks_by_aliases(mContext.mNwkAliases, networks, unresolved);
            for (auto alias : unresolved)
            {
                PrintNetworkMessage(alias, "network alias unknown", COLOR_ALIAS_FAILED);
            }
            VerifyOrExit(status == RegistryStatus::REG_SUCCESS, value = ERROR_IO_ERROR("lookup failed"));
        }
        else
        {
            status = mRegistry->get_all_border_routers(routers);
            VerifyOrExit(status == RegistryStatus::REG_SUCCESS, value = ERROR_IO_ERROR("lookup failed"));
        }
        if (networks.size() > 0)
        {
            for (auto nwk : networks)
            {
                std::vector<border_router> nwkRouters;
                status = mRegistry->get_border_routers_in_network(nwk.xpan, nwkRouters);
                if (status == RegistryStatus::REG_SUCCESS)
                {
                    routers.insert(routers.end(), nwkRouters.begin(), nwkRouters.end());
                }
                else
                {
                    PrintNetworkMessage(nwk.xpan.value, "lookup failed", COLOR_ALIAS_FAILED);
                }
            }
        }
        if (routers.size() > 0)
        {
            json  = routers;
            value = json.dump(JSON_INDENT_DEFAULT);
        }
        else
        {
            value = ERROR_NOT_FOUND("no border routers found");
        }
    }
    else if (CaseInsensitiveEqual(aExpr[1], "add"))
    {
        Error                    error;
        std::string              jsonStr;
        nlohmann::json           json;
        std::vector<BorderAgent> agents;

        VerifyOrExit((error = JsonFromFile(jsonStr, aExpr[2])) == ERROR_NONE, value = Value(error));
        // TODO [MP]: handle possible failures - will result in exception
        json = nlohmann::json::parse(jsonStr);

        // Can be either single BorderAgent or an array of them.
        if (!json.is_array())
        {
            // Parse single BorderAgent
            BorderAgent ba;
            BorderAgentFromJson(json, ba);
            agents.push_back(ba);
        }
        else
        {
            for (auto iter = json.begin(); iter != json.end(); ++iter)
            {
                BorderAgent ba;
                BorderAgentFromJson(*iter, ba);
                agents.push_back(ba);
            }
        }
        // Sanity check of BorderAgent records
        for (auto iter = agents.begin(); iter != agents.end(); ++iter)
        {
            // Check mandatory fields present
            // By Thread 1.1.1 spec table 8-4
            // TODO [MP] by #18: Replace the current with commented.
            //   Commented is the behavior by the spec. The code in effect is the one by implementation of the OT BR
            //   Border Agent component.
            // constexpr const uint32_t kMandatoryFieldsBits =
            //     (BorderAgent::kAddrBit | BorderAgent::kPortBit | BorderAgent::kThreadVersionBit |
            //      BorderAgent::kStateBit | BorderAgent::kNetworkNameBit | BorderAgent::kExtendedPanIdBit);
            constexpr const uint32_t kMandatoryFieldsBits =
                (BorderAgent::kAddrBit | BorderAgent::kPortBit | BorderAgent::kThreadVersionBit |
                 BorderAgent::kNetworkNameBit | BorderAgent::kExtendedPanIdBit);
            if ((iter->mPresentFlags & kMandatoryFieldsBits) != kMandatoryFieldsBits)
            {
                value = Value(ERROR_REJECTED("Missing mandatory border agent fields"));
                goto exit;
            }

            // Check local network information sanity
            constexpr const uint32_t kNetworkInfoBits =
                (BorderAgent::kExtendedPanIdBit | BorderAgent::kNetworkNameBit | BorderAgent::kDomainNameBit);
            if (((iter->mPresentFlags & kNetworkInfoBits) != 0) &&
                (((iter->mPresentFlags & BorderAgent::kExtendedPanIdBit) == 0) || (iter->mExtendedPanId == 0)))
            {
                value = ERROR_REJECTED("XPAN required but not provided for border agent host:port {}:{}", iter->mAddr,
                                       iter->mPort);
                goto exit;
            }

            // Sanity check across inbound data
            if (iter->mPresentFlags & BorderAgent::kExtendedPanIdBit)
            {
                struct
                {
                    bool addr : 1;
                    bool name : 1;
                    bool domain : 1;
                } flags = {false, false, false};

                // Network: (XPAN, PAN) combination must be same everywhere
                std::vector<BorderAgent>::iterator found =
                    std::find_if(agents.begin(), iter, [=, &flags](BorderAgent a) {
                        flags.addr = (iter->mAddr == a.mAddr && iter->mPort == a.mPort);
                        if (flags.addr)
                        {
                            return true;
                        }

#define CHECK_PRESENT_FLAG(record, flag) ((record).mPresentFlags & BorderAgent::k##flag##Bit)
                        if (CHECK_PRESENT_FLAG(a, ExtendedPanId) == 0)
                        {
                            return false;
                        }
                        flags.name = (CHECK_PRESENT_FLAG(a, NetworkName) != CHECK_PRESENT_FLAG(*iter, NetworkName)) ||
                                     ((CHECK_PRESENT_FLAG(*iter, NetworkName) != 0) &&
                                      iter->mExtendedPanId == a.mExtendedPanId && iter->mNetworkName != a.mNetworkName);
                        flags.domain = (CHECK_PRESENT_FLAG(a, DomainName) != CHECK_PRESENT_FLAG(*iter, DomainName)) ||
                                       ((CHECK_PRESENT_FLAG(*iter, DomainName) != 0) &&
                                        iter->mExtendedPanId == a.mExtendedPanId && iter->mDomainName != a.mDomainName);
                        return flags.name || flags.domain;
#undef CHECK_PRESENT_FLAG
                    });
                if (found != iter)
                {
                    if (flags.addr)
                    {
                        value = Value(
                            ERROR_REJECTED("Address {} and port {} combination is not unique for inbound border agents",
                                           iter->mAddr, iter->mPort));
                    }
                    else if (flags.name)
                    {
                        value =
                            Value(ERROR_REJECTED("Two inbound border agents have same XPAN '{}', but different network "
                                                 "names ('{}' and '{}')",
                                                 iter->mExtendedPanId, iter->mNetworkName, found->mNetworkName));
                    }
                    else if (flags.domain)
                    {
                        value = ERROR_REJECTED(
                            "Two inbound border agents have same XPAN '{}', but different domain names ('{}' and '{}')",
                            iter->mExtendedPanId, iter->mDomainName, found->mDomainName);
                    }
                    goto exit;
                }
            }
        }

        for (auto agent : agents)
        {
            auto status = mRegistry->add(agent);
            if (status != RegistryStatus::REG_SUCCESS)
            {
                value = ERROR_IO_ERROR("Registry insert failure with border agent address {} and port {}", agent.mAddr,
                                       agent.mPort);
                goto exit;
            }
        }
    }
    else if (CaseInsensitiveEqual(aExpr[1], "delete"))
    {
        if (aExpr.size() > 3 || (aExpr.size() == 3 && !(mContext.mNwkAliases.empty() && mContext.mDomAliases.empty())))
        {
            value = Error(ErrorCode::kInvalidArgs, "Too many arguments for `br delete` command`");
            goto exit;
        }
        else if (aExpr.size() == 3)
        {
            // Attempt to remove border agent by border_router_id
            border_router_id brId;
            try
            {
                brId = border_router_id(std::stoi(aExpr[2]));
            } catch (...)
            {
                value = ERROR_INVALID_ARGS("Failed to evaluate border router ID '{}'", aExpr[2]);
                goto exit;
            }
            VerifyOrExit(mRegistry->delete_border_router_by_id(brId) == registry_status::REG_SUCCESS,
                         value = ERROR_IO_ERROR("Failed to delete border router ID {}", aExpr[2]));
        }
        else if (mContext.mNwkAliases.size() > 0)
        {
            StringArray unresolved;
            // Remove all border agents in the networks
            VerifyOrExit(mRegistry->delete_border_routers_in_networks(mContext.mNwkAliases, unresolved) ==
                             registry_status::REG_SUCCESS,
                         value = Error(ErrorCode ::kIOError, "Failed to delete border routers"));
            // Report unresolved aliases
            for (auto &&alias : unresolved)
            {
                PrintNetworkMessage(alias, "failed to resolve", COLOR_ALIAS_FAILED);
            }
        }
        else if (mContext.mDomAliases.size() > 0)
        {
            StringArray undeleted;
            // Remove all border agents in the domain
            VerifyOrExit(
                mRegistry->delete_border_routers_in_domain(mContext.mDomAliases[0]) == registry_status::REG_SUCCESS,
                value = ERROR_IO_ERROR("Failed to delete border routers in the domain '{}'", mContext.mDomAliases[0]));
        }
    }
    else if (CaseInsensitiveEqual(aExpr[1], "scan"))
    {
        constexpr const mdns_record_type_t kMdnsQueryType  = MDNS_RECORDTYPE_PTR;
        constexpr const uint32_t           kMdnsBufferSize = 16 * 1024;
        const std::string                  kServiceName    = "_meshcop._udp.local";

        uint32_t                                  scanTimeout = 10000;
        int                                       mdnsSocket  = -1;
        FDGuard                                   fdgMdnsSocket;
        std::thread                               selectThread;
        event_base *                              base;
        timeval                                   tvTimeout;
        std::unique_ptr<event, void (*)(event *)> mdnsEvent(nullptr, event_free);
        std::vector<BorderAgentOrErrorMsg>        borderAgents;
        nlohmann::json                            baJson;
        char                                      mdnsSendBuffer[kMdnsBufferSize];

        if (mContext.mCommandKeys.size() == 2 && mContext.mCommandKeys[0] == "--timeout")
        {
            try
            {
                scanTimeout = stol(mContext.mCommandKeys[1]);
            } catch (...)
            {
                ExitNow(value = ERROR_INVALID_ARGS("Imparsable timeout value '{}'", aExpr[3]));
            }
        }

        // Open IPv4 mDNS socket
        mdnsSocket = mdns_socket_open_ipv4();
        VerifyOrExit(mdnsSocket >= 0, value = ERROR_IO_ERROR("failed to open mDNS IPv4 socket"));
        fdgMdnsSocket.mFD = mdnsSocket;

        //  Initialize event library
        base = event_base_new();

        // Instantly register timeout event to stop the event loop
        evutil_timerclear(&tvTimeout);
        tvTimeout.tv_sec  = scanTimeout / 1000;
        tvTimeout.tv_usec = (scanTimeout % 1000) * 1000;
        event_base_loopexit(base, &tvTimeout);

        // Register event for inbound events on mdnsSocket
        // The mdnsSocket is already non-blocking (as we need for the event library)
        auto mdnsReaderParser = [](evutil_socket_t aSocket, short aWhat, void *aArg) {
            (void)aWhat;

            BorderAgentOrErrorMsg               agent;
            char                                buf[1024 * 16];
            std::vector<BorderAgentOrErrorMsg> &agents = *static_cast<std::vector<BorderAgentOrErrorMsg> *>(aArg);

            mdns_query_recv(aSocket, buf, sizeof(buf), HandleRecord, &agent, 1);
            agents.push_back(agent);

            return;
        };
        mdnsEvent = std::unique_ptr<event, void (*)(event *)>(
            event_new(base, mdnsSocket, EV_READ | EV_PERSIST, mdnsReaderParser, &borderAgents), event_free);
        event_add(mdnsEvent.get(), nullptr);

        // Start event loop thread
        selectThread = std::thread([](event_base *aBase) { event_base_dispatch(aBase); }, base);

        // Send mDNS query
        if (mdns_query_send(mdnsSocket, kMdnsQueryType, kServiceName.c_str(), kServiceName.length(), mdnsSendBuffer,
                            sizeof(mdnsSendBuffer)) != 0)
        {
            // Stop event thread
            event_base_loopbreak(base);
            selectThread.join();
            ExitNow(value = ERROR_IO_ERROR("Sending mDNS query failed"));
        }
        // Join the waiting (event loop) thread
        selectThread.join();

        // Serialize BorderAgents to JSON into value
        for (auto agentOrError : borderAgents)
        {
            if (agentOrError.mError != ErrorCode::kNone)
            {
                mConsole.Write(agentOrError.mError.GetMessage(), Console::Color::kRed);
            }
            else
            {
                nlohmann::json ba;
                BorderAgentToJson(ba, agentOrError.mBorderAgent);
                baJson.push_back(ba);
            }
        }
        value = baJson.dump(JSON_INDENT_DEFAULT);
    }
    else
    {
        ExitNow(value = ERROR_INVALID_COMMAND("{} is not a valid sub-command", aExpr[1]));
    }

exit:
    return value;
} // namespace commissioner

Interpreter::Value Interpreter::ProcessDomain(const Expression &aExpr)
{
    using namespace ot::commissioner::persistent_storage;

    Value value;

    VerifyOrExit(aExpr.size() > 1, value = ERROR_INVALID_ARGS("too few arguments"));
    VerifyOrExit(aExpr.size() == 2, value = ERROR_INVALID_ARGS("too many arguments"));
    if (CaseInsensitiveEqual(aExpr[1], "list"))
    {
        RegistryStatus      status;
        nlohmann::json      json;
        std::vector<domain> domains;

        for (auto alias : mContext.mDomAliases)
        {
            if (alias == ALIAS_ALL || alias == ALIAS_OTHERS)
            {
                ExitNow(value = ERROR_INVALID_SYNTAX("alias '{}' not supported by the command", alias));
            }
        }
        if (mContext.mDomAliases.size() != 0)
        {
            StringArray unresolved;
            status = mRegistry->get_domains_by_aliases(mContext.mDomAliases, domains, unresolved);
            for (auto alias : unresolved)
            {
                PrintNetworkMessage(alias, "failed to resolve", COLOR_ALIAS_FAILED);
            }
            VerifyOrExit(status == RegistryStatus::REG_SUCCESS, value = ERROR_IO_ERROR("lookup failed"));
        }
        else
        {
            status = mRegistry->get_all_domains(domains);
            VerifyOrExit(status == RegistryStatus::REG_SUCCESS, value = ERROR_IO_ERROR("lookup failed"));
        }
        json  = domains;
        value = json.dump(JSON_INDENT_DEFAULT);
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
    using namespace ot::commissioner::persistent_storage;

    Value              value;
    CommissionerAppPtr commissioner = nullptr;

    VerifyOrExit(aExpr.size() >= 2, value = ERROR_INVALID_ARGS("too few arguments"));

    if (CaseInsensitiveEqual(aExpr[1], "save"))
    {
        VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS("too few arguments"));
        SuccessOrExit(value = mJobManager->GetSelectedCommissioner(commissioner));
        SuccessOrExit(value = commissioner->SaveNetworkData(aExpr[2]));
    }
    else if (CaseInsensitiveEqual(aExpr[1], "sync"))
    {
        SuccessOrExit(value = mJobManager->GetSelectedCommissioner(commissioner));
        SuccessOrExit(value = commissioner->SyncNetworkData());
    }
    else if (CaseInsensitiveEqual(aExpr[1], "list"))
    {
        SuccessOrExit(value = ProcessNetworkList(aExpr));
    }
    else if (CaseInsensitiveEqual(aExpr[1], "select"))
    {
        Interpreter::Value value;
        ::nlohmann::json   json;

        VerifyOrExit(aExpr.size() == 3, value = ERROR_INVALID_SYNTAX("too many arguments"));
        if (CaseInsensitiveEqual(aExpr[2], "none"))
        {
            registry_status status = mRegistry->forget_current_network();
            VerifyOrExit(status == registry_status::REG_SUCCESS, value = ERROR_IO_ERROR("network select failed"));
        }
        else
        {
            StringArray aliases = {aExpr[2]};
            XpanIdArray xpans;
            StringArray unresolved;
            VerifyOrExit(mRegistry->get_network_xpans_by_aliases(aliases, xpans, unresolved) == REG_SUCCESS,
                         value = ERROR_IO_ERROR("Failed to resolve extended PAN Id for network {}", aExpr[2]));
            VerifyOrExit(xpans.size() == 1, value = ERROR_IO_ERROR("Detected {} networks instead of 1 for alias '{}'",
                                                                   xpans.size(), aExpr[2]));
            VerifyOrExit(mRegistry->set_current_network(xpans[0]) == REG_SUCCESS,
                         value = ERROR_IO_ERROR("network set failed"));
            value = std::string("done");
        }
    }
    else if (CaseInsensitiveEqual(aExpr[1], "identify"))
    {
        ::nlohmann::json json;
        network          nwk;
        std::string      nwkData;

        VerifyOrExit(aExpr.size() == 2, value = ERROR_INVALID_SYNTAX("too many arguments"));
        VerifyOrExit(mRegistry->get_current_network(nwk) == registry_status::REG_SUCCESS,
                     value = ERROR_NOT_FOUND(NOT_FOUND_STR NETWORK_STR));
        if (nwk.id.id == EMPTY_ID)
        {
            value = std::string("none");
        }
        else
        {
            if (nwk.dom_id.id != EMPTY_ID)
            {
                VerifyOrExit(mRegistry->get_domain_name_by_xpan(nwk.xpan, nwkData) == REG_SUCCESS,
                             value = ERROR_NOT_FOUND(NOT_FOUND_STR DOMAIN_STR));
                nwkData += '/';
            }
            nwkData += nwk.name;
            json[nwk.xpan.str()] = nwkData;
            value                = json.dump(JSON_INDENT_DEFAULT);
        }
    }
    else
    {
        ExitNow(value = ERROR_INVALID_COMMAND("{} is not a valid sub-command", aExpr[1]));
    }

exit:
    return value;
}

Interpreter::Value Interpreter::ProcessSessionId(const Expression &aExpr)
{
    Value              value;
    CommissionerAppPtr commissioner = nullptr;

    SuccessOrExit(value = mJobManager->GetSelectedCommissioner(commissioner));
    value = ProcessSessionIdJob(commissioner, aExpr);
exit:
    return value;
}

Interpreter::Value Interpreter::ProcessSessionIdJob(CommissionerAppPtr &aCommissioner, const Expression &)
{
    Value    value;
    uint16_t sessionId;

    SuccessOrExit(value = aCommissioner->GetSessionId(sessionId));
    value = std::to_string(sessionId);

exit:
    return value;
}

Interpreter::Value Interpreter::ProcessNetworkList(const Expression &aExpr)
{
    using namespace ot::commissioner::persistent_storage;

    Interpreter::Value   value;
    Expression           retExpr;
    network              nwk{};
    std::vector<network> networks;
    ::nlohmann::json     json;

    SuccessOrExit(value = ReParseMultiNetworkSyntax(aExpr, retExpr));
    // export file specification must be single or omitted
    VerifyOrExit(mContext.mExportFiles.size() < 2, value = ERROR_INVALID_SYNTAX(SYNTAX_MULTI_EXPORT));
    // import file specification must be single or omitted
    VerifyOrExit(mContext.mImportFiles.size() < 2, value = ERROR_INVALID_SYNTAX(SYNTAX_MULTI_IMPORT));
    // network and domain must not be specified simultaneously
    VerifyOrExit(mContext.mNwkAliases.size() == 0 || mContext.mDomAliases.size() == 0,
                 value = ERROR_INVALID_SYNTAX(SYNTAX_NWK_DOM_MUTUAL));

    if (mContext.mDomAliases.size() > 0)
    {
        VerifyOrExit(mRegistry->get_networks_in_domain(mContext.mDomAliases[0], networks) ==
                         registry_status::REG_SUCCESS,
                     value = ERROR_NOT_FOUND("failed to found networks in domain '{}'", mContext.mDomAliases[0]));
        ExitNow(value = ERROR_NONE);
    }
    else if (mContext.mNwkAliases.size() == 0)
    {
        VerifyOrExit(mRegistry->get_all_networks(networks) == REG_SUCCESS,
                     value = ERROR_NOT_FOUND(NOT_FOUND_STR NETWORK_STR));
    }
    else
    {
        // Quickly check group aliases
        for (auto alias : mContext.mNwkAliases)
        {
            if (alias == ALIAS_ALL || alias == ALIAS_OTHERS)
                VerifyOrExit(mContext.mNwkAliases.size() == 1, value = ERROR_INVALID_SYNTAX(SYNTAX_GROUP_ALIAS, alias));
        }

        StringArray    unresolved;
        RegistryStatus status = mRegistry->get_networks_by_aliases(mContext.mNwkAliases, networks, unresolved);
        for (auto alias : unresolved)
        {
            PrintNetworkMessage(alias, "failed to resolve", COLOR_ALIAS_FAILED);
        }
        VerifyOrExit(status == REG_SUCCESS, value = ERROR_NOT_FOUND(NOT_FOUND_STR NETWORK_STR));
    }

    json  = networks;
    value = json.dump(JSON_INDENT_DEFAULT);

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
        CommissionerAppPtr commissioner = nullptr;

        SuccessOrExit(value = mJobManager->GetSelectedCommissioner(commissioner));
        VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS("too few arguments"));

        if (CaseInsensitiveEqual(aExpr[2], "locator"))
        {
            uint16_t locator;
            SuccessOrExit(value = commissioner->GetBorderAgentLocator(locator));
            value = ToHex(locator);
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
    Value              value;
    JoinerType         type;
    CommissionerAppPtr commissioner = nullptr;

    VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS("too few arguments"));
    SuccessOrExit(value = GetJoinerType(type, aExpr[2]));
    SuccessOrExit(value = mJobManager->GetSelectedCommissioner(commissioner));

    if (CaseInsensitiveEqual(aExpr[1], "enable"))
    {
        uint64_t    eui64;
        std::string pskd;
        std::string provisioningUrl;

        VerifyOrExit(aExpr.size() >= (type == JoinerType::kMeshCoP ? 5 : 4),
                     value = ERROR_INVALID_ARGS("too few arguments"));
        SuccessOrExit(value = ParseInteger(eui64, aExpr[3]));
        if (type == JoinerType::kMeshCoP)
        {
            pskd = aExpr[4];
            if (aExpr.size() >= 6)
            {
                provisioningUrl = aExpr[5];
            }
        }

        SuccessOrExit(value = commissioner->EnableJoiner(type, eui64, pskd, provisioningUrl));
    }
    else if (CaseInsensitiveEqual(aExpr[1], "enableall"))
    {
        std::string pskd;
        std::string provisioningUrl;
        if (type == JoinerType::kMeshCoP)
        {
            VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS("too few arguments"));
            pskd = aExpr[3];
            if (aExpr.size() >= 5)
            {
                provisioningUrl = aExpr[4];
            }
        }

        SuccessOrExit(value = commissioner->EnableAllJoiners(type, pskd, provisioningUrl));
    }
    else if (CaseInsensitiveEqual(aExpr[1], "disable"))
    {
        uint64_t eui64;
        VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS("too few arguments"));
        SuccessOrExit(value = ParseInteger(eui64, aExpr[3]));
        SuccessOrExit(value = commissioner->DisableJoiner(type, eui64));
    }
    else if (CaseInsensitiveEqual(aExpr[1], "disableall"))
    {
        SuccessOrExit(value = commissioner->DisableAllJoiners(type));
    }
    else if (CaseInsensitiveEqual(aExpr[1], "getport"))
    {
        uint16_t port;
        SuccessOrExit(value = commissioner->GetJoinerUdpPort(port, type));
        value = std::to_string(port);
    }
    else if (CaseInsensitiveEqual(aExpr[1], "setport"))
    {
        uint16_t port;
        VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS("too few arguments"));
        SuccessOrExit(value = ParseInteger(port, aExpr[3]));
        SuccessOrExit(value = commissioner->SetJoinerUdpPort(type, port));
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
    Value              value;
    CommissionerAppPtr commissioner = nullptr;

    SuccessOrExit(value = mJobManager->GetSelectedCommissioner(commissioner));
    value = ProcessCommDatasetJob(commissioner, aExpr);
exit:
    return value;
}

Interpreter::Value Interpreter::ProcessCommDatasetJob(CommissionerAppPtr &aCommissioner, const Expression &aExpr)
{
    Value value;

    VerifyOrExit(aExpr.size() >= 2, value = ERROR_INVALID_ARGS("too few arguments"));

    if (CaseInsensitiveEqual(aExpr[1], "get"))
    {
        CommissionerDataset dataset;
        SuccessOrExit(value = aCommissioner->GetCommissionerDataset(dataset, 0xFFFF));
        value = CommissionerDatasetToJson(dataset);
    }
    else if (CaseInsensitiveEqual(aExpr[1], "set"))
    {
        CommissionerDataset dataset;
        VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS("too few arguments"));
        SuccessOrExit(value = CommissionerDatasetFromJson(dataset, aExpr[2]));
        SuccessOrExit(value = aCommissioner->SetCommissionerDataset(dataset));
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
    Value              value;
    CommissionerAppPtr commissioner = nullptr;

    SuccessOrExit(value = mJobManager->GetSelectedCommissioner(commissioner));
    value = ProcessOpDatasetJob(commissioner, aExpr);
exit:
    return value;
}

Interpreter::Value Interpreter::ProcessOpDatasetJob(CommissionerAppPtr &aCommissioner, const Expression &aExpr)
{
    Value value;
    bool  isSet;

    VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS("too few arguments"));
    if (CaseInsensitiveEqual(aExpr[1], "get"))
    {
        isSet = false;
    }
    else if (CaseInsensitiveEqual(aExpr[1], "set"))
    {
        isSet = true;
    }
    else
    {
        ExitNow(value = ERROR_INVALID_COMMAND("{} is not a valid sub-command", aExpr[1]));
    }

    if (CaseInsensitiveEqual(aExpr[2], "activetimestamp"))
    {
        Timestamp activeTimestamp;
        VerifyOrExit(!isSet, value = ERROR_INVALID_ARGS("cannot set activetimestamp"));
        SuccessOrExit(value = aCommissioner->GetActiveTimestamp(activeTimestamp));
        value = ToString(activeTimestamp);
    }
    else if (CaseInsensitiveEqual(aExpr[2], "channel"))
    {
        Channel channel;
        if (isSet)
        {
            uint32_t delay;
            VerifyOrExit(aExpr.size() >= 6, value = ERROR_INVALID_ARGS("too few arguments"));
            SuccessOrExit(value = ParseInteger(channel.mPage, aExpr[3]));
            SuccessOrExit(value = ParseInteger(channel.mNumber, aExpr[4]));
            SuccessOrExit(value = ParseInteger(delay, aExpr[5]));
            SuccessOrExit(value = aCommissioner->SetChannel(channel, CommissionerApp::MilliSeconds(delay)));
        }
        else
        {
            SuccessOrExit(value = aCommissioner->GetChannel(channel));
            value = ToString(channel);
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "channelmask"))
    {
        ChannelMask channelMask;
        if (isSet)
        {
            SuccessOrExit(value = ParseChannelMask(channelMask, aExpr, 3));
            SuccessOrExit(value = aCommissioner->SetChannelMask(channelMask));
        }
        else
        {
            SuccessOrExit(value = aCommissioner->GetChannelMask(channelMask));
            value = ToString(channelMask);
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "xpanid"))
    {
        ByteArray xpanid;
        if (isSet)
        {
            VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS("too few arguments"));
            SuccessOrExit(value = utils::Hex(xpanid, aExpr[3]));
            SuccessOrExit(value = aCommissioner->SetExtendedPanId(xpanid));
        }
        else
        {
            SuccessOrExit(value = aCommissioner->GetExtendedPanId(xpanid));
            value = utils::Hex(xpanid);
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "meshlocalprefix"))
    {
        std::string prefix;
        if (isSet)
        {
            uint32_t delay;
            VerifyOrExit(aExpr.size() >= 5, value = ERROR_INVALID_ARGS("too few arguments"));
            SuccessOrExit(value = ParseInteger(delay, aExpr[4]));
            SuccessOrExit(value = aCommissioner->SetMeshLocalPrefix(aExpr[3], CommissionerApp::MilliSeconds(delay)));
        }
        else
        {
            SuccessOrExit(value = aCommissioner->GetMeshLocalPrefix(prefix));
            value = prefix;
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "networkmasterkey"))
    {
        ByteArray masterKey;
        if (isSet)
        {
            uint32_t delay;
            VerifyOrExit(aExpr.size() >= 5, value = ERROR_INVALID_ARGS("too few arguments"));
            SuccessOrExit(value = utils::Hex(masterKey, aExpr[3]));
            SuccessOrExit(value = ParseInteger(delay, aExpr[4]));
            SuccessOrExit(value = aCommissioner->SetNetworkMasterKey(masterKey, CommissionerApp::MilliSeconds(delay)));
        }
        else
        {
            SuccessOrExit(value = aCommissioner->GetNetworkMasterKey(masterKey));
            value = utils::Hex(masterKey);
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "networkname"))
    {
        std::string networkName;
        if (isSet)
        {
            VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS("too few arguments"));
            SuccessOrExit(value = aCommissioner->SetNetworkName(aExpr[3]));
        }
        else
        {
            SuccessOrExit(value = aCommissioner->GetNetworkName(networkName));
            value = networkName;
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "panid"))
    {
        PanId panid;
        if (isSet)
        {
            uint32_t delay;
            VerifyOrExit(aExpr.size() >= 5, value = ERROR_INVALID_ARGS("too few arguments"));
            SuccessOrExit(value = ParseInteger(panid, aExpr[3]));
            SuccessOrExit(value = ParseInteger(delay, aExpr[4]));
            SuccessOrExit(value = aCommissioner->SetPanId(panid, CommissionerApp::MilliSeconds(delay)));
        }
        else
        {
            SuccessOrExit(value = aCommissioner->GetPanId(panid));
            value = std::to_string(panid);
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "pskc"))
    {
        ByteArray pskc;
        if (isSet)
        {
            VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS("too few arguments"));
            SuccessOrExit(value = utils::Hex(pskc, aExpr[3]));
            SuccessOrExit(value = aCommissioner->SetPSKc(pskc));
        }
        else
        {
            SuccessOrExit(value = aCommissioner->GetPSKc(pskc));
            value = utils::Hex(pskc);
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "securitypolicy"))
    {
        SecurityPolicy policy;
        if (isSet)
        {
            VerifyOrExit(aExpr.size() >= 5, value = ERROR_INVALID_ARGS("too few arguments"));
            SuccessOrExit(value = ParseInteger(policy.mRotationTime, aExpr[3]));
            SuccessOrExit(value = utils::Hex(policy.mFlags, aExpr[4]));
            SuccessOrExit(value = aCommissioner->SetSecurityPolicy(policy));
        }
        else
        {
            SuccessOrExit(value = aCommissioner->GetSecurityPolicy(policy));
            value = ToString(policy);
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "active"))
    {
        ActiveOperationalDataset dataset;
        StringArray              nwkAliases;
        StringArray              unresolved;
        XpanIdArray              xpans;

        if (isSet)
        {
            VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS("too few arguments"));
            SuccessOrExit(value = ActiveDatasetFromJson(dataset, aExpr[3]));
            SuccessOrExit(value = aCommissioner->SetActiveDataset(dataset));
        }
        else
        {
            SuccessOrExit(value = aCommissioner->GetActiveDataset(dataset, 0xFFFF));
            value = ActiveDatasetToJson(dataset);
        }
        // Update network from active opdataset (no error there, log messages only)
        do
        {
            persistent_storage::network nwk;
            // Get network object for the opdataset object
            if ((dataset.mPresentFlags & ActiveOperationalDataset::kExtendedPanIdBit) != 0)
            {
                xpans.push_back(dataset.mExtendedPanId);
            }
            else
            {
                if ((dataset.mPresentFlags & ActiveOperationalDataset::kPanIdBit) != 0)
                {
                    std::ostringstream panIdStream;
                    panIdStream << "0x" << std::hex << std::setfill('0') << std::setw(4) << std::uppercase
                                << dataset.mPanId;
                    nwkAliases.push_back(panIdStream.str());
                }
                else if ((dataset.mPresentFlags & ActiveOperationalDataset::kNetworkNameBit) != 0)
                {
                    nwkAliases.push_back(dataset.mNetworkName);
                }
                else
                {
                    mConsole.Write("Dataset contains no network identification data", Console::Color::kYellow);
                    break;
                }
                if (mRegistry->get_network_xpans_by_aliases(nwkAliases, xpans, unresolved) !=
                        persistent_storage::registry_status::REG_SUCCESS ||
                    xpans.size() != 1)
                {
                    mConsole.Write(fmt::format(FMT_STRING("Failed to load network XPAN by alias '{}': got {} XPANs"),
                                               nwkAliases[0], xpans.size()),
                                   Console::Color::kYellow);
                    break;
                }
            }
            if (mRegistry->get_network_by_xpan(xpans[0], nwk) != persistent_storage::registry_status::REG_SUCCESS)
            {
                mConsole.Write(fmt::format(FMT_STRING("Failed to load network by XPAN '{}'"), xpans[0].str()),
                               Console::Color::kYellow);
                break;
            }

// Update network object
#define AODS_FIELD_IF_IS_SET(field, defaultValue) \
    (((dataset.mPresentFlags & ActiveOperationalDataset::k##field##Bit) == 0) ? (defaultValue) : (dataset.m##field))

            nwk.name    = AODS_FIELD_IF_IS_SET(NetworkName, "");
            nwk.xpan    = AODS_FIELD_IF_IS_SET(ExtendedPanId, xpan_id{0});
            nwk.channel = AODS_FIELD_IF_IS_SET(Channel, (Channel{0, 0})).mNumber;
            nwk.pan     = AODS_FIELD_IF_IS_SET(PanId, PanId{0});
            if ((dataset.mPresentFlags & ActiveOperationalDataset::kPanIdBit) == 0)
            {
                nwk.pan = "";
            }
            else
            {
                std::ostringstream value;
                value << "0x" << std::uppercase << std::hex << std::setw(4) << std::setfill('0')
                      << dataset.mPanId.mValue;
                nwk.pan = value.str();
            }

            if ((dataset.mPresentFlags & ActiveOperationalDataset::kMeshLocalPrefixBit) == 0)
            {
                nwk.mlp = "";
            }
            else
            {
                nwk.mlp = Ipv6PrefixToString(dataset.mMeshLocalPrefix);
            }
            // Magic constant originates from the Spec pt. 8.10.1.15
            // The flag set indicates 'CCM **not** supported' state
            nwk.ccm =
                (((AODS_FIELD_IF_IS_SET(SecurityPolicy, (SecurityPolicy{0, ByteArray{0, 0}})).mFlags[0] & 0x04) == 0)
                     ? 1
                     : 0);
#undef AODS_FIELD_IF_IS_SET
            // Store network object in the registry
            mRegistry->update(nwk);
        } while (false);
    }
    else if (CaseInsensitiveEqual(aExpr[2], "pending"))
    {
        PendingOperationalDataset dataset;
        if (isSet)
        {
            VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS("too few arguments"));
            SuccessOrExit(value = PendingDatasetFromJson(dataset, aExpr[3]));
            SuccessOrExit(value = aCommissioner->SetPendingDataset(dataset));
        }
        else
        {
            SuccessOrExit(value = aCommissioner->GetPendingDataset(dataset, 0xFFFF));
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
    Value              value;
    CommissionerAppPtr commissioner = nullptr;

    SuccessOrExit(value = mJobManager->GetSelectedCommissioner(commissioner));
    value = ProcessBbrDatasetJob(commissioner, aExpr);
exit:
    return value;
}

Interpreter::Value Interpreter::ProcessBbrDatasetJob(CommissionerAppPtr &aCommissioner, const Expression &aExpr)
{
    Value value;
    bool  isSet;

    VerifyOrExit(aExpr.size() >= 2, value = ERROR_INVALID_ARGS("too few arguments"));
    if (CaseInsensitiveEqual(aExpr[1], "get"))
    {
        isSet = false;
    }
    else if (CaseInsensitiveEqual(aExpr[1], "set"))
    {
        isSet = true;
    }
    else
    {
        ExitNow(value = ERROR_INVALID_COMMAND("{} is not a valid sub-command", aExpr[1]));
    }

    if (aExpr.size() == 2 && !isSet)
    {
        BbrDataset dataset;
        SuccessOrExit(value = aCommissioner->GetBbrDataset(dataset, 0xFFFF));
        ExitNow(value = BbrDatasetToJson(dataset));
    }

    VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS("too few arguments"));

    if (CaseInsensitiveEqual(aExpr[2], "trihostname"))
    {
        std::string triHostname;
        if (isSet)
        {
            VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS("too few arguments"));
            SuccessOrExit(value = aCommissioner->SetTriHostname(aExpr[3]));
        }
        else
        {
            SuccessOrExit(value = aCommissioner->GetTriHostname(triHostname));
            value = triHostname;
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "reghostname"))
    {
        std::string regHostname;
        if (isSet)
        {
            VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS("too few arguments"));
            SuccessOrExit(value = aCommissioner->SetRegistrarHostname(aExpr[3]));
        }
        else
        {
            SuccessOrExit(value = aCommissioner->GetRegistrarHostname(regHostname));
            value = regHostname;
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "regaddr"))
    {
        std::string regAddr;
        VerifyOrExit(!isSet, value = ERROR_INVALID_ARGS("cannot set  read-only Registrar Address"));
        SuccessOrExit(value = aCommissioner->GetRegistrarIpv6Addr(regAddr));
        value = regAddr;
    }
    else
    {
        if (isSet)
        {
            BbrDataset dataset;
            SuccessOrExit(value = BbrDatasetFromJson(dataset, aExpr[2]));
            SuccessOrExit(value = aCommissioner->SetBbrDataset(dataset));
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
    Value              value;
    CommissionerAppPtr commissioner = nullptr;

    SuccessOrExit(value = mJobManager->GetSelectedCommissioner(commissioner));
    VerifyOrExit(aExpr.size() >= 2, value = ERROR_INVALID_ARGS("too few arguments"));
    SuccessOrExit(value = commissioner->Reenroll(aExpr[1]));
exit:
    return value;
}

Interpreter::Value Interpreter::ProcessDomainReset(const Expression &aExpr)
{
    Value              value;
    CommissionerAppPtr commissioner = nullptr;

    SuccessOrExit(value = mJobManager->GetSelectedCommissioner(commissioner));
    VerifyOrExit(aExpr.size() >= 2, value = ERROR_INVALID_ARGS("too few arguments"));
    SuccessOrExit(value = commissioner->DomainReset(aExpr[1]));

exit:
    return value;
}

Interpreter::Value Interpreter::ProcessMigrate(const Expression &aExpr)
{
    Value              value;
    CommissionerAppPtr commissioner = nullptr;

    VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS("too few arguments"));
    SuccessOrExit(value = mJobManager->GetSelectedCommissioner(commissioner));
    SuccessOrExit(value = commissioner->Migrate(aExpr[1], aExpr[2]));
exit:
    return value;
}

Interpreter::Value Interpreter::ProcessMlr(const Expression &aExpr)
{
    uint32_t           timeout;
    Value              value;
    CommissionerAppPtr commissioner = nullptr;

    VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS("too few arguments"));
    SuccessOrExit(value = mJobManager->GetSelectedCommissioner(commissioner));
    SuccessOrExit(value = ParseInteger(timeout, aExpr.back()));
    SuccessOrExit(value = commissioner->RegisterMulticastListener({aExpr.begin() + 1, aExpr.end() - 1},
                                                                  CommissionerApp::Seconds(timeout)));
exit:
    return value;
}

Interpreter::Value Interpreter::ProcessAnnounce(const Expression &aExpr)
{
    uint32_t           channelMask;
    uint8_t            count;
    uint16_t           period;
    Value              value;
    CommissionerAppPtr commissioner = nullptr;

    VerifyOrExit(aExpr.size() >= 5, value = ERROR_INVALID_ARGS("too few arguments"));
    SuccessOrExit(value = mJobManager->GetSelectedCommissioner(commissioner));
    SuccessOrExit(value = ParseInteger(channelMask, aExpr[1]));
    SuccessOrExit(value = ParseInteger(count, aExpr[2]));
    SuccessOrExit(value = ParseInteger(period, aExpr[3]));
    SuccessOrExit(value =
                      commissioner->AnnounceBegin(channelMask, count, CommissionerApp::MilliSeconds(period), aExpr[4]));
exit:
    return value;
}

Interpreter::Value Interpreter::ProcessPanId(const Expression &aExpr)
{
    Value              value;
    CommissionerAppPtr commissioner = nullptr;

    SuccessOrExit(value = mJobManager->GetSelectedCommissioner(commissioner));
    VerifyOrExit(aExpr.size() >= 2, value = ERROR_INVALID_ARGS("too few arguments"));

    if (CaseInsensitiveEqual(aExpr[1], "query"))
    {
        uint32_t channelMask;
        uint16_t panId;
        VerifyOrExit(aExpr.size() >= 5, value = ERROR_INVALID_ARGS("too few arguments"));
        SuccessOrExit(value = ParseInteger(channelMask, aExpr[2]));
        SuccessOrExit(value = ParseInteger(panId, aExpr[3]));
        SuccessOrExit(value = commissioner->PanIdQuery(channelMask, panId, aExpr[4]));
    }
    else if (CaseInsensitiveEqual(aExpr[1], "conflict"))
    {
        uint16_t panId;
        bool     conflict;
        VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS("too few arguments"));
        SuccessOrExit(value = ParseInteger(panId, aExpr[2]));
        conflict = commissioner->HasPanIdConflict(panId);
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
    Value              value;
    CommissionerAppPtr commissioner = nullptr;

    SuccessOrExit(value = mJobManager->GetSelectedCommissioner(commissioner));
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
        SuccessOrExit(value = commissioner->EnergyScan(channelMask, count, period, scanDuration, aExpr[6]));
    }
    else if (CaseInsensitiveEqual(aExpr[1], "report"))
    {
        if (aExpr.size() >= 3)
        {
            const EnergyReport *report = nullptr;
            Address             dstAddr;
            SuccessOrExit(value = dstAddr.Set(aExpr[2]));
            report = commissioner->GetEnergyReport(dstAddr);
            value  = report == nullptr ? "null" : EnergyReportToJson(*report);
        }
        else
        {
            auto reports = commissioner->GetAllEnergyReports();
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
    mJobManager->StopCommissionerPool();

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
        value = data;
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

void Interpreter::BorderAgentHandler(const BorderAgent *aBorderAgent, const Error &aError)
{
    if (aError != ErrorCode::kNone)
    {
        Console::Write(aError.ToString(), Console::Color::kRed);
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
    BUILD_STRING(Discriminator, utils::Hex(aBorderAgent.mDiscriminator));
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

static void from_json(const nlohmann::json &aJson, BorderAgent::State &aState)
{
    uint32_t stateVal = aJson.get<uint32_t>();
    aState            = BorderAgent::State(stateVal);
}

void Interpreter::BorderAgentFromJson(const nlohmann::json &aJson, BorderAgent &aAgent)
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
    SET_IF_PRESENT(ExtendedPanId);
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

static void to_json(nlohmann::json &aJson, const BorderAgent::State &aState)
{
    aJson = static_cast<uint32_t>(aState);
}

void Interpreter::BorderAgentToJson(nlohmann::json &aJson, const BorderAgent &aAgent)
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
    SET_IF_PRESENT(ExtendedPanId);
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
