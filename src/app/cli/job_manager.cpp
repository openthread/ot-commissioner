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
 *   The file implements command job manager.
 */

#include "app/cli/job_manager.hpp"
#include "app/cli/interpreter.hpp"
#include "app/cli/job.hpp"
#include "common/error_macros.hpp"
#include "common/utils.hpp"

namespace ot {

namespace commissioner {

Error JobManager::Init(const Config &aConf, Interpreter &aInterpreter)
{
    // here we need to store a pointer to persistent storage object

    mConf        = aConf;
    mInterpreter = InterpreterPtr(&aInterpreter);
    return CommissionerApp::Create(mDefaultCommissioner, aConf);
}

Error JobManager::UpdateDefaultConfig(const ByteArray &aPSKc)
{
    Error error;

    VerifyOrExit(aPSKc.size() <= kMaxPSKcLength, error = ERROR_INVALID_ARGS("invalid PSKc length"));
    VerifyOrExit(!mDefaultCommissioner->IsActive(),
                 error = ERROR_INVALID_STATE("cannot set PSKc when the commissioner is active"));

    mConf.mPSKc = aPSKc;
    CommissionerApp::Create(mDefaultCommissioner, mConf).IgnoreError();

exit:
    return error;
}

Error JobManager::CreateNewJob(CommissionerAppPtr &aCommissioner, const Interpreter::Expression &aExpr)
{
    Interpreter::JobEvaluator eval;
    auto                      mapItem = Interpreter::mJobEvaluatorMap.find(aExpr[0]);

    if (mapItem != Interpreter::mJobEvaluatorMap.end())
    {
        return ERROR_INVALID_SYNTAX("{} not eligible for job", aExpr[0]);
    }
    eval     = mapItem->second;
    Job *job = new Job(*mInterpreter, aCommissioner, aExpr, eval);
    mJobPool.push_back(job);

    return ERROR_NONE;
}

Error JobManager::PrepareJobs(const Interpreter::Expression &aExpr, const NidArray &aNids, bool aGroupAlias)
{
    if (aExpr[0] == "start")
        return PrepareStartJobs(aExpr, aNids, aGroupAlias);
    else if (aExpr[0] == "stop")
        return PrepareStopJobs(aExpr, aNids, aGroupAlias);

    Error error = ERROR_NONE;
    // regular command
    for (auto nid : aNids)
    {
        auto entry = mCommissionerPool.find(nid);
        if (entry == mCommissionerPool.end())
        {
            if (!aGroupAlias)
            {
                // report 'not started' for nid
            }
            // ignore nid
            continue;
        }

        // nid found
        bool active = entry->second->IsActive();
        if (!active)
        {
            if (!aGroupAlias)
            {
                // report 'not started' for nid
            }
            // ignore nid
            continue;
        }

        SuccessOrExit(error = CreateNewJob(entry->second, aExpr));
    }
exit:
    return error;
}

Error JobManager::PrepareStartJobs(const Interpreter::Expression &aExpr, const NidArray &aNids, bool aGroupAlias)
{
    Config conf  = mConf;
    Error  error = ERROR_NONE;

    ASSERT(aExpr[0] == "start");
    /*
     * Coming here is a result of using multi-network syntax.
     * Therefore, no extra arguments to be used, otherwise it
     * is multi-network syntax violation.
     */
    ASSERT(aExpr.size() == 1);

    for (auto nid : aNids)
    {
        auto entry = mCommissionerPool.find(nid);
        if (entry == mCommissionerPool.end())
        {
            CommissionerAppPtr commissioner;

            SuccessOrExit(error = PrepareDtlsConfig(nid, conf));
            SuccessOrExit(error = CommissionerApp::Create(commissioner, conf));
            mCommissionerPool[nid] = commissioner;
        }

        bool active = entry->second->IsActive();
        if (active)
        {
            if (!aGroupAlias)
            {
                // report 'already started' for nid
            }
            // ignore nid
            continue;
        }

        SuccessOrExit(error = PrepareDtlsConfig(nid, conf));

        // resolve nid to border_router

        // augment aExpr with br_addr and br_port that are expected by Job handler

        ASSERT(aExpr.size() == 3); // 'start br_addr br_port'
        SuccessOrExit(error = CreateNewJob(entry->second, aExpr));
    }
exit:
    return error;
}

Error JobManager::PrepareStopJobs(const Interpreter::Expression &aExpr, const NidArray &aNids, bool aGroupAlias)
{
    Error error = ERROR_NONE;

    ASSERT(aExpr[0] == "stop");

    for (auto nid : aNids)
    {
        auto entry = mCommissionerPool.find(nid);
        if (entry == mCommissionerPool.end())
        {
            if (!aGroupAlias)
            {
                // report failure for nid
            }
            // ignore nid
            continue;
        }

        // nid found
        bool active = entry->second->IsActive();
        if (!active)
        {
            if (!aGroupAlias)
            {
                // report 'already stopped' for nid
            }
            // ignore nid
            continue;
        }

        SuccessOrExit(error = CreateNewJob(entry->second, aExpr));
    }
exit:
    return error;
}

Error JobManager::PrepareDtlsConfig(const uint64_t aNid, Config &aConfig)
{
    // prepare DTLS conf - resolve Security Materials paths
    // update conf with actual paths

    (void)aNid;
    (void)aConfig;

    return ERROR_NONE;
}

void JobManager::RunJobs()
{
    for (auto job : mJobPool)
    {
        ASSERT(job != NULL);
        job->Run();
    }
}

void JobManager::CancelCommand()
{
    for (auto job : mJobPool)
    {
        ASSERT(job != NULL);
        job->Cancel();
    }
    Wait();
}

void JobManager::Wait()
{
    for (auto job : mJobPool)
    {
        ASSERT(job != NULL);
        job->Wait();
    }
}

void JobManager::StopCommissionerPool()
{
    // going blatant straightforward
    // TODO: actually need to do that in threads
    for (auto commissionerEntry : mCommissionerPool)
    {
        CommissionerAppPtr commissioner = commissionerEntry.second;

        if (commissioner->IsActive())
            commissioner->Stop();
    }
    if (mDefaultCommissioner->IsActive())
        mDefaultCommissioner->Stop();
}

CommissionerAppPtr &JobManager::GetSelectedCommissioner()
{
    // get selected nid
    // if not void, find CommissionerApp in pool
    // else
    return mDefaultCommissioner;
}

} // namespace commissioner

} // namespace ot
