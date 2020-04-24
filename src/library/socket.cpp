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

#include "library/socket.hpp"

#include <memory.h>
#include <netinet/in.h>

#include "common/utils.hpp"
#include "library/logging.hpp"

namespace ot {

namespace commissioner {

static uint16_t GetSockPort(const sockaddr_storage &aSockAddr)
{
    if (aSockAddr.ss_family == AF_INET)
    {
        auto &addr4 = *reinterpret_cast<const sockaddr_in *>(&aSockAddr);
        return ntohs(addr4.sin_port);
    }
    else if (aSockAddr.ss_family == AF_INET6)
    {
        auto &addr6 = *reinterpret_cast<const sockaddr_in6 *>(&aSockAddr);
        return ntohs(addr6.sin6_port);
    }
    else
    {
        ASSERT(false);
    }
}

Socket::Socket(struct event_base *aEventBase)
    : mEventBase(aEventBase)
    , mEventHandler(nullptr)
    , mIsConnected(false)
{
    memset(&mEvent, 0, sizeof(mEvent));
}

Socket::~Socket()
{
    Reset();
}

void Socket::Reset()
{
    if (mEvent.ev_base != nullptr)
    {
        event_del(&mEvent);
    }
}

int Socket::Send(void *aSocket, const uint8_t *aBuf, size_t aLen)
{
    return reinterpret_cast<Socket *>(aSocket)->Send(aBuf, aLen);
}

int Socket::Receive(void *aSocket, uint8_t *aBuf, size_t aMaxLen)
{
    return reinterpret_cast<Socket *>(aSocket)->Receive(aBuf, aMaxLen);
}

void Socket::HandleEvent(evutil_socket_t, short aFlags, void *aSocket)
{
    auto socket = reinterpret_cast<Socket *>(aSocket);

    ASSERT(socket->mEventHandler != nullptr);
    socket->mEventHandler(aFlags);
}

UdpSocket::UdpSocket(struct event_base *aEventBase)
    : Socket(aEventBase)
    , mIsBound(false)
{
    mbedtls_net_init(&mNetCtx);
}

UdpSocket::~UdpSocket()
{
    mbedtls_net_free(&mNetCtx);
}

UdpSocket::UdpSocket(UdpSocket &&aOther)
    : Socket(aOther.mEventBase)
    , mNetCtx(aOther.mNetCtx)
    , mIsBound(aOther.mIsBound)
{
    mbedtls_net_init(&aOther.mNetCtx);
}

int UdpSocket::Connect(const std::string &aHost, uint16_t aPort)
{
    auto portStr = std::to_string(aPort);

    // Connect
    int rval = mbedtls_net_connect(&mNetCtx, aHost.c_str(), portStr.c_str(), MBEDTLS_NET_PROTO_UDP);
    VerifyOrExit(rval == 0);
    VerifyOrExit((rval = mbedtls_net_set_nonblock(&mNetCtx)) == 0);

    // Setup event
    rval = event_assign(&mEvent, mEventBase, mNetCtx.fd, EV_PERSIST | EV_READ | EV_WRITE | EV_ET, HandleEvent, this);
    VerifyOrExit(rval == 0);
    VerifyOrExit((rval = event_add(&mEvent, nullptr)) == 0);

exit:
    if (rval != 0)
    {
        mbedtls_net_free(&mNetCtx);
    }
    else
    {
        mIsConnected = true;
    }
    return rval;
}

int UdpSocket::Bind(const std::string &aBindIp, uint16_t aPort)
{
    auto portStr = std::to_string(aPort);

    // Bind
    int rval = mbedtls_net_bind(&mNetCtx, aBindIp.c_str(), portStr.c_str(), MBEDTLS_NET_PROTO_UDP);
    VerifyOrExit(rval == 0);
    VerifyOrExit((rval = mbedtls_net_set_nonblock(&mNetCtx)) == 0);

    // Setup Event
    rval = event_assign(&mEvent, mEventBase, mNetCtx.fd, EV_PERSIST | EV_READ | EV_WRITE | EV_ET, HandleEvent, this);
    VerifyOrExit(rval == 0);
    VerifyOrExit((rval = event_add(&mEvent, nullptr)) == 0);

exit:
    if (rval != 0)
    {
        mbedtls_net_free(&mNetCtx);
    }
    else
    {
        mIsBound = true;
    }
    return rval;
}

uint16_t UdpSocket::GetLocalPort() const
{
    sockaddr_storage addr;
    socklen_t        len = sizeof(sockaddr_storage);

    ASSERT(getsockname(mNetCtx.fd, reinterpret_cast<sockaddr *>(&addr), &len) == 0);
    return GetSockPort(addr);
}

Address UdpSocket::GetLocalAddr() const
{
    sockaddr_storage addr;
    socklen_t        len = sizeof(sockaddr_storage);

    ASSERT(getsockname(mNetCtx.fd, reinterpret_cast<sockaddr *>(&addr), &len) == 0);

    Address ret;
    ASSERT(ret.Set(addr) == Error::kNone);
    return ret;
}

uint16_t UdpSocket::GetPeerPort() const
{
    sockaddr_storage addr;
    socklen_t        len = sizeof(sockaddr_storage);

    ASSERT(mIsConnected);

    ASSERT(getpeername(mNetCtx.fd, reinterpret_cast<sockaddr *>(&addr), &len) == 0);
    return GetSockPort(addr);
}

Address UdpSocket::GetPeerAddr() const
{
    sockaddr_storage addr;
    socklen_t        len = sizeof(sockaddr_storage);

    ASSERT(mIsConnected);

    ASSERT(getpeername(mNetCtx.fd, reinterpret_cast<sockaddr *>(&addr), &len) == 0);

    Address ret;
    ASSERT(ret.Set(addr) == Error::kNone);
    return ret;
}

void UdpSocket::Reset()
{
    Socket::Reset();
    mbedtls_net_free(&mNetCtx);
}

int UdpSocket::Send(const uint8_t *aBuf, size_t aLen)
{
    ASSERT(mNetCtx.fd != -1);
    ASSERT(mIsConnected);

    return mbedtls_net_send(&mNetCtx, aBuf, aLen);
}

int UdpSocket::Receive(uint8_t *aBuf, size_t aMaxLen)
{
    ASSERT(mNetCtx.fd != -1);
    ASSERT(mIsConnected);

    return mbedtls_net_recv(&mNetCtx, aBuf, aMaxLen);
}

void UdpSocket::SetEventHandler(EventHandler aEventHandler)
{
    mEventHandler = [this, aEventHandler](short aFlags) {
        if (mIsBound && !mIsConnected && (aFlags & EV_READ))
        {
            mbedtls_net_context connectedCtx;
            mbedtls_net_init(&connectedCtx);

            // This will get 'mNetCtx' connected and returned as 'connectCtx'.
            int rval = mbedtls_net_accept(&mNetCtx, &connectedCtx, nullptr, 0, nullptr);
            if (rval != 0)
            {
                LOG_INFO("bound UDP socket accept new connection failed: {}", rval);
            }
            else
            {
                mbedtls_net_free(&mNetCtx);

                // We know that 'connectedCtx' has the same fd as the original 'mNetCtx' before accept.
                mNetCtx      = connectedCtx;
                mIsConnected = true;
            }
        }

        // Do not handle event unless the socket is connected.
        if (mIsConnected)
        {
            aEventHandler(aFlags);
        }
    };
}

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
    ASSERT(IsConnected());

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
    memcpy(aBuf, &mRecvBuf[0], len);
    mRecvBuf.erase(mRecvBuf.begin(), mRecvBuf.begin() + len);
    return static_cast<int>(len);
}

int MockSocket::Send(const ByteArray &aBuf)
{
    ASSERT(!aBuf.empty());
    return Send(&aBuf[0], aBuf.size());
}

int MockSocket::Receive(ByteArray &aBuf)
{
    uint8_t buf[512];
    int     rval;
    while ((rval = Receive(buf, sizeof(buf))) > 0)
    {
        aBuf.insert(aBuf.end(), buf, buf + rval);
    }
    if (rval == MBEDTLS_ERR_SSL_WANT_READ && !aBuf.empty())
    {
        return 0;
    }
    return rval;
}

int MockSocket::Connect(MockSocketPtr aPeerSocket)
{
    mPeerSocket  = aPeerSocket;
    mIsConnected = true;

    // Setup Event
    int rval = event_assign(&mEvent, mEventBase, -1, EV_PERSIST | EV_READ | EV_WRITE | EV_ET, HandleEvent, this);
    VerifyOrExit(rval == 0);
    VerifyOrExit((rval = event_add(&mEvent, nullptr)) == 0);

exit:
    return rval;
}

} // namespace commissioner

} // namespace ot
