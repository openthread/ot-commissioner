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
 *   This file includes definition of 1.1 mesh Commissioning Session.
 */

#ifndef COMMISSIONING_SESSION_HPP_
#define COMMISSIONING_SESSION_HPP_

#include <commissioner/error.hpp>

#include <functional>
#include <map>

#include "coap.hpp"
#include "coap_secure.hpp"
#include "dtls.hpp"

namespace ot {

namespace commissioner {

// The port a 1.1 mesh commissioning server is listen on for
// incoming DTLS connections from joiner.
static constexpr uint16_t kCommissioningPort = 9527;

// The commissioning session timeout value starting from connected. In seconds.
// This is the time duration of waiting for JOIN_FIN.req. After that,
// the commissioning session will be closed and removed.
static constexpr uint32_t kCommissioningTimeout = 20;

class CommissionerImpl;
class CommissioningSession;

using CommissioningSessionPtr = std::shared_ptr<CommissioningSession>;

// The session of commissioning a joiner.
class CommissioningSession : std::enable_shared_from_this<CommissioningSession>
{
public:
    using ConnectHandler = std::function<void(CommissioningSession &, Error)>;

    CommissioningSession(CommissionerImpl &aCommImpl,
                         const JoinerInfo &aJoinerInfo,
                         uint16_t          aJoinerUdpPort,
                         uint16_t          aJoinerRouterLocator,
                         const ByteArray & aJoinerIid,
                         const Address &   aJoinerAddr,
                         uint16_t          aJoinerPort);
    CommissioningSession(CommissioningSession &&aOther) = delete;
    CommissioningSession &operator=(CommissioningSession &&aOther) = delete;
    CommissioningSession(const CommissioningSession &aOther)       = delete;
    CommissioningSession &operator=(const CommissioningSession &aOther) = delete;
    ~CommissioningSession()                                             = default;

    uint16_t  GetJoinerUdpPort() const { return mJoinerUdpPort; }
    uint16_t  GetJoinerRouterLocator() const { return mJoinerRouterLocator; }
    ByteArray GetJoinerIid() const { return mJoinerIid; }
    Address   GetPeerAddr() const { return mDtlsSession->GetPeerAddr(); }
    uint16_t  GetPeerPort() const { return mDtlsSession->GetPeerPort(); }

    Error Start(ConnectHandler aOnConnected);

    void Stop();

    DtlsSession::State GetState() const { return mDtlsSession->GetState(); }

    bool Disabled() const { return mDtlsSession->GetState() == DtlsSession::State::kOpen; }

    Error RecvJoinerDtlsRecords(const ByteArray &aRecords);

    const TimePoint &GetExpirationTime() const { return mExpirationTime; }

private:
    void  HandleJoinerSocketEvent(short aFlags);
    Error SendRlyTx(const ByteArray &aBuf);
    void  HandleJoinFin(const coap::Request &aJoinFin);
    Error SendJoinFinResponse(const coap::Request &aJoinFinReq, bool aAccept);

    CommissionerImpl &mCommImpl;
    JoinerInfo        mJoinerInfo;

    uint16_t  mJoinerUdpPort;
    uint16_t  mJoinerRouterLocator;
    ByteArray mJoinerIid;

    MockSocketPtr  mJoinerSocket;
    MockSocketPtr  mSocket;
    DtlsSessionPtr mDtlsSession;
    coap::Coap     mCoap;

    coap::Resource mResourceJoinFin;

    ConnectHandler mOnConnected = nullptr;

    TimePoint mExpirationTime;
};

} // namespace commissioner

} // namespace ot

#endif // COMMISSIONING_SESSION_HPP_
