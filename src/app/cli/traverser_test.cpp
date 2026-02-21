/*
 *    Copyright (c) 2026, The OpenThread Commissioner Authors.
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
 *   The file implements tests for CLI network traverser.
 */

#include <gmock/gmock-matchers.h>
#include <gmock/gmock-spec-builders.h>
#define private public
#include "app/cli/interpreter.hpp"
#undef private

#include "app/cli/traverser.hpp"

#include <chrono>
#include <memory>
#include <string>

#include "app/commissioner_app_mock.hpp"
#include "commissioner/error.hpp"
#include "gtest/gtest.h"

using namespace ot::commissioner;
using testing::_;
using testing::Return;

TEST(TraverserTest, TestGlobalTimeout)
{
    std::shared_ptr<CommissionerAppMock> mockCommissioner = std::make_shared<CommissionerAppMock>();
    Interpreter                          interpreter;
    Interpreter::Expression              expr;

    // Initialize mCancelCommand to false as it is uninitialized by default
    interpreter.mCancelCommand = false;

    // Mock TraverseNetwork to return success but do nothing (simulate hang/long operation)
    EXPECT_CALL(*mockCommissioner, TraverseNetwork(_)).WillOnce(Return(Error()));

    // Expect CancelRequests to be called when timeout occurs
    EXPECT_CALL(*mockCommissioner, CancelRequests()).Times(1);

    // Run with a short timeout
    CommissionerAppPtr app    = mockCommissioner;
    auto               start  = std::chrono::steady_clock::now();
    std::string        result = Traverser::ProcessTraverseNetworkJob(&interpreter, app, expr, "",
                                                                     std::chrono::milliseconds(100)); // 100ms timeout
    auto               end    = std::chrono::steady_clock::now();

    // Verify results
    EXPECT_EQ(result, "Traversal timed out");

    // Verify duration was roughly the timeout duration
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_GE(duration.count(), 100);
}
