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
 *   This file defines test cases for logging.
 */

#include "common/logging.hpp"

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "commissioner/commissioner.hpp"

namespace ot {

namespace commissioner {

namespace {

class CountingLogger : public Logger
{
public:
    void Log(LogLevel, const std::string &, const std::string &) override { ++mCount; }

    std::atomic<int> mCount{0};
};

} // namespace

TEST(LoggingTest, ConcurrentInitLoggerAndLog)
{
    constexpr int kNumLogThreads = 4;
    constexpr int kNumIterations = 10000;

    auto                     logger = std::make_shared<CountingLogger>();
    std::vector<std::thread> logThreads;
    int                      count;

    for (int i = 0; i < kNumLogThreads; ++i)
    {
        logThreads.emplace_back([]() {
            for (int j = 0; j < kNumIterations; ++j)
            {
                Log(LogLevel::kInfo, "test", "message");
            }
        });
    }

    for (int i = 0; i < kNumIterations; ++i)
    {
        InitLogger((i % 2 == 0) ? logger : nullptr);
    }

    for (auto &thread : logThreads)
    {
        thread.join();
    }

    InitLogger(logger);
    count = logger->mCount;
    Log(LogLevel::kInfo, "test", "message");
    EXPECT_EQ(logger->mCount, count + 1);

    InitLogger(nullptr);
    Log(LogLevel::kInfo, "test", "message");
    EXPECT_EQ(logger->mCount, count + 1);
    EXPECT_EQ(logger.use_count(), 1);
}

} // namespace commissioner

} // namespace ot
