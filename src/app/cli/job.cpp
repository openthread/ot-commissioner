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
 *   The file implements command job.
 */

#include "app/cli/job.hpp"
#include "app/ps/registry.hpp"
#include "common/logging.hpp"
#include "common/utils.hpp"

#include <sstream>

namespace ot {

namespace commissioner {

void Job::Run()
{
    ASSERT(!mJobThread.joinable());
    mJobThread = std::thread([this] { mValue = mEval(&mInterpreter, mCommissioner, mExpr); });
}

void Job::Wait()
{
    ASSERT(mJobThread.joinable());
    mJobThread.join();
    if (!mValue.HasNoError())
    {
        LOG_DEBUG(LOG_REGION_JOB, "{}: job '{}' failed: {}", XpanId(mXpanId).str(), GetCommandString(),
                  mValue.ToString());
    }
}

void Job::Cancel()
{
    mCommissioner->CancelRequests();
}

std::string Job::GetCommandString()
{
    std::ostringstream command;
    std::string        out;

    for_each(mExpr.begin(), mExpr.end() - 1, [&command](std::string &item) { command << item << " "; });
    out = command.str();
    out.pop_back(); // get rid of trailing space
    return out;
}

Job::Job(Interpreter              &aInterpreter,
         CommissionerAppPtr       &aCommApp,
         Interpreter::Expression   aExpr,
         Interpreter::JobEvaluator aEval,
         XpanId                    aXpanId)
    : mInterpreter(aInterpreter)
    , mCommissioner(aCommApp)
    , mExpr(aExpr)
    , mEval(aEval)
    , mXpanId(aXpanId.mValue)
{
}

bool Job::IsStopped()
{
    return !mJobThread.joinable();
}

uint64_t Job::GetXpanId() const
{
    return mXpanId;
}

Interpreter::Value Job::GetValue() const
{
    return mValue;
}

} // namespace commissioner

} // namespace ot
