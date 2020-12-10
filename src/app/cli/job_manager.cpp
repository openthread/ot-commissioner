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
#include "app/cli/security_materials.hpp"
#include "app/ps/registry.hpp"
#include "common/error_macros.hpp"
#include "common/utils.hpp"
#include "library/logging.hpp"

namespace ot {

namespace commissioner {

Error JobManager::Init(const Config &aConf, Interpreter &aInterpreter)
{
    Error error;

    mDefaultConf = aConf;
    mInterpreter = InterpreterPtr(&aInterpreter);
    SuccessOrExit(error = sm::Init(aConf));
    SuccessOrExit(error = CommissionerApp::Create(mDefaultCommissioner, aConf));
exit:
    return error;
}

Error JobManager::UpdateDefaultConfigPSKc(const ByteArray &aPSKc)
{
    Error error;

    VerifyOrExit(aPSKc.size() <= kMaxPSKcLength, error = ERROR_INVALID_ARGS("invalid PSKc length"));
    VerifyOrExit(!mDefaultCommissioner->IsActive(),
                 error = ERROR_INVALID_STATE("cannot set PSKc when the commissioner is active"));

    mDefaultConf.mPSKc = aPSKc;
    CommissionerApp::Create(mDefaultCommissioner, mDefaultConf).IgnoreError();

exit:
    return error;
}

std::string JobManager::GetDefaultConfigPSKc() const
{
    return utils::Hex(mDefaultConf.mPSKc);
}

void JobManager::CleanupJobs()
{
    for (auto job : mJobPool)
    {
        ASSERT(job == NULL || job->IsStopped());
        delete job;
    }
    mJobPool.clear();
    mImportFile.clear();
}

void JobManager::SetImportFile(const std::string &importFile)
{
    mImportFile = importFile;
}

Error JobManager::CreateJob(CommissionerAppPtr &aCommissioner, const Interpreter::Expression &aExpr, uint64_t aXpanId)
{
    Interpreter::JobEvaluator eval;
    auto                      mapItem = Interpreter::mJobEvaluatorMap.find(aExpr[0]);

    if (mapItem != Interpreter::mJobEvaluatorMap.end())
    {
        return ERROR_INVALID_SYNTAX("{} not eligible for job", aExpr[0]);
    }
    eval     = mapItem->second;
    Job *job = new Job(*mInterpreter, aCommissioner, aExpr, eval, aXpanId);
    mJobPool.push_back(job);

    return ERROR_NONE;
}

Error JobManager::PrepareJobs(const Interpreter::Expression &aExpr, const NidArray &aNids, bool aGroupAlias)
{
    if (aExpr[0] == "start")
        return PrepareStartJobs(aExpr, aNids, aGroupAlias);
    else if (aExpr[0] == "stop")
        return PrepareStopJobs(aExpr, aNids, aGroupAlias);

    Error error;

    for (auto nid : aNids)
    {
        auto entry = mCommissionerPool.find(nid);
        if (entry == mCommissionerPool.end())
        {
            if (!aGroupAlias)
            {
                WarningMsg(nid, "not started");
            }
            continue;
        }

        bool active = entry->second->IsActive();
        if (!active)
        {
            if (!aGroupAlias)
            {
                WarningMsg(nid, "not started");
            }
            continue;
        }

        Interpreter::Expression jobExpr = aExpr;

        if (!mImportFile.empty())
        {
            Error importError = AppendImport(nid, jobExpr);
            if (importError != ERROR_NONE)
            {
                ErrorMsg(nid, "import entry not found");
                continue;
            }
        }

        SuccessOrExit(error = CreateJob(entry->second, jobExpr, nid));
    }
exit:
    return error;
}

Error JobManager::PrepareStartJobs(const Interpreter::Expression &aExpr, const NidArray &aNids, bool aGroupAlias)
{
    Config conf  = mDefaultConf;
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
                InfoMsg(nid, "already started");
            }
            continue;
        }

        SuccessOrExit(error = PrepareDtlsConfig(nid, conf));

        persistent_storage::border_router br;
        // TODO: resolve nid to border_router

        auto expr = aExpr;
        expr.push_back(br.agent.mAddr);
        expr.push_back(std::to_string(br.agent.mPort));
        ASSERT(expr.size() == 3); // 'start br_addr br_port'
        SuccessOrExit(error = CreateJob(entry->second, expr, nid));
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
                WarningMsg(nid, "not known to be started");
            }
            continue;
        }

        bool active = entry->second->IsActive();
        if (!active)
        {
            if (!aGroupAlias)
            {
                InfoMsg(nid, "already stopped");
            }
            continue;
        }

        SuccessOrExit(error = CreateJob(entry->second, aExpr, nid));
    }
exit:
    return error;
}

Error JobManager::PrepareDtlsConfig(const uint64_t aNid, Config &aConfig)
{
    Error                       error;
    std::string                 domainName;
    bool                        isCCM = false;
    sm::SecurityMaterials       dtlsConfig;
    Interpreter::RegistryStatus status;
    persistent_storage::network nwk;

    status = mInterpreter->mRegistry->get_network_by_xpan(aNid, nwk);
    VerifyOrExit(status == RegistryStatus::REG_SUCCESS, error = ERROR_IO_ERROR("network not found"));
    isCCM  = nwk.ccm > 0;
    status = mInterpreter->mRegistry->get_domain_name_by_xpan(aNid, domainName);
    if (status != RegistryStatus::REG_SUCCESS)
    {
        LOG_DEBUG(LOG_REGION_JOB_MANAGER, "{}: domain resolution failed with status={}",
                  persistent_storage::xpan_id(aNid).str(), status);
    }

    aConfig.mEnableCcm = isCCM;
    if (!domainName.empty())
    {
        aConfig.mDomainName = domainName;
        if (domainName != "DefaultDomain")
        {
            error = sm::GetDomainSM(domainName, dtlsConfig);
            if (ERROR_NONE != error)
            {
                WarningMsg(aNid, error.GetMessage());
                error = ERROR_NONE;
            }
        }
        else
        {
            error = sm::GetDefaultDomainSM(nwk.xpan.str(), isCCM, dtlsConfig);
            if (ERROR_NONE != error)
            {
                WarningMsg(aNid, error.GetMessage());
                error = ERROR_NONE;
            }
            if (!dtlsConfig.IsEmpty())
            {
                goto update;
            }
            error = sm::GetDefaultDomainSM(nwk.name, isCCM, dtlsConfig);
            if (ERROR_NONE != error)
            {
                WarningMsg(aNid, error.GetMessage());
                error = ERROR_NONE;
            }
        }
    }
    if (!dtlsConfig.IsEmpty())
    {
        goto update;
    }
    error = sm::GetNetworkSM(nwk.xpan.str(), isCCM, dtlsConfig);
    if (ERROR_NONE != error)
    {
        WarningMsg(aNid, error.GetMessage());
        error = ERROR_NONE;
    }
    if (!dtlsConfig.IsEmpty())
    {
        goto update;
    }
    error = sm::GetNetworkSM(nwk.name, isCCM, dtlsConfig);
    if (ERROR_NONE != error)
    {
        WarningMsg(aNid, error.GetMessage());
        error = ERROR_NONE;
    }
update:

#define UPDATE_IF_SET(name)            \
    if (dtlsConfig.m##name.size() > 0) \
    aConfig.m##name = dtlsConfig.m##name

    UPDATE_IF_SET(Certificate);
    UPDATE_IF_SET(PrivateKey);
    UPDATE_IF_SET(TrustAnchor);
    UPDATE_IF_SET(PSKc);

#undef UPDATE_IF_SET

    if (dtlsConfig.IsEmpty())
    {
        InfoMsg(aNid, "no updates to DTLS configuration, default configuration will be used");
    }
exit:
    return error;
}

Error JobManager::AppendImport(const uint64_t aNid, Interpreter::Expression &aExpr)
{
    Error error;
    (void)aNid;
    (void)aExpr;
    // TODO:
    // load JSON, else return 'bad format' error
    // find value by nid, else return 'not found' error
    // - value must be a JSON object, else return 'bad format' error
    // - the object must be of valid JSON syntax per command
    //   - commdataset
    //   - bbrdataset
    //   - opdataset
    //   else return 'bad format' error
    // serialize JSON object to string and append the one to aExp

    // if aNid is 0 : single command supposed (need more analysis)
    // - if plain dataset object and correct one, append as is
    // - else try to load by currently selected nid
    return error;
}

void JobManager::RunJobs()
{
    for (auto job : mJobPool)
    {
        ASSERT(job != NULL);
        job->Run();
    }
    WaitForJobs();
}

void JobManager::CancelCommand()
{
    for (auto job : mJobPool)
    {
        ASSERT(job != NULL);
        job->Cancel();
    }
    WaitForJobs();
}

void JobManager::WaitForJobs()
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

Error JobManager::GetSelectedCommissioner(CommissionerAppPtr &aCommissioner)
{
    Error                       error = ERROR_NONE;
    uint64_t                    nid   = 0;
    Interpreter::RegistryStatus status;

    status = mInterpreter->mRegistry->get_current_network_xpan(nid);
    VerifyOrExit(RegistryStatus::REG_SUCCESS == status, error = ERROR_IO_ERROR("selected network not found"));

    if (nid != 0)
    {
        auto entry = mCommissionerPool.find(nid);
        if (entry != mCommissionerPool.end())
        {
            aCommissioner = entry->second;
        }
        else
        {
            Config             conf         = mDefaultConf;
            CommissionerAppPtr commissioner = nullptr;

            SuccessOrExit(error = PrepareDtlsConfig(nid, conf));
            SuccessOrExit(error = CommissionerApp::Create(commissioner, conf));
            mCommissionerPool[nid] = commissioner;
            aCommissioner          = mCommissionerPool[nid];
        }
    }
    else
    {
        aCommissioner = mDefaultCommissioner;
    }
exit:
    return error;
}

} // namespace commissioner

} // namespace ot
