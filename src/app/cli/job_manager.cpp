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

#include <exception>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "app/border_agent.hpp"
#include "app/cli/console.hpp"
#include "app/cli/interpreter.hpp"
#include "app/cli/job.hpp"
#include "app/cli/job_manager.hpp"
#include "app/cli/security_materials.hpp"
#include "app/commissioner_app.hpp"
#include "app/json.hpp"
#include "app/ps/registry_entries.hpp"
#include "commissioner/commissioner.hpp"
#include "commissioner/defines.hpp"
#include "commissioner/error.hpp"
#include "commissioner/network_data.hpp"
#include "common/error_macros.hpp"
#include "common/logging.hpp"
#include "common/utils.hpp"

#define SYNTAX_IMPORT_UNSUPPORTED "import usupported"

namespace ot {

namespace commissioner {

using Json = nlohmann::json;
using persistent_storage::Network;
using security_material::SecurityMaterials;
using BRArray = std::vector<persistent_storage::BorderRouter>;

Error JobManager::Init(const Config &aConf)
{
    Error error;

    mDefaultConf = aConf;
    SuccessOrExit(error = security_material::Init(aConf));
    SuccessOrExit(error = CommissionerAppCreate(mDefaultCommissioner, aConf));
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
    CommissionerAppCreate(mDefaultCommissioner, mDefaultConf).IgnoreError();

exit:
    return error;
}

void JobManager::UpdateDefaultConfigCommissionerToken()
{
    mDefaultConf.mCommissionerToken = mDefaultCommissioner->GetToken();
}

std::string JobManager::GetDefaultConfigPSKc() const
{
    return utils::Hex(mDefaultConf.mPSKc);
}

void JobManager::CleanupJobs()
{
    for (auto job : mJobPool)
    {
        ASSERT(job == nullptr || job->IsStopped());
        delete job;
    }
    mJobPool.clear();
    mImportFile.clear();
}

void JobManager::SetImportFile(const std::string &importFile)
{
    mImportFile = importFile;
}

Error JobManager::CreateJob(CommissionerAppPtr &aCommissioner, const Interpreter::Expression &aExpr, XpanId aXpanId)
{
    Interpreter::JobEvaluator eval;
    auto                      mapItem = Interpreter::mJobEvaluatorMap.find(utils::ToLower(aExpr[0]));

    if (mapItem == Interpreter::mJobEvaluatorMap.end())
    {
        return ERROR_INVALID_COMMAND("{} not eligible for job", aExpr[0]);
    }
    eval     = mapItem->second;
    Job *job = new Job(mInterpreter, aCommissioner, aExpr, eval, aXpanId);
    mJobPool.push_back(job);

    return ERROR_NONE;
}

Error JobManager::PrepareJobs(const Interpreter::Expression &aExpr, const XpanIdArray &aNids, bool aGroupAlias)
{
    Error error;

    if (utils::ToLower(aExpr[0]) == "start")
    {
        ExitNow(error = PrepareStartJobs(aExpr, aNids, aGroupAlias));
    }
    else if (utils::ToLower(aExpr[0]) == "stop")
    {
        ExitNow(error = PrepareStopJobs(aExpr, aNids, aGroupAlias));
    }

    for (const auto &nid : aNids)
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

        bool isInactiveAllowed = mInterpreter.IsInactiveCommissionerAllowed(aExpr);
        if (!isInactiveAllowed)
        {
            bool active = entry->second->IsActive();
            if (!active)
            {
                if (!aGroupAlias)
                {
                    WarningMsg(nid, "not started");
                }
                continue;
            }
        }

        Interpreter::Expression jobExpr = aExpr;

        if (!mImportFile.empty())
        {
            Error importError = AppendImport(nid, jobExpr);
            if (importError != ERROR_NONE)
            {
                ErrorMsg(nid, importError.GetMessage());
                continue;
            }
        }

        SuccessOrExit(error = CreateJob(entry->second, jobExpr, nid));
    }

exit:
    return error;
}

Error JobManager::PrepareStartJobs(const Interpreter::Expression &aExpr, const XpanIdArray &aNids, bool aGroupAlias)
{
    Error error = ERROR_NONE;

    ASSERT(utils::ToLower(aExpr[0]) == "start");
    /*
     * Coming here is a result of using multi-network syntax.
     * Therefore, no extra arguments to be used, otherwise it
     * is multi-network syntax violation.
     */
    ASSERT(aExpr.size() == 1);

    for (const auto &nid : aNids)
    {
        BorderRouter br;
        Config       conf = mDefaultConf;

        error = PrepareDtlsConfig(nid, conf);
        if (error != ERROR_NONE)
        {
            ErrorMsg(nid, error.GetMessage());
            error = ERROR_NONE;
            continue;
        }
        {
            auto entry = mCommissionerPool.find(nid);
            if (entry == mCommissionerPool.end())
            {
                CommissionerAppPtr commissioner;

                SuccessOrExit(error = CommissionerAppCreate(commissioner, conf));
                mCommissionerPool[nid] = commissioner;
                entry                  = mCommissionerPool.find(nid);
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

            SuccessOrExit(error = MakeBorderRouterChoice(nid, br));
            auto expr = aExpr;
            expr.push_back(br.mAgent.mAddr);
            expr.push_back(std::to_string(br.mAgent.mPort));
            ASSERT(expr.size() == 3); // 'start br_addr br_port'
            SuccessOrExit(error = CreateJob(entry->second, expr, nid));
        }
    }
exit:
    return error;
}

Error JobManager::PrepareStopJobs(const Interpreter::Expression &aExpr, const XpanIdArray &aNids, bool aGroupAlias)
{
    Error error = ERROR_NONE;

    ASSERT(utils::ToLower(aExpr[0]) == "stop");

    for (const auto &nid : aNids)
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

Error JobManager::PrepareDtlsConfig(const XpanId aNid, Config &aConfig)
{
    Error             error;
    std::string       domainName;
    bool              isCCM = false;
    SecurityMaterials dtlsConfig;
    Network           nwk;
    BRArray           brs;
    bool              needCert = false;
    bool              needPSKc = false;
    RegistryStatus    status   = mInterpreter.mRegistry->GetBorderRoutersInNetwork(aNid, brs);

    VerifyOrExit(status == RegistryStatus::kSuccess,
                 error = ERROR_REGISTRY_ERROR("br lookup failed with status={}", static_cast<int>(status)));

    // review the network BRs connection modes that may require
    // certain supported type of credentials
    for (const auto &br : brs)
    {
        if (br.mAgent.mState.mConnectionMode == BorderAgent::State::ConnectionMode::kVendorSpecific ||
            br.mAgent.mState.mConnectionMode == BorderAgent::State::ConnectionMode::kX509Connection)
        {
            needCert = true;
        }
        if (br.mAgent.mState.mConnectionMode == BorderAgent::State::ConnectionMode::kVendorSpecific ||
            br.mAgent.mState.mConnectionMode == BorderAgent::State::ConnectionMode::kPSKcConnection)
        {
            needPSKc = true;
        }
    }

    /// @note: If vendor specific connection mode encountered, PSKc as
    ///        well as X.509 cert will be prepared to be available to
    ///        choose from; in case of another security material
    ///        type(s) required by BR vendor, please update
    ///        JobManager::PrepareDtlsConfig() method appropriately.

    status = mInterpreter.mRegistry->GetNetworkByXpan(aNid, nwk);
    VerifyOrExit(status == RegistryStatus::kSuccess,
                 error = ERROR_NOT_FOUND("network not found by XPAN '{}'", aNid.str()));
    isCCM  = nwk.mCcm > 0;
    status = mInterpreter.mRegistry->GetDomainNameByXpan(aNid, domainName);
    if (status != RegistryStatus::kSuccess)
    {
        LOG_DEBUG(LOG_REGION_JOB_MANAGER, "{}: domain resolution failed with status={}", XpanId(aNid).str(),
                  static_cast<int>(status));
    }

    aConfig.mEnableCcm = isCCM;
    if (!domainName.empty() && needCert)
    {
        aConfig.mDomainName = domainName;
        if (domainName != "DefaultDomain")
        {
            error = security_material::GetDomainSM(domainName, dtlsConfig);
            if (ERROR_NONE != error)
            {
                LOG_STR(DEBUG, LOG_REGION_JOB_MANAGER, error.GetMessage());
                if (ErrorCode::kNotFound != error.GetCode())
                {
                    WarningMsg(aNid, error.GetMessage());
                }
                error = ERROR_NONE;
            }
            if (!dtlsConfig.IsEmpty(isCCM))
            {
                if (needPSKc)
                {
                    /// @note: Please also note for CCM networks that only
                    ///        X.509 based credentials are valid for
                    ///        loading. Therefore, any other connection
                    ///        mode is considered a wrong configuration of
                    ///        border router, and respectively ignored.
                    LOG_DEBUG(LOG_REGION_JOB_MANAGER, "loading PSKc ignored for CCM network [{}:'{}']", aNid.str(),
                              nwk.mName);
                }
                goto update;
            }
        }
        else
        {
            // a network of DefaultDomain is a non-CCM one; we will
            // look up in a folder under nwk later
            error = ERROR_NONE;
        }
    }

    error = security_material::GetNetworkSM(nwk.mXpan.str(), needCert, needPSKc, dtlsConfig);
    if (ERROR_NONE != error)
    {
        LOG_STR(DEBUG, LOG_REGION_JOB_MANAGER, error.GetMessage());
        if (ErrorCode::kNotFound != error.GetCode())
        {
            WarningMsg(aNid, error.GetMessage());
        }
        // else we try a chance with nwk.mName later
        error = ERROR_NONE;
    }
    if (dtlsConfig.IsAnyFound(needCert, needPSKc))
    {
        goto update;
    }
    error = security_material::GetNetworkSM(nwk.mName, needCert, needPSKc, dtlsConfig);
    if (ERROR_NONE != error)
    {
        LOG_STR(DEBUG, LOG_REGION_JOB_MANAGER, error.GetMessage());
        if (ErrorCode::kNotFound != error.GetCode())
        {
            WarningMsg(aNid, error.GetMessage());
        }
        // nothing found, so continue with default values
        error = ERROR_NONE;
    }
update:

#define UPDATE_IF_SET(name)            \
    if (dtlsConfig.m##name.size() > 0) \
    aConfig.m##name = dtlsConfig.m##name

    UPDATE_IF_SET(Certificate);
    UPDATE_IF_SET(PrivateKey);
    UPDATE_IF_SET(TrustAnchor);
    UPDATE_IF_SET(CommissionerToken);
    UPDATE_IF_SET(PSKc);

#undef UPDATE_IF_SET

    if (dtlsConfig.IsEmpty(isCCM))
    {
        InfoMsg(aNid, "no updates to DTLS configuration, default configuration will be used");
    }

#define UPDATE_IF_SET(name)         \
    if (aConfig.m##name.size() > 0) \
    dtlsConfig.m##name = aConfig.m##name

    UPDATE_IF_SET(Certificate);
    UPDATE_IF_SET(PrivateKey);
    UPDATE_IF_SET(TrustAnchor);
    UPDATE_IF_SET(CommissionerToken);
    UPDATE_IF_SET(PSKc);

#undef UPDATE_IF_SET
    if (dtlsConfig.IsIncomplete(needCert, needPSKc, isCCM))
    {
        error = ERROR_SECURITY("incomplete DTLS configuration for the network [{}:'{}']", aNid.str(), nwk.mName);
    }
exit:
    return error;
}

Error JobManager::MakeBorderRouterChoice(const XpanId aNid, BorderRouter &br)
{
    Error          error;
    BRArray        brs;
    BRArray        choice;
    Network        nwk;
    RegistryStatus status = mInterpreter.mRegistry->GetBorderRoutersInNetwork(aNid, brs);

    VerifyOrExit(status == RegistryStatus::kSuccess,
                 error = ERROR_REGISTRY_ERROR("br lookup failed with status={}", static_cast<int>(status)));
    if (brs.size() == 1)
    {
        // looks like not much of a choice
        br = brs.front();
        ExitNow();
    }
    status = mInterpreter.mRegistry->GetNetworkByXpan(aNid, nwk);
    VerifyOrExit(status == RegistryStatus::kSuccess,
                 error = ERROR_NOT_FOUND("network not found by XPAN '{}'", aNid.str()));
    if (nwk.mCcm > 0) // Dealing with domain network
    {
        // - try to find active and connectable Primary BBR
        for (const auto &item : brs)
        {
            if (item.mAgent.mState.mBbrIsPrimary &&
                item.mAgent.mState.mConnectionMode > BorderAgent::State::ConnectionMode::kNotAllowed)
            {
                if (item.mAgent.mState.mBbrIsActive &&
                    item.mAgent.mState.mThreadIfStatus > BorderAgent::State::ThreadInterfaceStatus::kNotInitialized)
                {
                    /// @todo: ideally here we would make sure the
                    ///        Border Router's connection mode @ref
                    ///        BorderAgent::State::ConnectionMode::kX509Connection
                    ///        is specified, otherwise the announced
                    ///        BBR appears inconsistent with Thread
                    ///        spec. Although nothing we could do at
                    ///        runtime here, a warning message might
                    ///        be useful as an indication of a
                    ///        potential issue
                    br = item;
                    ExitNow();
                }
            }
        }
        // - go on with other active and connectable BBRs
        for (const auto &item : brs)
        {
            if (item.mAgent.mState.mBbrIsActive &&
                item.mAgent.mState.mConnectionMode > BorderAgent::State::ConnectionMode::kNotAllowed)
            {
                choice.push_back(item);
            }
        }
    }
    else // Dealing with standalone networks
    {
        // go on with connectable BRs
        for (const auto &item : brs)
        {
            if (item.mAgent.mState.mConnectionMode > BorderAgent::State::ConnectionMode::kNotAllowed)
            {
                choice.push_back(item);
            }
        }
    }

    // Below a final triage is done

    // - prefer br with high-availability
    for (const auto &item : choice)
    {
        if (item.mAgent.mState.mThreadIfStatus > BorderAgent::State::ThreadInterfaceStatus::kNotActive &&
            item.mAgent.mState.mAvailability > BorderAgent::State::Availability::kInfrequent)
        {
            br = item;
            ExitNow();
        }
    }
    // - prefer br with Thread Interface actively participating in communication
    for (const auto &item : choice)
    {
        if (item.mAgent.mState.mThreadIfStatus > BorderAgent::State::ThreadInterfaceStatus::kNotActive)
        {
            br = item;
            ExitNow();
        }
    }
    // - try to find br with Thread Interface at least enabled
    for (const auto &item : choice)
    {
        if (item.mAgent.mState.mThreadIfStatus > BorderAgent::State::ThreadInterfaceStatus::kNotInitialized)
        {
            br = item;
            ExitNow();
        }
    }
    error = ERROR_NOT_FOUND("no active BR found");
exit:
    return error;
}

Error JobManager::AppendImport(XpanId aXpanId, Interpreter::Expression &aExpr)
{
    Error       error;
    std::string jsonStr;
    Json        jsonSrc;
    Json        json;

    SuccessOrExit(error = JsonFromFile(jsonStr, mImportFile));
    jsonSrc = Json::parse(jsonStr);
    if (aXpanId == XpanId()) // must be single command
    {
        json = jsonSrc;
    }
    else if (jsonSrc.count(aXpanId.str()) > 0)
    {
        json = jsonSrc[aXpanId.str()];
    }
    jsonStr = json.dump(JSON_INDENT_DEFAULT);
    if (utils::ToLower(aExpr[0]) == "opdataset")
    {
        VerifyOrExit(utils::ToLower(aExpr[1]) == "set" && aExpr.size() == 3,
                     error = ERROR_INVALID_ARGS(SYNTAX_IMPORT_UNSUPPORTED));
        if (utils::ToLower(aExpr[2]) == "active")
        {
            ActiveOperationalDataset dataset;
            error = ActiveDatasetFromJson(dataset, jsonStr);
            if (error != ERROR_NONE)
            {
                jsonStr = jsonSrc.dump(JSON_INDENT_DEFAULT);
                SuccessOrExit(error = ActiveDatasetFromJson(dataset, jsonStr));
            }
            // TODO: try importing wrong format
        }
        else if (utils::ToLower(aExpr[2]) == "pending")
        {
            PendingOperationalDataset dataset;
            error = PendingDatasetFromJson(dataset, jsonStr);
            if (error != ERROR_NONE)
            {
                jsonStr = jsonSrc.dump(JSON_INDENT_DEFAULT);
                SuccessOrExit(error = PendingDatasetFromJson(dataset, jsonStr));
            }
            // TODO: try importing wrong format
        }
        else
        {
            ExitNow(error = ERROR_INVALID_ARGS(SYNTAX_IMPORT_UNSUPPORTED));
        }
    }
    else if (utils::ToLower(aExpr[0]) == "bbrdataset")
    {
        BbrDataset dataset;
        VerifyOrExit(aExpr.size() == 2 && utils::ToLower(aExpr[1]) == "set",
                     error = ERROR_INVALID_ARGS(SYNTAX_IMPORT_UNSUPPORTED));
        error = BbrDatasetFromJson(dataset, jsonStr);
        if (error != ERROR_NONE)
        {
            jsonStr = jsonSrc.dump(JSON_INDENT_DEFAULT);
            SuccessOrExit(error = BbrDatasetFromJson(dataset, jsonStr));
        }
        // TODO: try importing wrong format
    }
    else if (utils::ToLower(aExpr[0]) == "commdataset")
    {
        CommissionerDataset dataset;
        VerifyOrExit(aExpr.size() == 2 && utils::ToLower(aExpr[1]) == "set",
                     error = ERROR_INVALID_ARGS(SYNTAX_IMPORT_UNSUPPORTED));
        error = CommissionerDatasetFromJson(dataset, jsonStr);
        if (error != ERROR_NONE)
        {
            jsonStr = jsonSrc.dump(JSON_INDENT_DEFAULT);
            SuccessOrExit(error = CommissionerDatasetFromJson(dataset, jsonStr));
        }
        // TODO: try importing wrong format
    }
    else
    {
        ASSERT(false); // never to reach here
    }
    aExpr.push_back(jsonStr);
exit:
    return error;
}

void JobManager::RunJobs()
{
    for (auto job : mJobPool)
    {
        ASSERT(job != nullptr);
        job->Run();
    }
    WaitForJobs();
}

void JobManager::CancelCommand()
{
    CommissionerAppPtr commissioner = nullptr;
    Error              error        = ERROR_NONE;

    for (auto job : mJobPool)
    {
        ASSERT(job != nullptr);
        job->Cancel();
    }
    WaitForJobs();

    error = GetSelectedCommissioner(commissioner);
    if (error == ERROR_NONE)
    {
        if (commissioner->IsActive())
        {
            commissioner->CancelRequests();
        }
        else
        {
            commissioner->Stop();
        }
    }
}

void JobManager::WaitForJobs()
{
    for (auto job : mJobPool)
    {
        ASSERT(job != nullptr);
        job->Wait();
    }
}

Interpreter::Value JobManager::CollectJobsValue()
{
    Interpreter::Value value;
    nlohmann::json     json;
    XpanId             xpan;

    for (const auto &job : mJobPool)
    {
        ASSERT(job->IsStopped());
        if (job->GetValue().HasNoError())
        {
            xpan = XpanId{job->GetXpanId()};
            try
            {
                std::string valueStr = job->GetValue().ToString();
                if (valueStr.empty())
                {
                    // this may occur with non-dataset commands
                    // like 'start', 'stop', etc.
                    valueStr = "true";
                    // please note, this is where job-based execution
                    // is different from single command run when
                    // nothing but [done] is printed; we need to see a
                    // distinguished result per network
                }
                json[xpan.str()] = nlohmann::json::parse(valueStr);
            } catch (std::exception &e)
            {
                ErrorMsg(xpan, e.what());
            }
        }
        else // produce error messages immediately before printing value
        {
            ErrorMsg(XpanId{job->GetXpanId()}, job->GetValue().ToString());
        }
    }
    value = json.dump(JSON_INDENT_DEFAULT);
    return value;
}

void JobManager::StopCommissionerPool()
{
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
    Error          error = ERROR_NONE;
    XpanId         nid;
    RegistryStatus status;

    status = mInterpreter.mRegistry->GetCurrentNetworkXpan(nid);
    VerifyOrExit(RegistryStatus::kSuccess == status, error = ERROR_REGISTRY_ERROR("getting selected network failed"));

    if (nid.mValue != XpanId::kEmptyXpanId)
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
            SuccessOrExit(error = CommissionerAppCreate(commissioner, conf));
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

JobManager::JobManager(Interpreter &aInterpreter)
    : mInterpreter(aInterpreter)
{
}

bool JobManager::IsClean()
{
    return mJobPool.size() == 0 && mImportFile.size() == 0;
}

void JobManager::ErrorMsg(XpanId aNid, std::string aMessage)
{
    mInterpreter.PrintNetworkMessage(aNid.mValue, aMessage, Console::Color::kRed);
}

void JobManager::WarningMsg(XpanId aNid, std::string aMessage)
{
    mInterpreter.PrintNetworkMessage(aNid.mValue, aMessage, Console::Color::kMagenta);
}

void JobManager::InfoMsg(XpanId aNid, std::string aMessage)
{
    mInterpreter.PrintNetworkMessage(aNid.mValue, aMessage, Console::Color::kDefault);
}

} // namespace commissioner

} // namespace ot
