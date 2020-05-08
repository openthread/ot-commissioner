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

#include <catch2/catch.hpp>

#include "app/commissioner_app.hpp"
#include "common/utils.hpp"

namespace ot {

namespace commissioner {

TEST_CASE("pskd-validation", "[pskd]")
{
    Config config;
    config.mEnableCcm = false;
    config.mPSKc = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};

    auto commApp = CommissionerApp::Create(config);
    REQUIRE(commApp != nullptr);

    constexpr uint64_t eui64 = 0x0011223344556677;

    SECTION("A PSKd shorter than 6 characters should be rejected")
    {
        REQUIRE(commApp->EnableJoiner(JoinerType::kMeshCoP, eui64, "00001") == Error::kInvalidArgs);
    }

    SECTION("A PSKd longer than 32 characters should be rejected")
    {
        REQUIRE(commApp->EnableJoiner(JoinerType::kMeshCoP, eui64, "000000000000000000000000000000001") ==
                Error::kInvalidArgs);
    }

    SECTION("A PSKd including invalid characters should be rejected")
    {
        // Includes capital 'O' at the end.
        REQUIRE(commApp->EnableJoiner(JoinerType::kMeshCoP, eui64, "00000O") == Error::kInvalidArgs);

        // Includes capital 'I' at the end.
        REQUIRE(commApp->EnableJoiner(JoinerType::kMeshCoP, eui64, "11111I") == Error::kInvalidArgs);

        // Includes capital 'Q' at the end.
        REQUIRE(commApp->EnableJoiner(JoinerType::kMeshCoP, eui64, "99999Q") == Error::kInvalidArgs);

        // Includes capital 'Z' at the end.
        REQUIRE(commApp->EnableJoiner(JoinerType::kMeshCoP, eui64, "22222Z") == Error::kInvalidArgs);

        // Includes lower cases.
        REQUIRE(commApp->EnableJoiner(JoinerType::kMeshCoP, eui64, "abcedf") == Error::kInvalidArgs);

        // Includes lowercase alphanumeric characters.
        REQUIRE(commApp->EnableJoiner(JoinerType::kMeshCoP, eui64, "abcedf") == Error::kInvalidArgs);

        // Includes non-alphanumeric characters.
        REQUIRE(commApp->EnableJoiner(JoinerType::kMeshCoP, eui64, "+-#$%@") == Error::kInvalidArgs);
    }

    SECTION("A compliant PSKd should be accepted and kInvalidState error is expected")
    {
        REQUIRE(commApp->EnableJoiner(JoinerType::kMeshCoP, eui64, "PSKD01") == Error::kInvalidState);
    }
}

} // namespace commissioner

} // namespace ot
