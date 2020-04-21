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
 *   This file includes wrapper of mbedtls.
 */

#ifndef OT_COMM_LIBRARY_DTLS_HPP_
#define OT_COMM_LIBRARY_DTLS_HPP_

#include <functional>
#include <list>
#include <queue>
#include <vector>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ssl_cookie.h>
#include <mbedtls/timing.h>

#include "commissioner/commissioner.hpp"
#include "commissioner/defines.hpp"

#include "common/utils.hpp"
#include "library/endpoint.hpp"
#include "library/event.hpp"
#include "library/socket.hpp"
#include "library/timer.hpp"

namespace ot {

namespace commissioner {

static constexpr uint32_t kDtlsHandshakeTimeoutMin = 8;
static constexpr uint32_t kDtlsHandshakeTimeoutMax = 60;

struct DtlsConfig
{
    bool      mEnableDebugLogging = false;
    int       mLogLevel           = 3;
    ByteArray mPSK;
    ByteArray mOwnKey;
    ByteArray mOwnCert;
    ByteArray mCaChain;
};

DtlsConfig GetDtlsConfig(const Config &aConfig);

class DtlsSession : public Endpoint
{
public:
    using ConnectHandler = std::function<void(DtlsSession &, Error)>;

    enum class State
    {
        kOpen,
        kConnecting,
        kConnected,
        kDisconnected,
    };

    DtlsSession(struct event_base *aEventBase, bool aIsServer, SocketPtr aSocket);

    ~DtlsSession() override;
    DtlsSession(const DtlsSession &aOther) = delete;
    const DtlsSession &operator=(const DtlsSession &aOther) = delete;

    Error Send(const ByteArray &aBuf) override;

    Error Init(const DtlsConfig &aConfig);

    // Reset session state without changing user configurations.
    void Reset();

    void  Connect(ConnectHandler aOnConnected);
    Error Bind(const std::string &aBindIp, uint16_t aPort);
    void  Disconnect(Error aError);

    State GetState() const { return mState; }

    Address  GetPeerAddr() const override { return mSocket->GetPeerAddr(); }
    uint16_t GetPeerPort() const override { return mSocket->GetPeerPort(); }

    uint16_t GetLocalPort() const { return mSocket->GetLocalPort(); }

    const mbedtls_x509_crt *GetPeerCertificate() const { return mSsl.session ? mSsl.session->peer_cert : nullptr; }

    const ByteArray &GetKek() const { return mKek; }

    void HandleEvent(short aFlags);

private:
    class DtlsTimer : public Timer
    {
    public:
        DtlsTimer(struct event_base *aEventBase, Action aAction)
            : Timer(aEventBase, aAction)
            , mCancelled(false)
        {
        }

        static int  GetDelay(void *aDtlsTimer);
        static void SetDelay(void *aDtlsTimer, uint32_t aIntermediate, uint32_t aFinish);

    private:
        TimePoint mIntermediate;
        bool      mCancelled;
    };

    void HandshakeTimerCallback(Timer &aTimer);

    void InitMbedtls();
    void FreeMbedtls();

    // Reset DTLS state and connect.
    void Reconnect();

    // return:
    //   Error::kNone
    //   Error::kTransportBusy
    //   Error::kTransportFailed
    Error Read();

    // return:
    //   Error::kNone
    //   Error::kInvalidArgs
    //   Error::kTransportBusy
    //   Error::kTransportFailed
    Error Write(const ByteArray &aBuf);

    Error TryWrite();

    // return:
    //   Error::kNone
    //   Error::kTransportBusy
    //   Error::kTransportFailed
    Error Handshake();

    Error SetClientTransportId();

    // Decide if we should stop processing this session by given error.
    static bool ShouldStop(Error aError);

    static int HandleMbedtlsExportKeys(void *               aDtlsSession,
                                       const unsigned char *aMasterSecret,
                                       const unsigned char *aKeyBlock,
                                       size_t               aMacLength,
                                       size_t               aKeyLength,
                                       size_t               aIvLength);

    int HandleMbedtlsExportKeys(const unsigned char *aMasterSecret,
                                const unsigned char *aKeyBlock,
                                size_t               aMacLength,
                                size_t               aKeyLength,
                                size_t               aIvLength);

    SocketPtr mSocket;
    DtlsTimer mHandshakeTimer;

    State mState;
    bool  mIsServer;
    bool  mIsClientIdSet = false;

    ByteArray mKek;

    ConnectHandler mOnConnected = nullptr;

    std::queue<ByteArray> mSendQueue;

    std::vector<int>         mCipherSuites;
    mbedtls_ssl_config       mConfig;
    mbedtls_ssl_cookie_ctx   mCookie;
    mbedtls_ctr_drbg_context mCtrDrbg;
    mbedtls_entropy_context  mEntropy;
    mbedtls_ssl_context      mSsl;

    ByteArray          mPSK;
    mbedtls_x509_crt   mCaChain;
    mbedtls_x509_crt   mOwnCert;
    mbedtls_pk_context mOwnKey;
};

using DtlsSessionPtr = std::shared_ptr<DtlsSession>;

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_LIBRARY_DTLS_HPP_
