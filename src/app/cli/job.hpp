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
 *   The file defines command job.
 */

#ifndef OT_COMM_APP_CLI_JOB_HPP_
#define OT_COMM_APP_CLI_JOB_HPP_

#include <thread>
#include "app/cli/interpreter.hpp"
#include "app/commissioner_app.hpp"

namespace ot {

namespace commissioner {

class Job
{
public:
    Job(Interpreter &             aInterpreter,
        CommissionerAppPtr &      aCommApp,
        Interpreter::Expression   aExpr,
        Interpreter::JobEvaluator aEval,
        uint64_t                  aXpanId)
        : mInterpreter(aInterpreter)
        , mCommissioner(aCommApp)
        , mExpr(aExpr)
        , mEval(aEval)
        , mXpanId(aXpanId)
    {
    }
    ~Job() = default;

    void Run();
    void Wait();
    void Cancel();
    bool IsStopped() { return !mJobThread.joinable(); }

    Interpreter::Value GetValue() const { return mValue; }
    std::string        GetJsonString() const { return mJson; }

private:
    Interpreter &             mInterpreter;
    CommissionerAppPtr &      mCommissioner;
    Interpreter::Expression   mExpr;
    Interpreter::JobEvaluator mEval;
    Interpreter::Value        mValue;
    std::string               mJson;
    std::thread               mJobThread;
    uint64_t                  mXpanId;
};

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_APP_CLI_JOB_HPP_
