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
 *   This file includes definition of Thread 1.1 joiner session.
 */

#ifndef OT_COMM_LIBRARY_JOINER_SESSION_HPP
#define OT_COMM_LIBRARY_JOINER_SESSION_HPP

#include <functional>
#include <map>

#include <commissioner/error.hpp>

#include "library/coap.hpp"
#include "library/coap_secure.hpp"
#include "library/dtls.hpp"

namespace ot {

namespace commissioner {

// The listening port for incoming DTLS connections from joiner.
static constexpr uint16_t kListeningJoinerPort = 9527;

// The joiner session timeout value starting from connected, in seconds.
// This is the time duration of waiting for JOIN_FIN.req. After that,
// the joiner session will be closed and removed.
static constexpr uint32_t kJoinerTimeout = 20;

static constexpr uint8_t kLocalExternalAddrMask = 1 << 1;

class CommissionerImpl;
class JoinerSession;

using JoinerSessionPtr = std::shared_ptr<JoinerSession>;

// The session of commissioning a joiner.
class JoinerSession : std::enable_shared_from_this<JoinerSession>
{
public:
    JoinerSession(CommissionerImpl & aCommImpl,
                  const ByteArray &  aJoinerId,
                  const std::string &aJoinerPSkd,
                  uint16_t           aJoinerUdpPort,
                  uint16_t           aJoinerRouterLocator,
                  const Address &    aJoinerAddr,
                  uint16_t           aJoinerPort,
                  const Address &    aLocalAddr,
                  uint16_t           aLocalPort);
    JoinerSession(JoinerSession &&aOther) = delete;
    JoinerSession &operator=(JoinerSession &&aOther) = delete;
    JoinerSession(const JoinerSession &aOther)       = delete;
    JoinerSession &operator=(const JoinerSession &aOther) = delete;
    ~JoinerSession()                                      = default;

    ByteArray GetJoinerId() const { return mJoinerId; }
    uint16_t  GetJoinerUdpPort() const { return mJoinerUdpPort; }
    uint16_t  GetJoinerRouterLocator() const { return mJoinerRouterLocator; }
    Address   GetPeerAddr() const { return mDtlsSession->GetPeerAddr(); }
    uint16_t  GetPeerPort() const { return mDtlsSession->GetPeerPort(); }

    void Connect();

    DtlsSession::State GetState() const { return mDtlsSession->GetState(); }

    bool Disabled() const { return mDtlsSession->GetState() == DtlsSession::State::kOpen; }

    void RecvJoinerDtlsRecords(const ByteArray &aRecords);

    const TimePoint &GetExpirationTime() const { return mExpirationTime; }

private:
    friend class RelaySocket;

    class RelaySocket : public Socket
    {
    public:
        RelaySocket(JoinerSession &aJoinerSession,
                    const Address &aPeerAddr,
                    uint16_t       aPeerPort,
                    const Address &aLocalAddr,
                    uint16_t       aLocalPort);
        RelaySocket(RelaySocket &&aOther);
        ~RelaySocket() override = default;

        uint16_t GetLocalPort() const override { return mLocalPort; }
        Address  GetLocalAddr() const override { return mLocalAddr; }
        uint16_t GetPeerPort() const override { return mPeerPort; }
        Address  GetPeerAddr() const override { return mPeerAddr; }

        int Send(const uint8_t *aBuf, size_t aLen) override;
        int Receive(uint8_t *aBuf, size_t aMaxLen) override;

        void RecvJoinerDtlsRecords(const ByteArray &aRecords);

    private:
        JoinerSession &mJoinerSession;
        Address        mPeerAddr;
        uint16_t       mPeerPort;
        Address        mLocalAddr;
        uint16_t       mLocalPort;
        ByteArray      mRecvBuf;
    };

    using RelaySocketPtr = std::shared_ptr<RelaySocket>;

    ByteArray GetJoinerIid() const;

    void HandleConnect(Error aError);

    Error SendRlyTx(const ByteArray &aDtlsMessage, bool aIncludeKek);
    void  HandleJoinFin(const coap::Request &aJoinFin);
    void  SendJoinFinResponse(const coap::Request &aJoinFinReq, bool aAccept);

    CommissionerImpl &mCommImpl;

    ByteArray   mJoinerId;
    std::string mJoinerPSKd;
    uint16_t    mJoinerUdpPort;
    uint16_t    mJoinerRouterLocator;

    RelaySocketPtr mRelaySocket;
    DtlsSessionPtr mDtlsSession;
    coap::Coap     mCoap;

    coap::Resource mResourceJoinFin;

    TimePoint mExpirationTime;
};

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_LIBRARY_JOINER_SESSION_HPP
