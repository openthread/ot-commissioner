/*
 *    Copyright (c) 2019, The OpenThread Authors.
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
 *   The file includes definitions of Socket.
 */

#ifndef OT_COMM_LIBRARY_SOCKET_HPP_
#define OT_COMM_LIBRARY_SOCKET_HPP_

#include <memory>
#include <string>

#include <mbedtls/net_sockets.h>

#include <commissioner/defines.hpp>

#include "common/address.hpp"
#include "library/event.hpp"

namespace ot {

namespace commissioner {

class Socket
{
public:
    explicit Socket(struct event_base *aEventBase);
    Socket(Socket &&aOther) = default;
    Socket &operator=(Socket &&aOther) = delete;
    Socket(const Socket &aOther)       = delete;
    Socket &operator=(const Socket &aOther) = delete;
    virtual ~Socket();

    // Must only be called when the socket has valid local port.
    // For example, it has been bind to a local address and port.
    virtual uint16_t GetLocalPort() const = 0;

    // Must only be called when the socket has valid local address.
    virtual Address GetLocalAddr() const = 0;

    // Must only be called when the socket has been connect to a valid peer.
    virtual uint16_t GetPeerPort() const = 0;

    // Must only be called when the socket has been connect to a valid peer.
    virtual Address GetPeerAddr() const = 0;

    bool IsConnected() const { return mIsConnected; }

    virtual void Reset();

    virtual int Send(const uint8_t *aBuf, size_t aLen) = 0;

    virtual int Receive(uint8_t *aBuf, size_t aMaxLen) = 0;

    virtual void SetEventHandler(EventHandler aEventHandler) { mEventHandler = aEventHandler; }

    // Designed for mbedtls callbacks.
    static int Send(void *aSocket, const uint8_t *aBuf, size_t aLen);
    static int Receive(void *aSocket, uint8_t *aBuf, size_t aMaxLen);

protected:
    static void HandleEvent(evutil_socket_t aFd, short aFlags, void *aSocket);

    struct event_base *mEventBase;
    struct event       mEvent;
    EventHandler       mEventHandler;
    bool               mIsConnected;
};

using SocketPtr = std::shared_ptr<Socket>;

class UdpSocket : public Socket
{
public:
    explicit UdpSocket(struct event_base *aEventBase);
    UdpSocket(UdpSocket &&aOther);
    ~UdpSocket() override;

    int Connect(const std::string &aPeerAddr, uint16_t aPeerPort);

    int Bind(const std::string &aLocalAddr, uint16_t aLocalPort);

    uint16_t GetLocalPort() const override;
    Address  GetLocalAddr() const override;
    uint16_t GetPeerPort() const override;
    Address  GetPeerAddr() const override;

    void Reset() override;

    int Send(const uint8_t *aBuf, size_t aLen) override;

    int Receive(uint8_t *aBuf, size_t aMaxLen) override;

    void SetEventHandler(EventHandler aEventHandler) override;

private:
    mbedtls_net_context mNetCtx;
    bool                mIsBound;
};

using UdpSocketPtr = std::shared_ptr<UdpSocket>;

class MockSocket;
using MockSocketPtr = std::shared_ptr<MockSocket>;

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

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_LIBRARY_SOCKET_HPP_
