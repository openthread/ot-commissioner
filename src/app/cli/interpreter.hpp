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

#include <map>

#include "app/border_agent.hpp"
#include "app/cli/console.hpp"
#include "app/commissioner_app.hpp"
#include "app/ps/registry.hpp"

namespace ot {

namespace commissioner {

using CommissionerAppPtr = std::shared_ptr<CommissionerApp>;
using NidArray           = std::vector<uint64_t>;

class JobManager;

class Interpreter
{
public:
    Interpreter()  = default;
    ~Interpreter() = default;

    Error Init(const std::string &aConfigFile, const std::string &aRegistry);

    void Run();

    void CancelCommand();

private:
    friend class Job;
    friend class JobManager;
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
        Value(std::string aData)
            : mData(aData)
        {
        }

        // Allow implicit conversion from Error to Value.
        Value(Error aError)
            : mError(aError)
        {
        }

        bool operator==(const ErrorCode &aErrorCode) const { return mError.GetCode() == aErrorCode; }
        bool operator!=(const ErrorCode &aErrorCode) const { return !(*this == aErrorCode); }

        std::string ToString() const;
        bool        HasNoError() const;

    private:
        Error       mError;
        std::string mData;
    };

    using Expression = std::vector<std::string>;
    using Evaluator  = std::function<Value(Interpreter *, const Expression &)>;

    using JobEvaluator = std::function<Value(Interpreter *, CommissionerAppPtr &, const Expression &)>;
    using StringArray  = std::vector<std::string>;

private:
    Expression Read();

    Value Eval(const Expression &aExpr);

    void Print(const Value &aValue);

    void       PrintNetworkMessage(uint64_t aNid, std::string aMessage, Console::Color aColor);
    Expression ParseExpression(const std::string &aLiteral);
    bool       IsSyntaxSupported(const std::vector<StringArray> &aArr, const Expression &aExpr) const;
    bool       IsMultiNetworkSyntax(const Expression &aExpr);
    bool       IsMultiThreadProcessing(const Expression &aExpr);
    Value EvaluateMultiNetwork(const Expression &aExpr, const StringArray &nwkAliases, const StringArray &domAliases);
    Error ReParseMultiNetworkSyntax(const Expression &aExpr,
                                    Expression &      aRretExpr,
                                    StringArray &     aNwkAliases,
                                    StringArray &     aDomAliases,
                                    StringArray &     aExport,
                                    StringArray &     aImport);

    Value ProcessConfig(const Expression &aExpr);
    Value ProcessStart(const Expression &aExpr);
    Value ProcessStop(const Expression &aExpr);
    Value ProcessActive(const Expression &aExpr);
    Value ProcessToken(const Expression &aExpr);
    Value ProcessNetwork(const Expression &aExpr);
    Value ProcessNetworkList(const Expression &aExpr);
    Value ProcessNetworkSelect(const Expression &aExpr);
    Value ProcessNetworkIdentity(const Expression &aExpr);
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

    Value ProcessStartJob(CommissionerAppPtr &aCommissioner, const Expression &aExpr);
    Value ProcessStopJob(CommissionerAppPtr &aCommissioner, const Expression &aExpr);
    Value ProcessActiveJob(CommissionerAppPtr &aCommissioner, const Expression &aExpr);
    Value ProcessSessionIdJob(CommissionerAppPtr &aCommissioner, const Expression &aExpr);
    Value ProcessCommDatasetJob(CommissionerAppPtr &aCommissioner, const Expression &aExpr);
    Value ProcessOpDatasetJob(CommissionerAppPtr &aCommissioner, const Expression &aExpr);
    Value ProcessBbrDatasetJob(CommissionerAppPtr &aCommissioner, const Expression &aExpr);

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
    Console                     mConsole;
    std::shared_ptr<JobManager> mJobManager = nullptr;
    std::shared_ptr<::ot::commissioner::persistent_storage::registry> mRegistry   = nullptr;

    bool mShouldExit = false;

    static const std::map<std::string, std::string> & mUsageMap;
    static const std::map<std::string, Evaluator> &   mEvaluatorMap;
    static const std::vector<StringArray> &           mMultiNetworkSupported;
    static const std::vector<StringArray> &           mMultiThreadSupported;
    static const std::vector<StringArray> &           mExportSupported;
    static const std::vector<StringArray> &           mImportSupported;
    static const std::map<std::string, JobEvaluator> &mJobEvaluatorMap;
};

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_APP_CLI_INTERPRETER_HPP_
