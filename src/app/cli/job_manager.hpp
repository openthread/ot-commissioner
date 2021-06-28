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
#include "app/ps/registry_entries.hpp"

namespace ot {

namespace commissioner {

using CommissionerAppPtr = std::shared_ptr<CommissionerApp>;
using RegistryStatus     = ot::commissioner::persistent_storage::Registry::Status;
using ot::commissioner::XpanId;
using ot::commissioner::persistent_storage::BorderRouter;

class Job;

class JobManager
{
public:
    explicit JobManager(Interpreter &aInterpreter);
    ~JobManager() = default;

    Error Init(const Config &aConf);
    Error PrepareJobs(const Interpreter::Expression &aExpr, const XpanIdArray &aNids, bool aGroupAlias);
    /**
     * Run all prepared jobs.
     *
     * The call is blocking due to WaitForJobs() done internally. Job
     * resultant values remain available until CleanupJobs() is done.
     */
    void RunJobs();
    void CancelCommand();
    void CleanupJobs();
    void SetImportFile(const std::string &importFile);
    void StopCommissionerPool();
    /**
     * Collects values from job pool and aggregates those into a
     * single JSON object ready for output or export.
     *
     * Resultant JSON object is a map where key is a job's XPAN ID and
     * value is an elementary JSON (dataset object, boolean or string)
     */
    Interpreter::Value CollectJobsValue();
    /**
     * Returns a commissioner instance for the currently selected
     * network.
     *
     * - if instance not found, it is created and added to the pool
     * - if no network selected, default commissioner is returned
     */
    Error GetSelectedCommissioner(CommissionerAppPtr &aCommissioner);
    /**
     * Asserts if the execution context is blank and ready for the next run.
     */
    bool IsClean();
    /**
     * Apply new PSKc bytes and re-create @ref
     * JobManager::mDefaultCommissioner instance
     */
    Error UpdateDefaultConfigPSKc(const ByteArray &aPSKc);
    /**
     * Reads PSKc to string for the sake of `config get pskc' command
     */
    std::string GetDefaultConfigPSKc() const;
    /**
     * Appends to aExpr an imported argument loaded from mImportFile.
     *
     * It is expected mImportFile contains a JSON object where the
     * imported part is a value of a map entry under aNid key.
     */
    Error AppendImport(XpanId aXpanId, Interpreter::Expression &aExpr);
    /**
     * Make a well-thought choice from border routers belonging to a
     * given network identified by aNid XPAN ID.
     *
     * If CCM mode found enabled, an active Primary BBR is preferred.
     * Otherwiese, any secondary active BBR is preferred.
     *
     * If in non-CCM mode, a BR with most highly avaliable and Thread-active
     * interface becomes the choice.
     */
    Error MakeBorderRouterChoice(const XpanId aNid, BorderRouter &br);

private:
    friend class JobManagerTestSuite;

    using CommissionerPool = std::map<XpanId, CommissionerAppPtr>;
    using JobPool          = std::vector<Job *>;
    /**
     * Wait for all job threads to join.
     */
    void  WaitForJobs();
    Error PrepareStartJobs(const Interpreter::Expression &aExpr, const XpanIdArray &aNids, bool aGroupAlias);
    Error PrepareStopJobs(const Interpreter::Expression &aExpr, const XpanIdArray &aNids, bool aGroupAlias);
    /**
     * Updates DTLS parts of Config for the given network.
     *
     * If network belongs to a domain other than DefaultDomain, the
     * appropriate lookup for credentials is done under
     * $THREAD_SM_ROOT/dom/$did/ folder.
     *
     * If network belongs to DefaultDomain, the lookup is sequentially done in:
     * - $THREAD_SM_ROOT/dom/DefaultDomain/$nid/
     * - $THREAD_SM_ROOT/dom/DefaultDomain/$nname/
     * - $THREAD_SM_ROOT/nwk/$nid/
     * - $THREAD_SM_ROOT/nwk/$nname/
     *
     * If network does not belong to any domain, the lookup is done in:
     * - $THREAD_SM_ROOT/nwk/$nid/
     * - $THREAD_SM_ROOT/nwk/$nname/
     *
     * @note: SM administration personnel is free to give a name to a
     *        netowrk folder by network id (XPAN ID) or network
     *        name. However, first search is to be done by network id,
     *        and if it was successful, i.e. at least one file was
     *        found not empty, the search is stopped.
     */
    Error PrepareDtlsConfig(const XpanId aNid, Config &aConfig);
    Error CreateJob(CommissionerAppPtr &aCommissioner, const Interpreter::Expression &aExpr, XpanId aXpanId);

    void ErrorMsg(XpanId aNid, std::string aMessage);
    void WarningMsg(XpanId aNid, std::string aMessage);
    void InfoMsg(XpanId aNid, std::string aMessage);

    JobPool            mJobPool;
    CommissionerPool   mCommissionerPool;
    Config             mDefaultConf;
    std::string        mImportFile;
    Interpreter &      mInterpreter;
    CommissionerAppPtr mDefaultCommissioner = nullptr;
};

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_APP_CLI_JOB_MANAGER_HPP_
