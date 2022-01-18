/*
 *    Copyright (c) 2019, The OpenThread Commissioner Authors.
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
 *   This file defines test cases for DTLS implementation with mbedtls.
 */

#include "library/socket.hpp"

#include <memory.h>

#include <gtest/gtest.h>

#include "common/utils.hpp"

namespace ot {

namespace commissioner {

static std::string kServerAddr = "::";
static uint16_t    kServerPort = 9527;
static std::string kClientAddr = "::";
static uint16_t    kClientPort = 12345;

class MockSocket : public Socket
{
public:
    MockSocket(struct event_base *aEventBase, const Address &aLocalAddr, uint16_t aLocalPort);
    MockSocket(MockSocket &&aOther);
    ~MockSocket() override = default;

    uint16_t GetLocalPort() const override { return mLocalPort; }
    Address  GetLocalAddr() const override { return mLocalAddr; }
    uint16_t GetPeerPort() const override { return mPeerSocket->GetLocalPort(); }
    Address  GetPeerAddr() const override { return mPeerSocket->GetLocalAddr(); }

    int Send(const uint8_t *aBuf, size_t aLen) override;
    int Receive(uint8_t *aBuf, size_t aMaxLen) override;

    int Send(const ByteArray &aBuf);
    int Receive(ByteArray &aBuf);

    int Connect(MockSocketPtr aPeerSocket);

private:
    Address  mLocalAddr;
    uint16_t mLocalPort;

    MockSocketPtr mPeerSocket;
    ByteArray     mRecvBuf;
};

MockSocket::MockSocket(struct event_base *aEventBase, const Address &aLocalAddr, uint16_t aLocalPort)
    : Socket(aEventBase)
    , mLocalAddr(aLocalAddr)
    , mLocalPort(aLocalPort)
    , mPeerSocket(nullptr)
{
}

MockSocket::MockSocket(MockSocket &&aOther)
    : Socket(std::move(aOther))
    , mLocalAddr(std::move(aOther.mLocalAddr))
    , mLocalPort(std::move(aOther.mLocalPort))
    , mPeerSocket(std::move(aOther.mPeerSocket))
{
}

int MockSocket::Send(const uint8_t *aBuf, size_t aLen)
{
    VerifyOrDie(IsConnected());

    auto &peerRecvBuf = mPeerSocket->mRecvBuf;

    peerRecvBuf.insert(peerRecvBuf.end(), aBuf, aBuf + aLen);
    event_active(&mPeerSocket->mEvent, EV_READ, 0);

    return static_cast<int>(aLen);
}

int MockSocket::Receive(uint8_t *aBuf, size_t aMaxLen)
{
    if (mRecvBuf.empty())
    {
        return MBEDTLS_ERR_SSL_WANT_READ;
    }
    auto len = std::min(aMaxLen, mRecvBuf.size());
    memcpy(aBuf, mRecvBuf.data(), len);
    mRecvBuf.erase(mRecvBuf.begin(), mRecvBuf.begin() + len);
    return static_cast<int>(len);
}

int MockSocket::Send(const ByteArray &aBuf)
{
    VerifyOrDie(!aBuf.empty());
    return Send(aBuf.data(), aBuf.size());
}

int MockSocket::Receive(ByteArray &aBuf)
{
    uint8_t buf[512];
    int     rval;

    while ((rval = Receive(buf, sizeof(buf))) > 0)
    {
        aBuf.insert(aBuf.end(), buf, buf + rval);
    }

    VerifyOrExit(rval != MBEDTLS_ERR_SSL_WANT_READ || aBuf.empty(), rval = 0);

exit:
    return rval;
}

int MockSocket::Connect(MockSocketPtr aPeerSocket)
{
    mPeerSocket  = aPeerSocket;
    mIsConnected = true;

    // Setup Event
    int rval = event_assign(&mEvent, mEventBase, -1, EV_PERSIST, HandleEvent, this);
    VerifyOrExit(rval == 0);
    VerifyOrExit((rval = event_add(&mEvent, nullptr)) == 0);

exit:
    return rval;
}

TEST(SocketTest, UdpSocketHello)
{
    const ByteArray kHello{'h', 'e', 'l', 'l', 'o'};
    const ByteArray kWorld{'w', 'o', 'r', 'l', 'd'};

    auto eventBase = event_base_new();
    EXPECT_NE(eventBase, nullptr);

    UdpSocket serverSocket{eventBase};
    serverSocket.SetEventHandler([&](short aFlags) {
        uint8_t buf[1024];
        int     len;

        EXPECT_EQ(serverSocket.GetLocalPort(), kServerPort);

        if (aFlags & EV_READ)
        {
            len = serverSocket.Receive(buf, sizeof(buf));
            EXPECT_GT(len, 0);
            EXPECT_EQ((ByteArray{buf, buf + len}), kHello);

            len = serverSocket.Send(&kWorld[0], kWorld.size());
            EXPECT_EQ(static_cast<size_t>(len), kWorld.size());
        }
    });
    EXPECT_EQ(serverSocket.Bind(kServerAddr, kServerPort), 0);
    EXPECT_EQ(serverSocket.GetLocalPort(), kServerPort);

    UdpSocket clientSocket{eventBase};
    clientSocket.SetEventHandler([&](short aFlags) {
        uint8_t buf[1024];
        int     len;

        if (aFlags & EV_READ)
        {
            len = clientSocket.Receive(buf, sizeof(buf));
            EXPECT_EQ((ByteArray{buf, buf + len}), kWorld);

            event_base_loopbreak(eventBase);
        }
    });

    EXPECT_EQ(clientSocket.Connect(kServerAddr, kServerPort), 0);
    EXPECT_EQ(clientSocket.GetPeerPort(), kServerPort);
    EXPECT_EQ(clientSocket.Send(&kHello[0], kHello.size()), kHello.size());

    EXPECT_EQ(event_base_loop(eventBase, EVLOOP_NO_EXIT_ON_EMPTY), 0);
    event_base_free(eventBase);
}

TEST(SocketTest, MockSocketHello)
{
    const ByteArray kHello{'h', 'e', 'l', 'l', 'o'};
    const ByteArray kWorld{'w', 'o', 'r', 'l', 'd'};

    auto eventBase = event_base_new();
    EXPECT_NE(eventBase, nullptr);

    auto clientSocket = std::make_shared<MockSocket>(eventBase, Address::FromString(kClientAddr), kClientPort);
    auto serverSocket = std::make_shared<MockSocket>(eventBase, Address::FromString(kServerAddr), kServerPort);

    clientSocket->Connect(serverSocket);
    serverSocket->Connect(clientSocket);

    EXPECT_TRUE(clientSocket->IsConnected());
    EXPECT_TRUE(serverSocket->IsConnected());

    EXPECT_EQ(clientSocket->GetPeerAddr(), serverSocket->GetLocalAddr());
    EXPECT_EQ(clientSocket->GetPeerPort(), serverSocket->GetLocalPort());
    EXPECT_EQ(clientSocket->GetLocalAddr(), serverSocket->GetPeerAddr());
    EXPECT_EQ(clientSocket->GetLocalPort(), serverSocket->GetPeerPort());

    serverSocket->SetEventHandler([&](short aFlags) {
        if (aFlags & EV_READ)
        {
            uint8_t buf[1024];
            int     len = serverSocket->Receive(buf, sizeof(buf));
            EXPECT_EQ(static_cast<size_t>(len), kHello.size());

            len = serverSocket->Send(&kWorld[0], kWorld.size());
            EXPECT_EQ(static_cast<size_t>(len), kWorld.size());
        }
    });
    clientSocket->SetEventHandler([&](short aFlags) {
        if (aFlags & EV_READ)
        {
            uint8_t buf[1024];
            int     len = clientSocket->Receive(buf, sizeof(buf));
            EXPECT_EQ(static_cast<size_t>(len), kWorld.size());

            event_base_loopbreak(eventBase);
        }
    });

    int len = clientSocket->Send(&kHello[0], kHello.size());
    EXPECT_EQ(static_cast<size_t>(len), kHello.size());

    EXPECT_EQ(event_base_loop(eventBase, EVLOOP_NO_EXIT_ON_EMPTY), 0);
    event_base_free(eventBase);
}

} // namespace commissioner

} // namespace ot
