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

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include "app/border_agent.hpp"
#include "app/br_discover.hpp"
#include "app/cli/console.hpp"
#include "app/cli/interpreter.hpp"
#include "app/cli/job_manager.hpp"
#include "app/commissioner_app.hpp"
#include "app/file_util.hpp"
#include "app/json.hpp"
#include "app/mdns_handler.hpp"
#include "app/ps/registry.hpp"
#include "app/ps/registry_entries.hpp"
#include "app/sys_logger.hpp"
#include "commissioner/commissioner.hpp"
#include "commissioner/defines.hpp"
#include "commissioner/error.hpp"
#include "commissioner/network_data.hpp"
#include "common/address.hpp"
#include "common/error_macros.hpp"
#include "common/utils.hpp"
#include "event2/event.h"
#include "event2/event_struct.h"
#include "event2/util.h"
#include "fmt/core.h"
#include "fmt/format.h"
#include "mdns/mdns.h"
#include "nlohmann/json.hpp"

#define KEYWORD_NETWORK "--nwk"
#define KEYWORD_DOMAIN "--dom"
#define KEYWORD_EXPORT "--export"
#define KEYWORD_IMPORT "--import"

#define ALIAS_THIS "this"
#define ALIAS_NONE "none"
#define ALIAS_ALL "all"
#define ALIAS_OTHERS "other"

#define SYNTAX_NO_PARAM "keyword '{}' used with no parameter"
#define SYNTAX_UNKNOWN_KEY "unknown keyword '{}' encountered"
#define SYNTAX_MULTI_DOMAIN "multiple domain specification not allowed"
#define SYNTAX_MULTI_EXPORT "multiple files not allowed for --export"
#define SYNTAX_MULTI_IMPORT "multiple files not allowed for --import"
#define SYNTAX_NWK_DOM_MUTUAL "--nwk and --dom are mutually exclusive"
#define SYNTAX_EXP_IMP_MUTUAL "--export and --import are mutually exclusive"
#define SYNTAX_NOT_SUPPORTED "'{}' is not supported by the command"
#define SYNTAX_GROUP_ALIAS "'{}' must not combine with any other network alias"
#define SYNTAX_INVALID_COMMAND "'{}' is not a valid command, type 'help' to list all commands"
#define SYNTAX_INVALID_SUBCOMMAND "'{}' is not a valid sub-command"
#define SYNTAX_FEW_ARGS "too few arguments"
#define SYNTAX_MANY_ARGS "too many arguments"

#define RUNTIME_EMPTY_NIDS "specified alias(es) resolved to an empty list of networks"
#define RUNTIME_AMBIGUOUS "network alias '{}' is ambiguous"
#define RUNTIME_LOOKUP_FAILED "lookup failed"
#define RUNTIME_RESOLVING_FAILED "failed to resolve"
#define RUNTIME_CUR_NETWORK_FAILED "getting current network failed"

#define NOT_FOUND_STR "Failed to find registry entity of type: "
#define DOMAIN_STR "domain"
#define NETWORK_STR "network"
#define WARN_NETWORK_SELECTION_DROPPED "Network selection was dropped by the command"
#define WARN_NETWORK_SELECTION_CHANGED "Network selection was changed by the command"

#define COLOR_ALIAS_FAILED Console::Color::kYellow

namespace ot {

namespace commissioner {

using ot::commissioner::persistent_storage::Network;

namespace {
/**
 * File descriptor guard. Automatically closes the attached descriptor
 * on leaving guarded scope.
 */
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
    // For the time being it is enough to call close() on POSIX file
    // descriptors. If we'd want support Windows it would be necessary
    // to add support for platform specific file descriptor close
    // functions (like, say, closesocket() or even
    // mdns_socket_close()).
    ~FDGuard()
    {
        if (mFD > 0)
        {
            close(mFD);
        }
    }

    int mFD;
};

} // namespace

Interpreter::NetworkSelectionComparator::NetworkSelectionComparator(const Interpreter &aInterpreter)
    : mInterpreter(aInterpreter)
{
    Network        nwk;
    RegistryStatus status = mInterpreter.mRegistry->GetCurrentNetwork(nwk);

    mSuccess = status == RegistryStatus::kSuccess;
    if (mSuccess)
    {
        mStartWith = nwk.mXpan;
    }
}

Interpreter::NetworkSelectionComparator::~NetworkSelectionComparator()
{
    if (mSuccess)
    {
        Network        nwk;
        RegistryStatus status = mInterpreter.mRegistry->GetCurrentNetwork(nwk);

        if (status == RegistryStatus::kSuccess && mStartWith.mValue != nwk.mXpan.mValue)
        {
            Console::Write(nwk.mXpan.mValue == XpanId::kEmptyXpanId ? WARN_NETWORK_SELECTION_DROPPED
                                                                    : WARN_NETWORK_SELECTION_CHANGED,
                           Console::Color::kYellow);
        }
    }
}

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
    {"quit", &Interpreter::ProcessExit},           {"help", &Interpreter::ProcessHelp},
};

const std::map<std::string, std::string> &Interpreter::mUsageMap = *new std::map<std::string, std::string>{
    {"config", "config get admincode\n"
               "config set admincode <9-digits-thread-administrator-passcode>\n"
               "config get pskc\n"
               "config set pskc <pskc-hex-string>"},
    {"start", "start <border-agent-addr> <border-agent-port>\n"
              "start [ --nwk <network-alias-list | --dom <domain-alias>]"},
    {"stop", "stop\n"
             "stop [ --nwk <network-alias-list | --dom <domain-alias>]"},
    {"active", "active\n"
               "active [ --nwk <network-alias-list | --dom <domain-alias>]"},
    {"token", "token request <registrar-addr> <registrar-port>\n"
              "token print\n"
              "token set <signed-token-hex-string-file>"},
    {"br", "br list [--nwk <network-alias-list> | --dom <domain-name>]\n"
           "br add <json-file-path>\n"
           "br delete (<br-record-id> | --nwk <network-alias-list> | --dom <domain-name>)\n"
           "br scan [--nwk <network-alias-list> | --dom <domain-name>] [--export <json-file-path>] [--timeout <ms>]\n"},
    {"domain", "domain list [--dom <domain-name>]"},
    {"network", "network save <network-data-file>\n"
                "network sync\n"
                "network list [--nwk <network-alias-list> | --dom <domain-name>]\n"
                "network select <extended-pan-id>|<name>|<pan-id>|none\n"
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
    {"exit", "exit"},
    {"quit", "quit\n"
             "(an alias to 'exit' command)"},
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
        Interpreter::StringArray{"br", "scan"},
        Interpreter::StringArray{"domain", "list"},
        Interpreter::StringArray{"network", "list"},
        Interpreter::StringArray{"token", "request"},
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
        Interpreter::StringArray{"opdataset", "set", "active"},
        Interpreter::StringArray{"opdataset", "set", "pending"},
    };

const std::vector<Interpreter::StringArray> &Interpreter::mInactiveCommissionerExecution =
    *new std::vector<Interpreter::StringArray>{
        Interpreter::StringArray{"active"},
        Interpreter::StringArray{"token", "request"},
    };

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
    char    *endPtr = nullptr;

    integer = strtoull(aStr.c_str(), &endPtr, 0);

    VerifyOrExit(endPtr != nullptr && endPtr > aStr.c_str(),
                 error = ERROR_INVALID_ARGS("{} is not a valid integer", aStr));

    aInteger = integer;

exit:
    return error;
}

static inline std::string ToLower(const std::string &aStr)
{
    return utils::ToLower(aStr);
}

static inline bool CaseInsensitiveEqual(const std::string &aLhs, const std::string &aRhs)
{
    return ToLower(aLhs) == ToLower(aRhs);
}

bool Interpreter::MultiNetCommandContext::HasGroupAlias()
{
    for (const auto &alias : mNwkAliases)
    {
        if (alias == ALIAS_ALL || alias == ALIAS_OTHERS)
            return true;
    }
    return false;
}

void Interpreter::MultiNetCommandContext::Cleanup()
{
    mNwkAliases.clear();
    mDomAliases.clear();
    mExportFiles.clear();
    mImportFiles.clear();
    mCommandKeys.clear();
}

Error Interpreter::UpdateNetworkSelectionInfo(bool onStart /*=false*/)
{
    Error   error;
    Network nwk;

    VerifyOrExit(mRegistry->GetCurrentNetwork(nwk) == Registry::Status::kSuccess,
                 error = ERROR_REGISTRY_ERROR(RUNTIME_CUR_NETWORK_FAILED));
    if (onStart && nwk.mXpan.mValue != XpanId::kEmptyXpanId)
    {
        Console::Write(fmt::format(FMT_STRING("Network selection recalled from previous session.\n"
                                              "Restored to [{}:'{}']"),
                                   nwk.mXpan.str(), nwk.mName),
                       Console::Color::kGreen);
    }
exit:
    Console::SetPrompt(nwk.mName);
    return error;
}

Error Interpreter::Init(const std::string &aConfigFile, const std::string &aRegistryFile)
{
    Error error;

    std::string verboseEnv = ToLower(SafeStr(getenv("VERBOSE")));
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

    int flags;

    mCancelCommand = false;

    mJobManager.reset(new JobManager(*this));
    SuccessOrExit(error = mJobManager->Init(config));
    mRegistry.reset(new Registry(aRegistryFile));
    VerifyOrExit(mRegistry != nullptr,
                 error = ERROR_OUT_OF_MEMORY("Failed to create registry for file '{}'", aRegistryFile));
    VerifyOrExit(mRegistry->Open() == RegistryStatus::kSuccess,
                 error = ERROR_REGISTRY_ERROR("registry failed to open"));

    VerifyOrExit(pipe(mCancelPipe) != -1,
                 error = ERROR_IO_ERROR("failed to initialize command cancellation structures"));
    // set both pipe FDs non-blocking
    flags = fcntl(mCancelPipe[0], F_GETFL, 0);
    fcntl(mCancelPipe[0], F_SETFL, flags | O_NONBLOCK);
    flags = fcntl(mCancelPipe[1], F_GETFL, 0);
    fcntl(mCancelPipe[1], F_SETFL, flags | O_NONBLOCK);
    // set up console verbosity
    gVerbose = verboseEnv == "1" || verboseEnv == "yes" || verboseEnv == "true";
    error    = UpdateNetworkSelectionInfo(true);
exit:
    return error;
}

void Interpreter::Run()
{
    Expression expr;

    VerifyOrExit(mJobManager != nullptr);

    while (!mShouldExit)
    {
        PrintOrExport(Eval(Read()));
    }

exit:
    return;
}

void Interpreter::CancelCommand()
{
    int cancelData = 1;
    mJobManager->CancelCommand();
    // For commands being handled in place
    mCancelCommand = true;

    // reading from non-blocking pipe to empty the one prior to
    // sending cancellation signal by writing to the pipe below
    if (read(mCancelPipe[0], &cancelData, sizeof(cancelData)) == -1 && errno == EAGAIN)
    {
        cancelData = -1;
        // No data in the pipe, write is safe (no overflow)
        ssize_t result = write(mCancelPipe[1], &cancelData, sizeof(cancelData));
        (void)result;
        // writing to the pipe results in invoking mdnsCancelEventQueue()
        // lambda and respectively breaking event loop
    }
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
    int        cancelData;

    mCancelCommand = false;
    // empty the pipe prior to starting command execution
    while (read(mCancelPipe[0], &cancelData, sizeof(cancelData)) != -1)
        ;

    // make sure first the command is known
    auto evaluator = mEvaluatorMap.find(ToLower(aExpr.front()));
    if (evaluator == mEvaluatorMap.end())
    {
        ExitNow(value = ERROR_INVALID_COMMAND(SYNTAX_INVALID_COMMAND, aExpr.front()));
    }
    // prepare for parsing next command
    mContext.Cleanup();

    SuccessOrExit(value = ReParseMultiNetworkSyntax(aExpr, retExpr));
    // export file specification must be single or omitted
    VerifyOrExit(mContext.mExportFiles.size() < 2, value = ERROR_INVALID_ARGS(SYNTAX_MULTI_EXPORT));
    // import file specification must be single or omitted
    VerifyOrExit(mContext.mImportFiles.size() < 2, value = ERROR_INVALID_ARGS(SYNTAX_MULTI_IMPORT));

    if (mContext.mExportFiles.size() > 0)
    {
        supported = IsFeatureSupported(mExportSyntax, retExpr);
        VerifyOrExit(supported, value = ERROR_INVALID_ARGS(SYNTAX_NOT_SUPPORTED, KEYWORD_EXPORT));
        // export and import must not be specified simultaneously
        VerifyOrExit(mContext.mImportFiles.size() == 0, value = ERROR_INVALID_ARGS(SYNTAX_EXP_IMP_MUTUAL));
    }
    else if (mContext.mImportFiles.size() > 0)
    {
        supported = IsFeatureSupported(mImportSyntax, retExpr);
        VerifyOrExit(supported, value = ERROR_INVALID_ARGS(SYNTAX_NOT_SUPPORTED, KEYWORD_IMPORT));
        // export and import must not be specified simultaneously
        VerifyOrExit(mContext.mExportFiles.size() == 0, value = ERROR_INVALID_ARGS(SYNTAX_EXP_IMP_MUTUAL));
        // let job manager take care of import data
        mJobManager->SetImportFile(mContext.mImportFiles.front());
    }

    if (mContext.mNwkAliases.size() > 0 || mContext.mDomAliases.size() > 0)
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
            VerifyOrExit(mContext.mImportFiles.size() == 0,
                         value = ERROR_INVALID_ARGS(
                             "Import syntax is not supported in synchronous processing of multi-network command"));

            value = evaluator->second(this, retExpr);
        }
    }
    else
    {
        // handle single command using selected network
        if (mContext.mImportFiles.size() > 0)
        {
            XpanId xpan = XpanId();
            if (mContext.mNwkAliases.empty())
            {
                auto result = mRegistry->GetCurrentNetworkXpan(xpan);
                if (result != Registry::Status::kSuccess)
                {
                    xpan = XpanId();
                }
            }
            else
            {
                XpanIdArray nwks;
                StringArray unresolved;
                VerifyOrExit(mRegistry->GetNetworkXpansByAliases(mContext.mNwkAliases, nwks, unresolved) ==
                                 Registry::Status::kSuccess,
                             value = ERROR_INVALID_ARGS("Failed to resolve network alias for import"));
                xpan = nwks[0];
            }
            SuccessOrExit(value = mJobManager->AppendImport(xpan, retExpr));
        }
        value = evaluator->second(this, retExpr);
    }
exit:
    // It is necessary to cleanup import file anyways
    mJobManager->CleanupJobs();
    return value;
}

bool Interpreter::IsMultiNetworkSyntax(const Expression &aExpr)
{
    for (const auto &word : aExpr)
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
        // MN-syntax is also supported for import from multi-entry JSON
        supported |= mContext.mImportFiles.size() > 0 && mContext.mNwkAliases.size() == 1 &&
                     mContext.mNwkAliases[0] != ALIAS_ALL && mContext.mNwkAliases[0] != ALIAS_OTHERS;
        VerifyOrExit(supported, error = ERROR_INVALID_ARGS(SYNTAX_NOT_SUPPORTED, KEYWORD_NETWORK));
        // network and domain must not be specified simultaneously
        VerifyOrExit(mContext.mDomAliases.size() == 0, error = ERROR_INVALID_ARGS(SYNTAX_NWK_DOM_MUTUAL));
        // validate group alias usage; if used, it must be alone
        for (const auto &alias : mContext.mNwkAliases)
        {
            if (alias == ALIAS_ALL || alias == ALIAS_OTHERS)
            {
                VerifyOrExit(mContext.mNwkAliases.size() == 1, error = ERROR_INVALID_ARGS(SYNTAX_GROUP_ALIAS, alias));
            }
        }
        StringArray    unresolved;
        RegistryStatus status = mRegistry->GetNetworkXpansByAliases(mContext.mNwkAliases, aNids, unresolved);
        for (const auto &alias : unresolved)
        {
            std::string errorMessage;
            switch (status)
            {
            case RegistryStatus::kAmbiguity:
                errorMessage = "alias ambiguous";
                break;
            case RegistryStatus::kSuccess:  // partially successful resolution
            case RegistryStatus::kNotFound: // failed to find any
                errorMessage = "alias not found";
                break;
            default:
                errorMessage = RUNTIME_RESOLVING_FAILED;
            }
            PrintNetworkMessage(alias, errorMessage, COLOR_ALIAS_FAILED);
        }
        VerifyOrExit(aNids.size() > 0, error = ERROR_REGISTRY_ERROR(RUNTIME_EMPTY_NIDS));
        VerifyOrExit(status == RegistryStatus::kSuccess, error = ERROR_REGISTRY_ERROR(RUNTIME_RESOLVING_FAILED));
    }
    else if (mContext.mDomAliases.size() > 0)
    {
        // verify if respective syntax supported by the current command
        supported = IsFeatureSupported(mMultiNetworkSyntax, aExpr);
        VerifyOrExit(supported, error = ERROR_INVALID_ARGS(SYNTAX_NOT_SUPPORTED, KEYWORD_DOMAIN));
        // network and domain must not be specified simultaneously
        VerifyOrExit(mContext.mNwkAliases.size() == 0, error = ERROR_INVALID_ARGS(SYNTAX_NWK_DOM_MUTUAL));
        // domain must be single
        VerifyOrExit(mContext.mDomAliases.size() < 2, error = ERROR_INVALID_ARGS(SYNTAX_MULTI_DOMAIN));
        RegistryStatus status = mRegistry->GetNetworkXpansInDomain(mContext.mDomAliases.front(), aNids);
        VerifyOrExit(status == RegistryStatus::kSuccess,
                     error = status == RegistryStatus::kNotFound
                                 ? ERROR_REGISTRY_ERROR("domain '{}' not found", mContext.mDomAliases[0])
                                 : ERROR_REGISTRY_ERROR("domain '{}' failed to resolve with status={}",
                                                        mContext.mDomAliases.front(), static_cast<int>(status)));
    }
    else
    {
        // as we came here to evaluate multi-network command, either
        // network or domain list must have entries
        VerifyOrExit(false,
                     ERROR_INVALID_ARGS(
                         "Either network or domain list must have entries for multi-network command")); // must not hit
                                                                                                        // this, ever
    }

    VerifyOrExit(aNids.size() > 0, error = ERROR_INVALID_ARGS(RUNTIME_EMPTY_NIDS));
exit:
    return error;
}

bool Interpreter::IsFeatureSupported(const std::vector<StringArray> &aArr, const Expression &aExpr) const
{
    for (const auto &commandCase : aArr)
    {
        if (commandCase.size() > aExpr.size())
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

    for (const auto &word : aExpr)
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
            VerifyOrExit(!inKey, error = ERROR_INVALID_ARGS(SYNTAX_NO_PARAM, lastKey));
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
        error = ERROR_INVALID_ARGS(SYNTAX_NO_PARAM, lastKey);
    }
exit:
    return error;
}

void Interpreter::PrintOrExport(const Value &aValue)
{
    std::string output = aValue.ToString();

    if (aValue.HasNoError() && mContext.mExportFiles.size() > 0)
    {
        Error error;

        for (const auto &path : mContext.mExportFiles)
        {
            std::string filePath = path;

            // Make sure folder path exists, if it's not creatable, print error and output to console instead
            error = RestoreFilePath(path);
            if (error != ERROR_NONE)
            {
                Console::Write(error.GetMessage(), Console::Color::kRed);
                break;
            }
            ///
            /// @todo: maybe instead of overwriting an existing file
            ///        suggest a new file name by incrementing suffix
            ///        filePath[-suffix].ext
            ///
            error = WriteFile(aValue.ToString(), filePath);
            if (error != ERROR_NONE)
            {
                std::string out = fmt::format(FMT_STRING("failed to write to '{}'\n"), filePath);
                Console::Write(out, Console::Color::kRed);
                break;
            }
        }
        if (error == ERROR_NONE)
        {
            // value is written to file, no console output expected other than
            // [done]/[failed]
            output.clear();
        }
    }
    if (!output.empty())
    {
        output += "\n";
    }
    output += aValue.HasNoError() ? "[done]" : "[failed]";
    auto color = aValue.HasNoError() ? Console::Color::kGreen : Console::Color::kRed;

    Console::Write(output, color);
}

void Interpreter::PrintNetworkMessage(uint64_t aNid, std::string aMessage, Console::Color aColor)
{
    std::string nidHex = std::string(XpanId(aNid));
    PrintNetworkMessage(nidHex, aMessage, aColor);
}

void Interpreter::PrintNetworkMessage(std::string alias, std::string aMessage, Console::Color aColor)
{
    Console::Write(alias.append(": "));
    Console::Write(aMessage, aColor);
    Console::Write("\n");
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

    VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
    VerifyOrExit(aExpr[2] == "pskc" || aExpr[2] == "admincode",
                 value = ERROR_INVALID_ARGS("{} is not a valid property", aExpr[2]));
    if (aExpr[1] == "get")
    {
        if (aExpr[2] == "admincode")
        {
            ExitNow(value = mThreadAdministratorPasscode);
        }

        value = mJobManager->GetDefaultConfigPSKc();
    }
    else if (aExpr[1] == "set")
    {
        ByteArray pskc;

        VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
        if (aExpr[2] == "admincode")
        {
            mThreadAdministratorPasscode = aExpr[3];

            // TODO(rongli@): add len and last digit check per SPEC-1216v11
            pskc.assign(mThreadAdministratorPasscode.begin(), mThreadAdministratorPasscode.end());
        }
        else
        {
            SuccessOrExit(value = utils::Hex(pskc, aExpr[3]));
        }
        SuccessOrExit(value = mJobManager->UpdateDefaultConfigPSKc(pskc));
    }
    else
    {
        ExitNow(value = ERROR_INVALID_COMMAND(SYNTAX_INVALID_SUBCOMMAND, aExpr[1]));
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
        XpanId         nid;
        RegistryStatus status = mRegistry->GetCurrentNetworkXpan(nid);
        VerifyOrExit(status == RegistryStatus::kSuccess,
                     value = ERROR_REGISTRY_ERROR("getting selected network failed"));
        SuccessOrExit(value = mJobManager->GetSelectedCommissioner(commissioner));
        SuccessOrExit(value = mJobManager->MakeBorderRouterChoice(nid, br));
        expr.push_back(br.mAgent.mAddr);
        expr.push_back(std::to_string(br.mAgent.mPort));
        break;
    }
    case 2:
    {
        // starting newtork by br raw id (experimental, for dev tests only)
        persistent_storage::BorderRouterId rawid;
        NetworkSelectionComparator         guard(*this);

        SuccessOrExit(value = ParseInteger(rawid.mId, expr[1]));
        VerifyOrExit(mRegistry->GetBorderRouter(rawid, br) == RegistryStatus::kSuccess,
                     value = ERROR_NOT_FOUND("br[{}] not found", rawid.mId));
        VerifyOrExit(mRegistry->SetCurrentNetwork(br) == RegistryStatus::kSuccess,
                     value = ERROR_NOT_FOUND("network selection failed for nwk[{}]", br.mNetworkId.mId));
        SuccessOrExit(UpdateNetworkSelectionInfo());
        SuccessOrExit(value = mJobManager->GetSelectedCommissioner(commissioner));
        expr.pop_back();
        expr.push_back(br.mAgent.mAddr);
        expr.push_back(std::to_string(br.mAgent.mPort));
        break;
    }
    case 3:
    {
        NetworkSelectionComparator guard(*this);

        // starting network with explicit br_addr and br_port
        VerifyOrExit(mRegistry->ForgetCurrentNetwork() == RegistryStatus::kSuccess,
                     value = ERROR_REGISTRY_ERROR("failed to drop network selection"));
        SuccessOrExit(UpdateNetworkSelectionInfo());
        SuccessOrExit(value = mJobManager->GetSelectedCommissioner(commissioner));
        break;
    }
    default:
        ExitNow(value = ERROR_INVALID_COMMAND(SYNTAX_MANY_ARGS));
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

    VerifyOrExit(aExpr.size() >= 3, error = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
    SuccessOrExit(error = ParseInteger(port, aExpr[2]));
    SuccessOrExit(error = aCommissioner->Start(existingCommissionerId, aExpr[1], port));

exit:
    if (!existingCommissionerId.empty())
    {
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
    RegistryStatus     status;
    Network            curNwk;

    SuccessOrExit(value = mJobManager->GetSelectedCommissioner(commissioner));
    VerifyOrExit(aExpr.size() >= 2, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
    VerifyOrExit((status = mRegistry->GetCurrentNetwork(curNwk)) == RegistryStatus::kSuccess,
                 value = ERROR_REGISTRY_ERROR(RUNTIME_CUR_NETWORK_FAILED " with status={}", static_cast<int>(status)));

    if (CaseInsensitiveEqual(aExpr[1], "request"))
    {
        VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
        // request allowed in case of CCM network or no network selected
        if (curNwk.mId.mId == persistent_storage::EMPTY_ID || curNwk.mCcm != 0)
        {
            uint16_t port;
            SuccessOrExit(value = ParseInteger(port, aExpr[3]));
            SuccessOrExit(value = commissioner->RequestToken(aExpr[2], port));
            if (curNwk.mId.mId == persistent_storage::EMPTY_ID)
            {
                // when there is no network selected, a successful
                // 'token request' must update default configuration
                mJobManager->UpdateDefaultConfigCommissionerToken();
            }
        }
        else
        {
            ExitNow(value = ERROR_INVALID_STATE("request disallowed when non-CCM network selected"));
        }
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
        VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
        SuccessOrExit(value = ReadHexStringFile(signedToken, aExpr[2]));

        SuccessOrExit(value = commissioner->SetToken(signedToken));
        if (curNwk.mId.mId == persistent_storage::EMPTY_ID)
        {
            // when there is no network selected, a successful
            // 'token set' must update default configuration
            // as well
            mJobManager->UpdateDefaultConfigCommissionerToken();
        }
    }
    else
    {
        ExitNow(value = ERROR_INVALID_COMMAND(SYNTAX_INVALID_SUBCOMMAND, aExpr[1]));
    }

exit:
    return value;
}

Interpreter::Value Interpreter::ProcessBr(const Expression &aExpr)
{
    using namespace ot::commissioner::persistent_storage;

    Value value;

    VerifyOrExit(aExpr.size() >= 2, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
    if (CaseInsensitiveEqual(aExpr[1], "list"))
    {
        nlohmann::json            json;
        std::vector<BorderRouter> routers;
        std::vector<Network>      networks;
        RegistryStatus            status;

        VerifyOrExit(aExpr.size() == 2, value = ERROR_INVALID_ARGS(SYNTAX_MANY_ARGS));
        if (mContext.mDomAliases.size() > 0)
        {
            for (const auto &dom : mContext.mDomAliases)
            {
                std::vector<Network> domNetworks;
                status = mRegistry->GetNetworksInDomain(dom, domNetworks);
                if (status != RegistryStatus::kSuccess)
                {
                    PrintNetworkMessage(dom, "domain name unknown", COLOR_ALIAS_FAILED);
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
            status = mRegistry->GetNetworksByAliases(mContext.mNwkAliases, networks, unresolved);
            // no need to announce unresolved items here as those were
            // displayed in the course of ValidateMultiNetworkSyntax()
            VerifyOrExit(status == RegistryStatus::kSuccess, value = ERROR_REGISTRY_ERROR(RUNTIME_LOOKUP_FAILED));
        }
        else
        {
            status = mRegistry->GetAllBorderRouters(routers);
            if (status != RegistryStatus::kSuccess && status != RegistryStatus::kNotFound)
            {
                ExitNow(value = ERROR_REGISTRY_ERROR(RUNTIME_LOOKUP_FAILED));
            }
        }
        if (networks.size() > 0)
        {
            for (const auto &nwk : networks)
            {
                std::vector<BorderRouter> nwkRouters;
                status = mRegistry->GetBorderRoutersInNetwork(nwk.mXpan, nwkRouters);
                if (status == RegistryStatus::kSuccess)
                {
                    routers.insert(routers.end(), nwkRouters.begin(), nwkRouters.end());
                }
                else
                {
                    PrintNetworkMessage(nwk.mXpan.mValue, RUNTIME_LOOKUP_FAILED, COLOR_ALIAS_FAILED);
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
            value = Value("[]");
        }
    }
    else if (CaseInsensitiveEqual(aExpr[1], "add"))
    {
        Error                    error;
        std::string              jsonStr;
        nlohmann::json           json;
        std::vector<BorderAgent> agents;

        if (aExpr.size() < 3)
        {
            ExitNow(value = ERROR_INVALID_ARGS("input JSON file path required"));
        }
        else if (aExpr.size() > 3)
        {
            ExitNow(value = ERROR_INVALID_ARGS(SYNTAX_MANY_ARGS));
        }

        VerifyOrExit((error = JsonFromFile(jsonStr, aExpr[2])) == ERROR_NONE, value = Value(error));

        try
        {
            json = nlohmann::json::parse(jsonStr);
        } catch (std::exception &e)
        {
            ExitNow(value = ERROR_BAD_FORMAT("failed to parse JSON: {}", e.what()));
        }

        // Can be either single BorderAgent or an array of them.
        if (!json.is_array())
        {
            // Parse single BorderAgent
            BorderAgent ba;
            try
            {
                BorderAgentFromJson(ba, json);
            } catch (std::exception &e)
            {
                ExitNow(value = ERROR_BAD_FORMAT("incorrect border agent JSON format: {}", e.what()));
            }
            agents.push_back(ba);
        }
        else
        {
            for (const auto &jsonObject : json)
            {
                BorderAgent ba;
                try
                {
                    BorderAgentFromJson(ba, jsonObject);
                } catch (std::exception &e)
                {
                    ExitNow(value = ERROR_BAD_FORMAT("incorrect border agent JSON format: {}", e.what()));
                }
                agents.push_back(ba);
            }
        }
        // Sanity check of BorderAgent records
        for (auto iter = agents.begin(); iter != agents.end(); ++iter)
        {
            // Check mandatory fields present
            // By Thread 1.1.1 spec table 8-4
            constexpr const uint32_t kMandatoryFieldsBits =
                (BorderAgent::kAddrBit | BorderAgent::kPortBit | BorderAgent::kThreadVersionBit |
                 BorderAgent::kStateBit | BorderAgent::kNetworkNameBit | BorderAgent::kExtendedPanIdBit);
            if ((iter->mPresentFlags & kMandatoryFieldsBits) != kMandatoryFieldsBits)
            {
                ExitNow(value = ERROR_REJECTED(
                            "Missing mandatory border agent fields (fieldFlags = {:X}, expectedFlags = {:X})",
                            iter->mPresentFlags, kMandatoryFieldsBits));
            }

            // Check local network information sanity
            constexpr const uint32_t kNetworkInfoBits =
                (BorderAgent::kExtendedPanIdBit | BorderAgent::kNetworkNameBit | BorderAgent::kDomainNameBit);
            if (((iter->mPresentFlags & kNetworkInfoBits) != 0) &&
                (((iter->mPresentFlags & BorderAgent::kExtendedPanIdBit) == 0) || (iter->mExtendedPanId == 0)))
            {
                ExitNow(value = ERROR_REJECTED("XPAN required but not provided for border agent host:port {}:{}",
                                               iter->mAddr, iter->mPort));
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
                        flags.domain = (CHECK_PRESENT_FLAG(a, DomainName) && CHECK_PRESENT_FLAG(*iter, DomainName)) &&
                                       (iter->mExtendedPanId == a.mExtendedPanId && iter->mDomainName != a.mDomainName);
                        return flags.name || flags.domain;
#undef CHECK_PRESENT_FLAG
                    });
                if (found != iter)
                {
                    if (flags.addr)
                    {
                        value =
                            ERROR_REJECTED("Address {} and port {} combination is not unique for inbound border agents",
                                           iter->mAddr, iter->mPort);
                    }
                    else if (flags.name)
                    {
                        value = ERROR_REJECTED("Two inbound border agents have same XPAN '{}', but different network "
                                               "names ('{}' and '{}')",
                                               iter->mExtendedPanId, iter->mNetworkName, found->mNetworkName);
                    }
                    else if (flags.domain)
                    {
                        value = ERROR_REJECTED(
                            "Two inbound border agents have same XPAN '{}', but different domain names ('{}' and '{}')",
                            iter->mExtendedPanId, iter->mDomainName, found->mDomainName);
                    }
                    ExitNow();
                }
            }
        }

        for (const auto &agent : agents)
        {
            auto status = mRegistry->Add(agent);
            if (status != RegistryStatus::kSuccess)
            {
                ExitNow(value = ERROR_REGISTRY_ERROR("Insertion failure with border agent address {} and port {}",
                                                     agent.mAddr, agent.mPort));
            }
        }
    }
    else if (CaseInsensitiveEqual(aExpr[1], "delete"))
    {
        if (aExpr.size() > 3 || (aExpr.size() == 3 && !(mContext.mNwkAliases.empty() && mContext.mDomAliases.empty())))
        {
            ExitNow(value = ERROR_INVALID_ARGS("Too many arguments for `br delete` command`"));
        }
        else if (aExpr.size() == 3)
        {
            // Attempt to remove border agent by BorderRouterId
            BorderRouterId brId;
            try
            {
                brId = BorderRouterId(std::stoi(aExpr[2]));
            } catch (...)
            {
                ExitNow(value = ERROR_INVALID_ARGS("Failed to evaluate border router ID '{}'", aExpr[2]));
            }
            auto status = mRegistry->DeleteBorderRouterById(brId);
            if (status == RegistryStatus::kRestricted)
            {
                value = ERROR_REGISTRY_ERROR("Failed to delete border router ID {}: last router in the current network",
                                             aExpr[2]);
            }
            else if (status != RegistryStatus::kSuccess)
            {
                value = ERROR_REGISTRY_ERROR("Failed to delete border router ID {}", aExpr[2]);
            }
            VerifyOrExit(status == RegistryStatus::kSuccess);
        }
        else if (mContext.mNwkAliases.size() > 0)
        {
            StringArray unresolved;
            // Remove all border agents in the networks
            auto status = mRegistry->DeleteBorderRoutersInNetworks(mContext.mNwkAliases, unresolved);
            if (status == RegistryStatus::kRestricted)
            {
                value = ERROR_REGISTRY_ERROR("Can't delete all border routers from the current network");
            }
            else if (status != RegistryStatus::kSuccess)
            {
                value = Error(ErrorCode ::kIOError, "Failed to delete border routers");
            }
            // Report unresolved aliases
            for (const auto &alias : unresolved)
            {
                PrintNetworkMessage(alias, RUNTIME_RESOLVING_FAILED, COLOR_ALIAS_FAILED);
            }
            VerifyOrExit(status == RegistryStatus::kSuccess);
        }
        else if (mContext.mDomAliases.size() > 0)
        {
            StringArray undeleted;
            // Remove all border agents in the domain
            auto status = mRegistry->DeleteBorderRoutersInDomain(mContext.mDomAliases[0]);
            if (status == RegistryStatus::kRestricted)
            {
                value =
                    ERROR_REGISTRY_ERROR("Failed to delete border routers in the domain '{}' of the current network",
                                         mContext.mDomAliases[0]);
            }
            else if (status != RegistryStatus::kSuccess)
            {
                value =
                    ERROR_REGISTRY_ERROR("Failed to delete border routers in the domain '{}'", mContext.mDomAliases[0]);
            }
            VerifyOrExit(status == RegistryStatus::kSuccess);
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
        event_base                               *base;
        timeval                                   tvTimeout;
        std::unique_ptr<event, void (*)(event *)> mdnsEvent(nullptr, event_free);
        std::unique_ptr<event, void (*)(event *)> cancelEvent(nullptr, event_free);
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

        // Stop event queue on data in the mCancelPipe
        auto mdnsCancelEventQueue = [](evutil_socket_t aSocket, short aWhat, void *aArg) {
            (void)aSocket;
            (void)aWhat;
            event_base *ev_base = static_cast<event_base *>(aArg);
            event_base_loopbreak(ev_base);
        };
        // Attach cancel pipe to event loop. When bytes written to the
        // pipe, mdnsCancelEventQueue() lambda is invoked resulting in
        // breaking event loop
        cancelEvent = std::unique_ptr<event, void (*)(event *)>(
            event_new(base, mCancelPipe[0], EV_READ | EV_PERSIST, mdnsCancelEventQueue, base), event_free);
        event_add(cancelEvent.get(), nullptr);

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

        if (mCancelCommand)
        {
            int cancelData;
            while (read(mCancelPipe[0], &cancelData, sizeof(cancelData)) != -1)
                ;
            ExitNow(value = ERROR_CANCELLED("Scan cancelled by user"));
        }

        XpanIdArray xpans;
        StringArray unresolved;
        if (!mContext.mNwkAliases.empty())
        {
            VerifyOrExit(mRegistry->GetNetworkXpansByAliases(mContext.mNwkAliases, xpans, unresolved) ==
                             RegistryStatus::kSuccess,
                         value = ERROR_REGISTRY_ERROR("Failed to convert network aliases to XPAN IDs"));
            for (const auto &alias : unresolved)
            {
                PrintNetworkMessage(alias, RUNTIME_RESOLVING_FAILED, COLOR_ALIAS_FAILED);
            }
            VerifyOrExit(!xpans.empty(),
                         value = ERROR_NOT_FOUND(
                             "No known extended PAN ID discovered for network aliases. Please make complete rescan."));
        }

        // Serialize BorderAgents to JSON into value
        for (const auto &agentOrError : borderAgents)
        {
            if (agentOrError.mError != ErrorCode::kNone)
            {
                Console::Write(agentOrError.mError.GetMessage(), Console::Color::kRed);
            }
            else
            {
                nlohmann::json ba;
                BorderAgentToJson(agentOrError.mBorderAgent, ba);
                if ((mContext.mNwkAliases.empty() && mContext.mDomAliases.empty()) ||
                    (!mContext.mDomAliases.empty() &&
                     (agentOrError.mBorderAgent.mPresentFlags & BorderAgent::kDomainNameBit) &&
                     (agentOrError.mBorderAgent.mDomainName == mContext.mDomAliases[0])) ||
                    (!mContext.mNwkAliases.empty() &&
                     (agentOrError.mBorderAgent.mPresentFlags & BorderAgent::kExtendedPanIdBit) &&
                     (std::find(xpans.begin(), xpans.end(), XpanId(agentOrError.mBorderAgent.mExtendedPanId)) !=
                      xpans.end())))
                {
                    baJson.push_back(ba);
                }
            }
        }
        value = baJson.dump(JSON_INDENT_DEFAULT);
    }
    else
    {
        ExitNow(value = ERROR_INVALID_COMMAND(SYNTAX_INVALID_SUBCOMMAND, aExpr[1]));
    }

exit:
    return value;
} // namespace commissioner

Interpreter::Value Interpreter::ProcessDomain(const Expression &aExpr)
{
    using namespace ot::commissioner::persistent_storage;

    Value value;

    VerifyOrExit(aExpr.size() > 1, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
    VerifyOrExit(aExpr.size() == 2, value = ERROR_INVALID_ARGS(SYNTAX_MANY_ARGS));
    if (CaseInsensitiveEqual(aExpr[1], "list"))
    {
        RegistryStatus      status;
        nlohmann::json      json;
        std::vector<Domain> domains;

        for (const auto &alias : mContext.mDomAliases)
        {
            if (alias == ALIAS_ALL || alias == ALIAS_OTHERS)
            {
                ExitNow(value = ERROR_INVALID_ARGS("alias '{}' not supported by the command", alias));
            }
        }
        if (mContext.mDomAliases.size() != 0)
        {
            StringArray unresolved;
            status = mRegistry->GetDomainsByAliases(mContext.mDomAliases, domains, unresolved);
            for (const auto &alias : unresolved)
            {
                PrintNetworkMessage(alias, RUNTIME_RESOLVING_FAILED, COLOR_ALIAS_FAILED);
            }
            VerifyOrExit(status == RegistryStatus::kSuccess, value = ERROR_REGISTRY_ERROR(RUNTIME_LOOKUP_FAILED));
        }
        else
        {
            status = mRegistry->GetAllDomains(domains);
            if (status != RegistryStatus::kSuccess && status != RegistryStatus::kNotFound)
            {
                ExitNow(value = ERROR_REGISTRY_ERROR(RUNTIME_LOOKUP_FAILED));
            }
        }
        json  = domains;
        value = json.dump(JSON_INDENT_DEFAULT);
    }
    else
    {
        ExitNow(value = ERROR_INVALID_COMMAND(SYNTAX_INVALID_SUBCOMMAND, aExpr[1]));
    }

exit:
    return value;
}

Interpreter::Value Interpreter::ProcessNetwork(const Expression &aExpr)
{
    using namespace ot::commissioner::persistent_storage;

    Value              value;
    CommissionerAppPtr commissioner = nullptr;

    VerifyOrExit(aExpr.size() >= 2, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));

    if (CaseInsensitiveEqual(aExpr[1], "save"))
    {
        VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
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
        ::nlohmann::json json;

        VerifyOrExit(aExpr.size() == 3, value = ERROR_INVALID_ARGS(SYNTAX_MANY_ARGS));
        if (CaseInsensitiveEqual(aExpr[2], "none"))
        {
            RegistryStatus status = mRegistry->ForgetCurrentNetwork();
            VerifyOrExit(status == RegistryStatus::kSuccess,
                         value = ERROR_REGISTRY_ERROR("network unselection failed"));
        }
        else
        {
            StringArray aliases = {aExpr[2]};
            XpanIdArray xpans;
            StringArray unresolved;
            VerifyOrExit(mRegistry->GetNetworkXpansByAliases(aliases, xpans, unresolved) == RegistryStatus::kSuccess,
                         value = ERROR_REGISTRY_ERROR("Failed to resolve extended PAN Id for network '{}'", aExpr[2]));
            VerifyOrExit(xpans.size() == 1, value = ERROR_IO_ERROR("Detected {} networks instead of 1 for alias '{}'",
                                                                   xpans.size(), aExpr[2]));
            VerifyOrExit(mRegistry->SetCurrentNetwork(xpans[0]) == RegistryStatus::kSuccess,
                         value = ERROR_REGISTRY_ERROR("network selection failed"));
        }
        // update console prompt after network selection/deselection
        SuccessOrExit(UpdateNetworkSelectionInfo());
    }
    else if (CaseInsensitiveEqual(aExpr[1], "identify"))
    {
        ::nlohmann::json json;
        Network          nwk;
        std::string      nwkData;

        VerifyOrExit(aExpr.size() == 2, value = ERROR_INVALID_ARGS(SYNTAX_MANY_ARGS));
        VerifyOrExit(mRegistry->GetCurrentNetwork(nwk) == RegistryStatus::kSuccess,
                     value = ERROR_REGISTRY_ERROR(RUNTIME_CUR_NETWORK_FAILED));
        if (nwk.mId.mId == EMPTY_ID)
        {
            value = std::string("none");
        }
        else
        {
            if (nwk.mDomainId.mId != EMPTY_ID)
            {
                VerifyOrExit(mRegistry->GetDomainNameByXpan(nwk.mXpan, nwkData) == RegistryStatus::kSuccess,
                             value = ERROR_NOT_FOUND(NOT_FOUND_STR DOMAIN_STR));
                nwkData += '/';
            }
            nwkData += nwk.mName;
            json[nwk.mXpan.str()] = nwkData;
            value                 = json.dump(JSON_INDENT_DEFAULT);
        }
    }
    else
    {
        ExitNow(value = ERROR_INVALID_COMMAND(SYNTAX_INVALID_SUBCOMMAND, aExpr[1]));
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
    Network              nwk{};
    std::vector<Network> networks;
    ::nlohmann::json     json;

    SuccessOrExit(value = ReParseMultiNetworkSyntax(aExpr, retExpr));
    // export file specification must be single or omitted
    VerifyOrExit(mContext.mExportFiles.size() < 2, value = ERROR_INVALID_ARGS(SYNTAX_MULTI_EXPORT));
    // import file specification must be single or omitted
    VerifyOrExit(mContext.mImportFiles.size() < 2, value = ERROR_INVALID_ARGS(SYNTAX_MULTI_IMPORT));
    // network and domain must not be specified simultaneously
    VerifyOrExit(mContext.mNwkAliases.size() == 0 || mContext.mDomAliases.size() == 0,
                 value = ERROR_INVALID_ARGS(SYNTAX_NWK_DOM_MUTUAL));

    if (mContext.mDomAliases.size() > 0)
    {
        VerifyOrExit(mRegistry->GetNetworksInDomain(mContext.mDomAliases[0], networks) == RegistryStatus::kSuccess,
                     value = ERROR_NOT_FOUND("failed to find networks in domain '{}'", mContext.mDomAliases[0]));
    }
    else if (mContext.mNwkAliases.size() == 0)
    {
        RegistryStatus status = mRegistry->GetAllNetworks(networks);
        if (status != RegistryStatus::kSuccess && status != RegistryStatus::kNotFound)
        {
            value = ERROR_NOT_FOUND(NOT_FOUND_STR NETWORK_STR);
        }
    }
    else
    {
        // Quickly check group aliases
        for (const auto &alias : mContext.mNwkAliases)
        {
            if (alias == ALIAS_ALL || alias == ALIAS_OTHERS)
                VerifyOrExit(mContext.mNwkAliases.size() == 1, value = ERROR_INVALID_ARGS(SYNTAX_GROUP_ALIAS, alias));
        }

        StringArray    unresolved;
        RegistryStatus status = mRegistry->GetNetworksByAliases(mContext.mNwkAliases, networks, unresolved);
        for (const auto &alias : unresolved)
        {
            PrintNetworkMessage(alias, RUNTIME_RESOLVING_FAILED, COLOR_ALIAS_FAILED);
        }
        VerifyOrExit(status == RegistryStatus::kSuccess, value = ERROR_NOT_FOUND(NOT_FOUND_STR NETWORK_STR));
    }

    json  = networks;
    value = json.dump(JSON_INDENT_DEFAULT);

exit:
    return value;
}

Interpreter::Value Interpreter::ProcessBorderAgent(const Expression &aExpr)
{
    Value value;

    VerifyOrExit(aExpr.size() >= 2, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));

    if (CaseInsensitiveEqual(aExpr[1], "discover"))
    {
        uint64_t timeout = 4000;

        if (aExpr.size() >= 3)
        {
            SuccessOrExit(value = ParseInteger(timeout, aExpr[2]));
        }
        std::string netIf = "";

        if (aExpr.size() == 4)
        {
            netIf = aExpr[3];
        }

        SuccessOrExit(value = DiscoverBorderAgent(BorderAgentHandler, static_cast<size_t>(timeout), netIf));
    }
    else if (CaseInsensitiveEqual(aExpr[1], "get"))
    {
        CommissionerAppPtr commissioner = nullptr;

        SuccessOrExit(value = mJobManager->GetSelectedCommissioner(commissioner));
        VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));

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
        value = ERROR_INVALID_COMMAND(SYNTAX_INVALID_SUBCOMMAND, aExpr[1]);
    }

exit:
    return value;
}

Interpreter::Value Interpreter::ProcessJoiner(const Expression &aExpr)
{
    Value              value;
    JoinerType         type;
    CommissionerAppPtr commissioner = nullptr;

    VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
    SuccessOrExit(value = GetJoinerType(type, aExpr[2]));
    SuccessOrExit(value = mJobManager->GetSelectedCommissioner(commissioner));

    if (CaseInsensitiveEqual(aExpr[1], "enable"))
    {
        uint64_t    eui64;
        std::string pskd;
        std::string provisioningUrl;

        VerifyOrExit(aExpr.size() >= (type == JoinerType::kMeshCoP ? 5 : 4),
                     value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
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
            VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
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
        VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
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
        VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
        SuccessOrExit(value = ParseInteger(port, aExpr[3]));
        SuccessOrExit(value = commissioner->SetJoinerUdpPort(type, port));
    }
    else
    {
        value = ERROR_INVALID_COMMAND(SYNTAX_INVALID_SUBCOMMAND, aExpr[1]);
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

    VerifyOrExit(aExpr.size() >= 2, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));

    if (CaseInsensitiveEqual(aExpr[1], "get"))
    {
        CommissionerDataset dataset;
        SuccessOrExit(value = aCommissioner->GetCommissionerDataset(dataset, 0xFFFF));
        value = CommissionerDatasetToJson(dataset);
    }
    else if (CaseInsensitiveEqual(aExpr[1], "set"))
    {
        CommissionerDataset dataset;
        VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
        SuccessOrExit(value = CommissionerDatasetFromJson(dataset, aExpr[2]));
        SuccessOrExit(value = aCommissioner->SetCommissionerDataset(dataset));
    }
    else
    {
        value = ERROR_INVALID_COMMAND(SYNTAX_INVALID_SUBCOMMAND, aExpr[1]);
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

    VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
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
        ExitNow(value = ERROR_INVALID_COMMAND(SYNTAX_INVALID_SUBCOMMAND, aExpr[1]));
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
            VerifyOrExit(aExpr.size() >= 6, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
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
            VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
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
            VerifyOrExit(aExpr.size() >= 5, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
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
            VerifyOrExit(aExpr.size() >= 5, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
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
            VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
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
            VerifyOrExit(aExpr.size() >= 5, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
            SuccessOrExit(value = ParseInteger(panid, aExpr[3]));
            SuccessOrExit(value = ParseInteger(delay, aExpr[4]));
            SuccessOrExit(value = aCommissioner->SetPanId(panid, CommissionerApp::MilliSeconds(delay)));
        }
        else
        {
            SuccessOrExit(value = aCommissioner->GetPanId(panid));
            value = std::string(panid);
        }
    }
    else if (CaseInsensitiveEqual(aExpr[2], "pskc"))
    {
        ByteArray pskc;
        if (isSet)
        {
            VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
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
            VerifyOrExit(aExpr.size() >= 5, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
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
            VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
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
            Network nwk;
            // Get network object for the opdataset object
            if ((dataset.mPresentFlags & ActiveOperationalDataset::kExtendedPanIdBit) != 0)
            {
                xpans.push_back(dataset.mExtendedPanId);
            }
            else
            {
                if ((dataset.mPresentFlags & ActiveOperationalDataset::kPanIdBit) != 0)
                {
                    nwkAliases.push_back(fmt::format(FMT_STRING("0x{:04X}"), dataset.mPanId.mValue));
                }
                else if ((dataset.mPresentFlags & ActiveOperationalDataset::kNetworkNameBit) != 0)
                {
                    nwkAliases.push_back(dataset.mNetworkName);
                }
                else
                {
                    Console::Write("Dataset contains no network identification data", Console::Color::kYellow);
                    break;
                }
                if (mRegistry->GetNetworkXpansByAliases(nwkAliases, xpans, unresolved) != RegistryStatus::kSuccess ||
                    xpans.size() != 1)
                {
                    Console::Write(fmt::format(FMT_STRING("Failed to load network XPAN by alias '{}': got {} XPANs"),
                                               nwkAliases[0], xpans.size()),
                                   Console::Color::kYellow);
                    break;
                }
            }
            if (mRegistry->GetNetworkByXpan(xpans[0], nwk) != RegistryStatus::kSuccess)
            {
                Console::Write(fmt::format(FMT_STRING("Failed to find network record by XPAN '{}'"), xpans[0].str()),
                               Console::Color::kYellow);
                break; /// @todo It is possible that network XPAN ID
                       ///       might have change since the last sync
                       ///       so maybe it is worth to look up for
                       ///       the network by name or PAN ID as well
                       ///       to apply the change
            }

// Update network object
#define AODS_FIELD_IF_IS_SET(field, defaultValue) \
    (((dataset.mPresentFlags & ActiveOperationalDataset::k##field##Bit) == 0) ? (defaultValue) : (dataset.m##field))

            nwk.mName    = AODS_FIELD_IF_IS_SET(NetworkName, "");
            nwk.mXpan    = AODS_FIELD_IF_IS_SET(ExtendedPanId, XpanId{});
            nwk.mChannel = AODS_FIELD_IF_IS_SET(Channel, (Channel{0, 0})).mNumber;
            nwk.mPan     = AODS_FIELD_IF_IS_SET(PanId, PanId{});
            if ((dataset.mPresentFlags & ActiveOperationalDataset::kPanIdBit) == 0)
            {
                nwk.mPan = PanId::kEmptyPanId;
            }
            else
            {
                nwk.mPan = dataset.mPanId;
            }

            if ((dataset.mPresentFlags & ActiveOperationalDataset::kMeshLocalPrefixBit) == 0)
            {
                nwk.mMlp = "";
            }
            else
            {
                nwk.mMlp = Ipv6PrefixToString(dataset.mMeshLocalPrefix);
            }

            SecurityPolicy sp = AODS_FIELD_IF_IS_SET(SecurityPolicy, (SecurityPolicy{0, ByteArray{0, 0}}));
            nwk.mCcm          = !(sp.mFlags[0] & kSecurityPolicyBit_CCM);
#undef AODS_FIELD_IF_IS_SET
            // Store network object in the registry
            mRegistry->Update(nwk);
        } while (false);
    }
    else if (CaseInsensitiveEqual(aExpr[2], "pending"))
    {
        PendingOperationalDataset dataset;
        if (isSet)
        {
            VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
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

    VerifyOrExit(aExpr.size() >= 2, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
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
        ExitNow(value = ERROR_INVALID_COMMAND(SYNTAX_INVALID_SUBCOMMAND, aExpr[1]));
    }

    if (aExpr.size() == 2 && !isSet)
    {
        BbrDataset dataset;
        SuccessOrExit(value = aCommissioner->GetBbrDataset(dataset, 0xFFFF));
        ExitNow(value = BbrDatasetToJson(dataset));
    }

    VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));

    if (CaseInsensitiveEqual(aExpr[2], "trihostname"))
    {
        std::string triHostname;
        if (isSet)
        {
            VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
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
            VerifyOrExit(aExpr.size() >= 4, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
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
    VerifyOrExit(aExpr.size() >= 2, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
    SuccessOrExit(value = commissioner->Reenroll(aExpr[1]));
exit:
    return value;
}

Interpreter::Value Interpreter::ProcessDomainReset(const Expression &aExpr)
{
    Value              value;
    CommissionerAppPtr commissioner = nullptr;

    SuccessOrExit(value = mJobManager->GetSelectedCommissioner(commissioner));
    VerifyOrExit(aExpr.size() >= 2, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
    SuccessOrExit(value = commissioner->DomainReset(aExpr[1]));

exit:
    return value;
}

Interpreter::Value Interpreter::ProcessMigrate(const Expression &aExpr)
{
    Value              value;
    CommissionerAppPtr commissioner = nullptr;

    VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
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

    VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
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

    VerifyOrExit(aExpr.size() >= 5, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
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
    VerifyOrExit(aExpr.size() >= 2, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));

    if (CaseInsensitiveEqual(aExpr[1], "query"))
    {
        uint32_t channelMask;
        uint16_t panId;
        VerifyOrExit(aExpr.size() >= 5, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
        SuccessOrExit(value = ParseInteger(channelMask, aExpr[2]));
        SuccessOrExit(value = ParseInteger(panId, aExpr[3]));
        SuccessOrExit(value = commissioner->PanIdQuery(channelMask, panId, aExpr[4]));
    }
    else if (CaseInsensitiveEqual(aExpr[1], "conflict"))
    {
        uint16_t panId;
        bool     conflict;
        VerifyOrExit(aExpr.size() >= 3, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
        SuccessOrExit(value = ParseInteger(panId, aExpr[2]));
        conflict = commissioner->HasPanIdConflict(panId);
        value    = std::to_string(conflict);
    }
    else
    {
        value = ERROR_INVALID_COMMAND(SYNTAX_INVALID_SUBCOMMAND, aExpr[1]);
    }

exit:
    return value;
}

Interpreter::Value Interpreter::ProcessEnergy(const Expression &aExpr)
{
    Value              value;
    CommissionerAppPtr commissioner = nullptr;

    SuccessOrExit(value = mJobManager->GetSelectedCommissioner(commissioner));
    VerifyOrExit(aExpr.size() >= 2, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));

    if (CaseInsensitiveEqual(aExpr[1], "scan"))
    {
        uint32_t channelMask;
        uint8_t  count;
        uint16_t period;
        uint16_t scanDuration;
        VerifyOrExit(aExpr.size() >= 7, value = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));
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
        value = ERROR_INVALID_COMMAND(SYNTAX_INVALID_SUBCOMMAND, aExpr[1]);
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
        for (const auto &kv : mEvaluatorMap)
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
                 error = ERROR_INVALID_ARGS(SYNTAX_FEW_ARGS));

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

Interpreter::Value::Value(std::string aData)
    : mData(aData)
{
}

Interpreter::Value::Value(Error aError)
    : mError(aError)
{
}

bool Interpreter::Value::operator==(const ErrorCode &aErrorCode) const
{
    return mError.GetCode() == aErrorCode;
}

bool Interpreter::Value::operator!=(const ErrorCode &aErrorCode) const
{
    return !(*this == aErrorCode);
}

} // namespace commissioner

} // namespace ot
