/**
 * TODO copyright
 */
/**
 * @file JobManager mock definitions
 */

#ifndef OT_COMM_APP_CLI_JOB_MANAGER_MOCK_HPP_
#define OT_COMM_APP_CLI_JOB_MANAGER_MOCK_HPP_

#include "gmock/gmock.h"

#include "app/cli/job_manager.hpp"

namespace ot {

namespace commissioner {

class JobManagerMock : public JobManager
{
public:
    JobManagerMock()          = default;
    virtual ~JobManagerMock() = default;

    MOCK_METHOD(Error, Init, (const Config &, Interpreter &));
    MOCK_METHOD(Error, PrepareJobs, (const Interpreter::Expression &, const NidArray &, bool));
    MOCK_METHOD(void, RunJobs, ());
    MOCK_METHOD(void, CancelCommand, ());
    MOCK_METHOD(void, Wait, ());
    MOCK_METHOD(void, CleanupJobs, ());
    MOCK_METHOD(void, SetImportFile, (const std::string &));
    MOCK_METHOD(void, StopCommissionerPool, ());
    MOCK_METHOD(Error, GetSelectedCommissioner, (CommissionerAppPtr &));
    MOCK_METHOD(Error, AppendImport, (const uint64_t, Interpreter::Expression &));
};

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_APP_CLI_JOB_MANAGER_MOCK_HPP_
