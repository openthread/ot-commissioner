/*
 *    Copyright (c) 2020, The OpenThread Authors.
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
 *   This file defines test cases of CommissionerApp.
 *
 */

#include "app/commissioner_app.hpp"

#include <gtest/gtest.h>

#include "common/utils.hpp"

namespace ot {

namespace commissioner {

TEST(PskdTest, Pskdvalidation_TooShortPskdShouldBeRejected)
{
    Config config;
    config.mEnableCcm = false;
    config.mPSKc = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};

    std::shared_ptr<CommissionerApp> commApp;
    EXPECT_EQ(CommissionerAppCreate(commApp, config), ErrorCode::kNone);

    constexpr uint64_t eui64 = 0x0011223344556677;

    EXPECT_EQ(commApp->EnableJoiner(JoinerType::kMeshCoP, eui64, "00001"), ErrorCode::kInvalidArgs);
}

TEST(PskdTest, Pskdvalidation_TooLongPskdShouldBeRejected)
{
    Config config;
    config.mEnableCcm = false;
    config.mPSKc = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};

    std::shared_ptr<CommissionerApp> commApp;
    EXPECT_EQ(CommissionerAppCreate(commApp, config), ErrorCode::kNone);

    constexpr uint64_t eui64 = 0x0011223344556677;

    EXPECT_EQ(commApp->EnableJoiner(JoinerType::kMeshCoP, eui64, "000000000000000000000000000000001").GetCode(),
              ErrorCode::kInvalidArgs);
}

TEST(PskdTest, Pskdvalidation_PskdWithInvalidCharactersShouldBeRejected)
{
    Config config;
    config.mEnableCcm = false;
    config.mPSKc = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};

    std::shared_ptr<CommissionerApp> commApp;
    EXPECT_EQ(CommissionerAppCreate(commApp, config), ErrorCode::kNone);

    constexpr uint64_t eui64 = 0x0011223344556677;

    // Includes capital 'O' at the end.
    EXPECT_EQ(commApp->EnableJoiner(JoinerType::kMeshCoP, eui64, "00000O"), ErrorCode::kInvalidArgs);

    // Includes capital 'I' at the end.
    EXPECT_EQ(commApp->EnableJoiner(JoinerType::kMeshCoP, eui64, "11111I"), ErrorCode::kInvalidArgs);

    // Includes capital 'Q' at the end.
    EXPECT_EQ(commApp->EnableJoiner(JoinerType::kMeshCoP, eui64, "99999Q"), ErrorCode::kInvalidArgs);

    // Includes capital 'Z' at the end.
    EXPECT_EQ(commApp->EnableJoiner(JoinerType::kMeshCoP, eui64, "22222Z"), ErrorCode::kInvalidArgs);

    // Includes lowercase alphanumeric characters.
    EXPECT_EQ(commApp->EnableJoiner(JoinerType::kMeshCoP, eui64, "abcedf"), ErrorCode::kInvalidArgs);

    // Includes non-alphanumeric characters.
    EXPECT_EQ(commApp->EnableJoiner(JoinerType::kMeshCoP, eui64, "+-#$%@"), ErrorCode::kInvalidArgs);
}

TEST(PskdTest, Pskdvalidation_GoodPskdShouldBeAccepted)
{
    Config config;
    config.mEnableCcm = false;
    config.mPSKc = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};

    std::shared_ptr<CommissionerApp> commApp;
    EXPECT_EQ(CommissionerAppCreate(commApp, config), ErrorCode::kNone);

    constexpr uint64_t eui64 = 0x0011223344556677;

    EXPECT_NE(commApp->EnableJoiner(JoinerType::kMeshCoP, eui64, "PSKD01"), ErrorCode::kInvalidArgs);
}

} // namespace commissioner

} // namespace ot
