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
 *   This file defines test cases of CommissionerSafe.
 *
 */

#include <stdint.h>
#include <tuple>
#include <unistd.h>

#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "commissioner/commissioner.hpp"
#include "commissioner/defines.hpp"
#include "commissioner/error.hpp"
#include "common/address.hpp"
#include "library/commissioner_safe.hpp"
#include "library/joiner_session.hpp"

using testing::ContainerEq;

namespace ot {

namespace commissioner {

TEST(CommissionerSafeTest, StopImmediatelyAfterStarting)
{
    CommissionerHandler dummyHandler;

    Config config;
    config.mEnableCcm = false;
    config.mPSKc = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};

    // This creates an CommissionerSafe instance.
    auto commissioner = Commissioner::Create(dummyHandler);
    EXPECT_NE(commissioner, nullptr);
    EXPECT_EQ(commissioner->Init(config), ErrorCode::kNone);
}

TEST(CommissionerSafeTestProxyMode, ShouldBeAbleToReceiveJoinerMessage)
{
    class MockHandler : public CommissionerHandler
    {
    public:
        MOCK_METHOD(void,
                    OnJoinerMessage,
                    (const ByteArray &aJoinerId, uint16_t aPort, const ByteArray &aPayload),
                    (override));
    };

    MockHandler mockHandler;

    Config config;

    config.mEnableCcm = false;
    config.mProxyMode = true;
    config.mPSKc      = ByteArray(8);

    // This creates an CommissionerSafe instance.
    auto commissioner = Commissioner::Create(mockHandler);

    EXPECT_NE(commissioner, nullptr);
    EXPECT_EQ(commissioner->Init(config), ErrorCode::kNone);

    ByteArray joinerId{1, 2, 3, 4, 5, 6, 7, 8};
    ByteArray payload{1, 2, 3};

    EXPECT_CALL(mockHandler, OnJoinerMessage(ContainerEq(joinerId), 5400, ContainerEq(payload))).Times(1);

    JoinerSession session(*static_cast<CommissionerSafe &>(*commissioner).mImpl, joinerId, std::string(), 12345, 0x1200,
                          Address(), 12344, Address(), 12343);
    session.RecvJoinerDtlsRecords(payload, 5400);
    sleep(1);
}

TEST(CommissionerSafeTestProxyMode, ShouldBeAbleToSendToJoinerIfJoinerSessionExists)
{
    CommissionerHandler dummyHandler;

    Config config;

    config.mEnableCcm = false;
    config.mProxyMode = true;
    config.mPSKc      = ByteArray(8);

    // This creates an CommissionerSafe instance.
    auto commissioner = Commissioner::Create(dummyHandler);

    EXPECT_NE(commissioner, nullptr);
    EXPECT_EQ(commissioner->Init(config), ErrorCode::kNone);

    ByteArray joinerId{1, 2, 3, 4, 5, 6, 7, 8};
    ByteArray payload{1, 2, 3};

    auto impl = static_cast<CommissionerSafe &>(*commissioner).mImpl;
    impl->mJoinerSessions.emplace(
        std::piecewise_construct, std::forward_as_tuple(joinerId),
        std::forward_as_tuple(*impl, joinerId, std::string(), 12345, 0x1200, Address(), 12344, Address(), 12343));

    EXPECT_EQ(commissioner->SendToJoiner(joinerId, 5400, payload), ErrorCode::kNone);
}

TEST(CommissionerSafeTestProxyMode, ShouldFailToSendToJoinerForNotFoundIfJoinerSessionDoesNotExist)
{
    CommissionerHandler dummyHandler;

    Config config;

    config.mEnableCcm = false;
    config.mProxyMode = true;
    config.mPSKc      = ByteArray(8);

    // This creates an CommissionerSafe instance.
    auto commissioner = Commissioner::Create(dummyHandler);

    EXPECT_NE(commissioner, nullptr);
    EXPECT_EQ(commissioner->Init(config), ErrorCode::kNone);

    ByteArray joinerId{1, 2, 3, 4, 5, 6, 7, 8};
    ByteArray payload{1, 2, 3};

    EXPECT_EQ(commissioner->SendToJoiner(joinerId, 5400, payload), ErrorCode::kNotFound);
}
} // namespace commissioner
} // namespace ot
