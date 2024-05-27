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
 *   The file defines CLI interpreter.
 */

#ifndef OT_COMM_APP_CLI_INTERPRETER_HPP_
#define OT_COMM_APP_CLI_INTERPRETER_HPP_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "app/border_agent.hpp"
#include "app/cli/console.hpp"
#include "app/commissioner_app.hpp"
#include "app/ps/registry.hpp"
#include "commissioner/error.hpp"
#include "commissioner/network_data.hpp"

namespace ot {

namespace commissioner {

typedef std::shared_ptr<CommissionerApp> CommissionerAppPtr;

class JobManager;

class Interpreter
{
    friend class InterpreterTestSuite;

public:
    using Expression     = std::vector<std::string>;
    using StringArray    = std::vector<std::string>;
    using Registry       = ot::commissioner::persistent_storage::Registry;
    using RegistryStatus = Registry::Status;

    Interpreter()  = default;
    ~Interpreter() = default;

    Error Init(const std::string &aConfigFile, const std::string &aRegistry);

    void Run();

    void CancelCommand();

private:
    friend class Job;
    friend class JobManager;

    struct NetworkSelectionComparator
    {
        const Interpreter &mInterpreter;
        uint64_t           mStartWith;
        bool               mSuccess;

        NetworkSelectionComparator(const Interpreter &aInterpreter);
        ~NetworkSelectionComparator();
    };
    /**
     * The result value of an Expression processed by the Interpreter.
     * Specifically, it is an union of Error and std::string.
     *
     */
    class Value
    {
    public:
        Value() = default;

        // Allow implicit conversion from std::string to Value.
        Value(std::string aData);

        // Allow implicit conversion from Error to Value.
        Value(Error aError);

        bool operator==(const ErrorCode &aErrorCode) const;
        bool operator!=(const ErrorCode &aErrorCode) const;

        std::string ToString() const;
        bool        HasNoError() const;

    private:
        Error       mError;
        std::string mData;
    };

    using Evaluator    = std::function<Value(Interpreter *, const Expression &)>;
    using JobEvaluator = std::function<Value(Interpreter *, CommissionerAppPtr &, const Expression &)>;

    /**
     * Multi-network command context.
     *
     * Filled in by ReParseMultiNetworkSyntax(). After that the
     * context is validated by ValidateMultiNetworkSyntax().
     */
    struct MultiNetCommandContext
    {
        StringArray mNwkAliases;
        StringArray mDomAliases;
        StringArray mExportFiles;
        StringArray mImportFiles;
        StringArray mCommandKeys;

        void Cleanup();
        bool HasGroupAlias();
    } mContext;

private:
    Expression Read();
    Value      Eval(const Expression &aExpr);
    /**
     * Prints the collected resultant value to console unless export
     * to a file is expected by the executed command. In the latter
     * case the collected value itself is exported to the specified
     * file while sole '[done|failed]' command result indication is
     * printed to console.
     */
    void       PrintOrExport(const Value &aValue);
    void       PrintNetworkMessage(uint64_t aNid, std::string aMessage, Console::Color aColor);
    void       PrintNetworkMessage(std::string alias, std::string aMessage, Console::Color aColor);
    Expression ParseExpression(const std::string &aLiteral);
    /**
     * Tests if the current expression belongs to a particular group
     * of commands. Mostly used in syntax validation procedure.
     */
    bool IsFeatureSupported(const std::vector<StringArray> &aArr, const Expression &aExpr) const;
    /**
     * Tests if network/domain-wise syntax encountered in the tested
     * command parameters.
     */
    bool IsMultiNetworkSyntax(const Expression &aExpr);
    /**
     * Asserts if the tested command eligible for multi-job execution.
     */
    bool IsMultiJob(const Expression &aExpr);
    /**
     * Tests if the command allows execution with
     * inactive/disconnected @ref CommissionerApp object.
     *
     * @see @ref JobManager::CommissionerPool
     */
    bool IsInactiveCommissionerAllowed(const Expression &aExpr);
    /**
     * Implements network/domain-wise syntax validation. Import syntax
     * applicability is also taken into consideration. Besides,
     * resolution of the provided network aliases is checked in the
     * course of execution.
     */
    Value ValidateMultiNetworkSyntax(const Expression &aExpr, std::vector<uint64_t> &aNids);
    /**
     * Resolves network aliases into a set of network ids. In the
     * course of resolution, duplicate network ids are compacted if
     * encountered. The resultant expression is cleaned of the
     * re-parsed multi-network syntax elements.
     *
     * @see @ref Interpreter::MultiNetCommandContext
     */
    Error ReParseMultiNetworkSyntax(const Expression &aExpr, Expression &aRetExpr);
    /**
     * Updates on-screen visualization of the current network
     * selection.
     *
     * If selected, network name is added to command prompt, and with
     * no network selected the prompt is empty. Besides, if onStart
     * flag passed, a message is produced regarding the last session
     * network selection restored if there was any.
     */
    Error UpdateNetworkSelectionInfo(bool onStart = false);

    Value ProcessConfig(const Expression &aExpr);
    Value ProcessStart(const Expression &aExpr);
    Value ProcessStop(const Expression &aExpr);
    Value ProcessActive(const Expression &aExpr);
    Value ProcessToken(const Expression &aExpr);
    Value ProcessBr(const Expression &aExpr);
    Value ProcessDomain(const Expression &aExpr);
    Value ProcessNetwork(const Expression &aExpr);
    Value ProcessNetworkList(const Expression &aExpr);
    Value ProcessSessionId(const Expression &aExpr);
    Value ProcessBorderAgent(const Expression &aExpr);
    Value ProcessJoiner(const Expression &aExpr);
    Value ProcessCommDataset(const Expression &aExpr);
    Value ProcessOpDataset(const Expression &aExpr);
    Value ProcessBbrDataset(const Expression &aExpr);
    Value ProcessReenroll(const Expression &aExpr);
    Value ProcessDomainReset(const Expression &aExpr);
    Value ProcessMigrate(const Expression &aExpr);
    Value ProcessMlr(const Expression &aExpr);
    Value ProcessAnnounce(const Expression &aExpr);
    Value ProcessPanId(const Expression &aExpr);
    Value ProcessEnergy(const Expression &aExpr);
    Value ProcessExit(const Expression &aExpr);
    Value ProcessHelp(const Expression &aExpr);
    Value ProcessDiag(const Expression &aExpr);

    Value ProcessStartJob(CommissionerAppPtr &aCommissioner, const Expression &aExpr);
    Value ProcessStopJob(CommissionerAppPtr &aCommissioner, const Expression &aExpr);
    Value ProcessActiveJob(CommissionerAppPtr &aCommissioner, const Expression &aExpr);
    Value ProcessSessionIdJob(CommissionerAppPtr &aCommissioner, const Expression &aExpr);
    Value ProcessCommDatasetJob(CommissionerAppPtr &aCommissioner, const Expression &aExpr);
    Value ProcessOpDatasetJob(CommissionerAppPtr &aCommissioner, const Expression &aExpr);
    Value ProcessBbrDatasetJob(CommissionerAppPtr &aCommissioner, const Expression &aExpr);
    Value ProcessDiagJob(CommissionerAppPtr &aCommissioner, const Expression &aExpr);

    static void BorderAgentHandler(const BorderAgent *aBorderAgent, const Error &aError);

    static const std::string Usage(Expression aExpr);
    static Error             GetJoinerType(JoinerType &aType, const std::string &aStr);
    static Error             ParseChannelMask(ChannelMask &aChannelMask, const Expression &aExpr, size_t aIndex);
    static std::string       ToString(const Timestamp &aTimestamp);
    static std::string       ToString(const Channel &aChannel);
    static std::string       ToString(const ChannelMask &aChannelMask);
    static std::string       ToString(const SecurityPolicy &aSecurityPolicy);
    static std::string       ToString(const EnergyReport &aReport);
    static std::string       ToString(const BorderAgent &aBorderAgent);
    static std::string       ToString(const BorderAgent::State &aState);
    static std::string       BaConnModeToString(uint32_t aConnMode);
    static std::string       BaThreadIfStatusToString(uint32_t aIfStatus);
    static std::string       BaAvailabilityToString(uint32_t aAvailability);

private:
    std::shared_ptr<JobManager> mJobManager                  = nullptr;
    std::shared_ptr<Registry>   mRegistry                    = nullptr;
    std::string                 mThreadAdministratorPasscode = "";

    bool mShouldExit = false;

    std::atomic_bool mCancelCommand;
    /**
     * Pipe object intended to be added to event loop to listen on for
     * command cancellation.
     *
     * @note So far, used solely for breaking `br scan' execution.
     */
    int mCancelPipe[2] = {-1, -1};

    static const std::map<std::string, std::string>  &mUsageMap;
    static const std::map<std::string, Evaluator>    &mEvaluatorMap;
    static const std::vector<StringArray>            &mMultiNetworkSyntax;
    static const std::vector<StringArray>            &mMultiJobExecution;
    static const std::vector<StringArray>            &mInactiveCommissionerExecution;
    static const std::vector<StringArray>            &mExportSyntax;
    static const std::vector<StringArray>            &mImportSyntax;
    static const std::map<std::string, JobEvaluator> &mJobEvaluatorMap;
};

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_APP_CLI_INTERPRETER_HPP_
