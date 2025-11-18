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

#include "library/socket.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory.h>
#include <string>

#include <netinet/in.h>
#include <sys/socket.h>

#include "common/address.hpp"
#include "common/logging.hpp"
#include "common/utils.hpp"
#include "event2/event.h"
#include "event2/util.h"
#include "library/event.hpp"
#include "library/message.hpp"
#include "mbedtls/net_sockets.h"

namespace ot {

namespace commissioner {

static uint16_t GetSockPort(const sockaddr_storage &aSockAddr)
{
    uint16_t port;

    if (aSockAddr.ss_family == AF_INET)
    {
        auto &addr4 = *reinterpret_cast<const sockaddr_in *>(&aSockAddr);
        ExitNow(port = ntohs(addr4.sin_port));
    }
    else if (aSockAddr.ss_family == AF_INET6)
    {
        auto &addr6 = *reinterpret_cast<const sockaddr_in6 *>(&aSockAddr);
        ExitNow(port = ntohs(addr6.sin6_port));
    }
    else
    {
        VerifyOrDie(false);
    }

exit:
    return port;
}

Socket::Socket(struct event_base *aEventBase)
    : mEventBase(aEventBase)
    , mEventHandler(nullptr)
    , mIsConnected(false)
    , mSubType(MessageSubType::kNone)
{
    memset(&mEvent, 0, sizeof(mEvent));
}

Socket::~Socket()
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

    VerifyOrDie(socket->mEventHandler != nullptr);
    socket->mEventHandler(aFlags);
}

UdpSocket::UdpSocket(struct event_base *aEventBase)
    : Socket(aEventBase)
    , mIsBound(false)
{
    mbedtls_net_init(&mNetCtx);
}

UdpSocket::~UdpSocket() { mbedtls_net_free(&mNetCtx); }

UdpSocket::UdpSocket(UdpSocket &&aOther)
    : Socket(aOther.mEventBase)
    , mNetCtx(aOther.mNetCtx)
    , mIsBound(aOther.mIsBound)
{
    mbedtls_net_init(&aOther.mNetCtx);
}

int UdpSocket::Connect(const std::string &aPeerAddr, uint16_t aPeerPort)
{
    auto portStr = std::to_string(aPeerPort);

    // Free the fd if already opened.
    mbedtls_net_free(&mNetCtx);
    if (mEvent.ev_base != nullptr)
    {
        event_del(&mEvent);
    }

    // Connect
    int rval = mbedtls_net_connect(&mNetCtx, aPeerAddr.c_str(), portStr.c_str(), MBEDTLS_NET_PROTO_UDP);
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

int UdpSocket::Bind(const std::string &aLocalAddr, uint16_t aLocalPort)
{
    auto portStr = std::to_string(aLocalPort);

    // Free the fd if already opened.
    mbedtls_net_free(&mNetCtx);
    if (mEvent.ev_base != nullptr)
    {
        event_del(&mEvent);
    }

    // Bind
    int rval = mbedtls_net_bind(&mNetCtx, aLocalAddr.c_str(), portStr.c_str(), MBEDTLS_NET_PROTO_UDP);
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

    VerifyOrDie(getsockname(mNetCtx.fd, reinterpret_cast<sockaddr *>(&addr), &len) == 0);
    return GetSockPort(addr);
}

Address UdpSocket::GetLocalAddr() const
{
    sockaddr_storage addr;
    socklen_t        len = sizeof(sockaddr_storage);
    Address          ret;

    VerifyOrDie(getsockname(mNetCtx.fd, reinterpret_cast<sockaddr *>(&addr), &len) == 0);
    SuccessOrDie(ret.Set(addr));
    return ret;
}

uint16_t UdpSocket::GetPeerPort() const
{
    sockaddr_storage addr;
    socklen_t        len = sizeof(sockaddr_storage);

    VerifyOrDie(mIsConnected);

    VerifyOrDie(getpeername(mNetCtx.fd, reinterpret_cast<sockaddr *>(&addr), &len) == 0);
    return GetSockPort(addr);
}

Address UdpSocket::GetPeerAddr() const
{
    sockaddr_storage addr;
    socklen_t        len = sizeof(sockaddr_storage);
    Address          ret;

    VerifyOrDie(mIsConnected);
    VerifyOrDie(getpeername(mNetCtx.fd, reinterpret_cast<sockaddr *>(&addr), &len) == 0);
    SuccessOrDie(ret.Set(addr));
    return ret;
}

int UdpSocket::Send(const uint8_t *aBuf, size_t aLen)
{
    VerifyOrDie(mNetCtx.fd >= 0);
    VerifyOrDie(mIsConnected);

    return mbedtls_net_send(&mNetCtx, aBuf, aLen);
}

int UdpSocket::Receive(uint8_t *aBuf, size_t aMaxLen)
{
    VerifyOrDie(mNetCtx.fd >= 0);
    VerifyOrDie(mIsConnected);

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
                LOG_INFO(LOG_REGION_SOCKET, "UDP socket(={}) accept new connection failed: {}",
                         static_cast<void *>(this), rval);
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

} // namespace commissioner

} // namespace ot
