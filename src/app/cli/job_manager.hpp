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
 *   The file defines command job manager.
 */

#ifndef OT_COMM_APP_CLI_JOB_MANAGER_HPP_
#define OT_COMM_APP_CLI_JOB_MANAGER_HPP_

#include "app/cli/interpreter.hpp"
#include "app/commissioner_app.hpp"

namespace ot {

namespace commissioner {

using CommissionerAppPtr = std::shared_ptr<CommissionerApp>;
using InterpreterPtr     = std::shared_ptr<Interpreter>;

class Interpreter;
class Job;

class JobManager
{
public:
    JobManager()  = default;
    ~JobManager() = default;

    Error               Init(const Config &aConf, Interpreter &aInterpreter);
    Error               PrepareJobs(const Interpreter::Expression &aExpr, const NidArray &aNids, bool aGroupAlias);
    void                RunJobs();
    void                CancelCommand();
    void                Wait();
    void                StopCommissionerPool();
    CommissionerAppPtr &GetSelectedCommissioner();
    /**
     * Apply new PSKc bytes and re-create @ref
     * JobManager::mDefaultCommissioner instance
     */
    Error UpdateDefaultConfig(const ByteArray &aPSKc);

private:
    using CommissionerPool = std::map<uint64_t, CommissionerAppPtr>;
    using JobPool          = std::vector<Job *>;

    Error PrepareStartJobs(const Interpreter::Expression &aExpr, const NidArray &aNids, bool aGroupAlias);
    Error PrepareStopJobs(const Interpreter::Expression &aExpr, const NidArray &aNids, bool aGroupAlias);
    Error PrepareDtlsConfig(const uint64_t aNid, Config &aConfig);
    Error CreateNewJob(CommissionerAppPtr &aCommissioner, const Interpreter::Expression &aExpr);

    JobPool            mJobPool;
    CommissionerPool   mCommissionerPool;
    Config             mConf;
    InterpreterPtr     mInterpreter         = nullptr;
    CommissionerAppPtr mDefaultCommissioner = nullptr;
};

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_APP_CLI_JOB_MANAGER_HPP_
