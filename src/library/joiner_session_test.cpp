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
 *   This file defines test cases of JoinerSession in proxy mode.
 */

#include "library/joiner_session.hpp"

#include <stdint.h>

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "commissioner/commissioner.hpp"
#include "commissioner/defines.hpp"
#include "commissioner/error.hpp"
#include "common/address.hpp"
#include "event2/event.h"
#include "library/commissioner_impl.hpp"
#include "library/commissioner_test_peer.hpp"

using testing::ContainerEq;

namespace ot {

namespace commissioner {

class JoinerSessionProxyModeTest : public testing::Test
{
protected:
    class MockHandler : public CommissionerHandler
    {
    public:
        MOCK_METHOD(void,
                    OnJoinerMessage,
                    (const ByteArray &aJoinerId, uint16_t aPort, const ByteArray &aPayload),
                    (override));
    };

    void SetUp() override
    {
        Config config;

        config.mEnableCcm = false;
        config.mProxyMode = true;
        config.mPSKc      = ByteArray(16);

        mEventBase = event_base_new();
        ASSERT_NE(mEventBase, nullptr);

        mImpl.reset(new CommissionerImpl(mHandler, mEventBase));
        ASSERT_EQ(mImpl->Init(config), ErrorCode::kNone);
    }

    void TearDown() override
    {
        mImpl.reset();

        if (mEventBase != nullptr)
        {
            event_base_free(mEventBase);
        }
    }

    JoinerSession &AddJoinerSession(const ByteArray &aJoinerId)
    {
        auto &sessions = CommissionerTestPeer::GetJoinerSessions(*mImpl);

        return sessions
            .emplace(std::piecewise_construct, std::forward_as_tuple(aJoinerId),
                     std::forward_as_tuple(*mImpl, aJoinerId, std::string(), 12345, 0x1200, Address(), 12344, Address(),
                                           12343))
            .first->second;
    }

    MockHandler                       mHandler;
    struct event_base                *mEventBase = nullptr;
    std::unique_ptr<CommissionerImpl> mImpl;
};

TEST_F(JoinerSessionProxyModeTest, LargeJoinerMessageDoesNotBlockSubsequentMessages)
{
    const ByteArray kJoinerId{1, 2, 3, 4, 5, 6, 7, 8};
    const ByteArray kLargePayload(1500, 0xab);
    const ByteArray kSmallPayload{1, 2, 3};

    auto &session = AddJoinerSession(kJoinerId);

    {
        testing::InSequence sequence;

        EXPECT_CALL(mHandler, OnJoinerMessage(ContainerEq(kJoinerId), 5400, ContainerEq(kLargePayload))).Times(1);
        EXPECT_CALL(mHandler, OnJoinerMessage(ContainerEq(kJoinerId), 5401, ContainerEq(kSmallPayload))).Times(1);
    }

    session.RecvJoinerDtlsRecords(kLargePayload, 5400);
    session.RecvJoinerDtlsRecords(kSmallPayload, 5401);

    event_base_loop(mEventBase, EVLOOP_NONBLOCK);
}

} // namespace commissioner

} // namespace ot
