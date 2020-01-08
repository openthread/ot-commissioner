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
 *   This file implements wrapper of mbedtls.
 */

#include "dtls.hpp"

#include <cassert>
#include <cstring>

#include <algorithm>
#include <iostream>

#include <mbedtls/debug.h>
#include <mbedtls/pem.h>
#include <mbedtls/platform.h>

#include "logging.hpp"
#include "openthread/sha256.hpp"

namespace ot {

namespace commissioner {

static const int    kAuthMode              = MBEDTLS_SSL_VERIFY_REQUIRED;
static const size_t kMaxContentLength      = MBEDTLS_SSL_MAX_CONTENT_LEN;
static const size_t KMaxFragmentLengthCode = MBEDTLS_SSL_MAX_FRAG_LEN_1024;
static const size_t kMaxTransmissionUnit   = 1280;

static_assert(256 * (1 << KMaxFragmentLengthCode) <= kMaxContentLength, "invalid DTLS Max Fragment Length");

static void HandleMbedtlsDebug(void *, int level, const char *file, int line, const char *str)
{
    switch (level)
    {
    case 1:
        LOG_CRIT("{}, {}: {}", file, line, str);
        break;

    case 2:
        LOG_WARN("{}, {}: {}", file, line, str);
        break;

    case 3:
        LOG_INFO("{}, {}: {}", file, line, str);
        break;

    case 4:
    default:
        LOG_DEBUG("{}, {}: {}", file, line, str);
        break;
    }
}

DtlsConfig GetDtlsConfig(const Config &aConfig)
{
    DtlsConfig dtlsConfig;

    dtlsConfig.mEnableDebugLogging = aConfig.mEnableDtlsDebugLogging;

    dtlsConfig.mPSK     = aConfig.mPSKc;
    dtlsConfig.mOwnKey  = aConfig.mPrivateKey;
    dtlsConfig.mOwnCert = aConfig.mCertificate;
    dtlsConfig.mCaChain = aConfig.mTrustAnchor;

    return dtlsConfig;
}

DtlsSession::DtlsSession(struct event_base *aEventBase, bool aIsServer, SocketPtr aSocket)
    : mSocket(aSocket)
    , mHandshakeTimer(aEventBase, [this](Timer &aTimer) { HandshakeTimerCallback(aTimer); })
    , mState(State::kOpen)
    , mIsServer(aIsServer)
{
    mSocket->SetEventHandler([this](short aFlags) { HandleEvent(aFlags); });
    InitMbedtls();
}

DtlsSession::~DtlsSession()
{
    FreeMbedtls();
}

void DtlsSession::InitMbedtls()
{
    mbedtls_ssl_config_init(&mConfig);
    mbedtls_ssl_cookie_init(&mCookie);
    mbedtls_ctr_drbg_init(&mCtrDrbg);
    mbedtls_entropy_init(&mEntropy);
    mbedtls_ssl_init(&mSsl);

    mbedtls_x509_crt_init(&mCaChain);
    mbedtls_x509_crt_init(&mOwnCert);
    mbedtls_pk_init(&mOwnKey);
}

void DtlsSession::FreeMbedtls()
{
    mbedtls_pk_free(&mOwnKey);
    mbedtls_x509_crt_free(&mOwnCert);
    mbedtls_x509_crt_free(&mCaChain);

    mbedtls_ssl_free(&mSsl);
    mbedtls_entropy_free(&mEntropy);
    mbedtls_ctr_drbg_free(&mCtrDrbg);
    mbedtls_ssl_cookie_free(&mCookie);
    mbedtls_ssl_config_free(&mConfig);
}

Error DtlsSession::Init(const DtlsConfig &aConfig)
{
    int rval;
    rval = mbedtls_ssl_config_defaults(&mConfig, mIsServer, MBEDTLS_SSL_TRANSPORT_DATAGRAM, MBEDTLS_SSL_PRESET_DEFAULT);
    VerifyOrExit(rval == 0);

    mbedtls_ssl_conf_authmode(&mConfig, kAuthMode);

    // Debug
    mbedtls_debug_set_threshold(10);

    if (aConfig.mEnableDebugLogging)
    {
        mbedtls_ssl_conf_dbg(&mConfig, HandleMbedtlsDebug, nullptr);
    }

    // Timeouts
    mbedtls_ssl_conf_handshake_timeout(&mConfig, kDtlsHandshakeTimeoutMin * 1000, kDtlsHandshakeTimeoutMax * 1000);

    mCipherSuites.clear();

    // PSK
    if (aConfig.mPSK.size() != 0)
    {
        mPSK = aConfig.mPSK;
        mCipherSuites.push_back(MBEDTLS_TLS_ECJPAKE_WITH_AES_128_CCM_8);
    }

    // X509
    if (aConfig.mCaChain.size() != 0 || aConfig.mOwnCert.size() != 0 || aConfig.mOwnKey.size() != 0)
    {
        rval = mbedtls_x509_crt_parse(&mCaChain, &aConfig.mCaChain[0], aConfig.mCaChain.size());
        VerifyOrExit(rval == 0);

        rval = mbedtls_x509_crt_parse(&mOwnCert, &aConfig.mOwnCert[0], aConfig.mOwnCert.size());
        VerifyOrExit(rval == 0);

        rval = mbedtls_pk_parse_key(&mOwnKey, &aConfig.mOwnKey[0], aConfig.mOwnKey.size(), nullptr, 0);
        VerifyOrExit(rval == 0);

        mCipherSuites.push_back(MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_CCM_8);

        mbedtls_ssl_conf_ca_chain(&mConfig, &mCaChain, nullptr);
        rval = mbedtls_ssl_conf_own_cert(&mConfig, &mOwnCert, &mOwnKey);
        VerifyOrExit(rval == 0);
    }

    mCipherSuites.push_back(0);
    mbedtls_ssl_conf_ciphersuites(&mConfig, &mCipherSuites[0]);

    mbedtls_ssl_conf_export_keys_cb(&mConfig, HandleMbedtlsExportKeys, this);

    // RNG & Entropy
    rval = mbedtls_ctr_drbg_seed(&mCtrDrbg, mbedtls_entropy_func, &mEntropy, nullptr, 0);
    VerifyOrExit(rval == 0);

    mbedtls_ssl_conf_rng(&mConfig, mbedtls_ctr_drbg_random, &mCtrDrbg);

    // Cookie
    if (mIsServer)
    {
        rval = mbedtls_ssl_cookie_setup(&mCookie, mbedtls_ctr_drbg_random, &mCtrDrbg);
        VerifyOrExit(rval == 0);

        mbedtls_ssl_conf_dtls_cookies(&mConfig, mbedtls_ssl_cookie_write, mbedtls_ssl_cookie_check, &mCookie);
    }

    // bio
    // mbedtls_ssl_set_bio(&mSsl, &mNetCtx, mbedtls_net_send, mbedtls_net_recv, nullptr);
    mbedtls_ssl_set_bio(&mSsl, mSocket.get(), Socket::Send, Socket::Receive, nullptr);

    // Timer
    mbedtls_ssl_set_timer_cb(&mSsl, &mHandshakeTimer, DtlsTimer::SetDelay, DtlsTimer::GetDelay);

    rval = mbedtls_ssl_conf_max_frag_len(&mConfig, KMaxFragmentLengthCode);
    VerifyOrExit(rval == 0);

    // MTU
    mbedtls_ssl_set_mtu(&mSsl, kMaxTransmissionUnit);

    // Setup
    rval = mbedtls_ssl_setup(&mSsl, &mConfig);
    VerifyOrExit(rval == 0);

    // Set EC-JPAKE password after initializing the SSL object.
    if (!aConfig.mPSK.empty())
    {
        rval = mbedtls_ssl_set_hs_ecjpake_password(&mSsl, &aConfig.mPSK[0], aConfig.mPSK.size());
        VerifyOrExit(rval == 0);
    }

exit:
    // TODO(wgtdkp): more specific return error
    return rval == 0 ? Error::kNone : Error::kFailed;
}

void DtlsSession::Reset()
{
    VerifyOrExit(mState == State::kConnecting || mState == State::kConnected || mState == State::kDisconnected);

    mbedtls_ssl_session_reset(&mSsl);

    mIsClientIdSet = false;

    if (!mPSK.empty())
    {
        mbedtls_ssl_set_hs_ecjpake_password(&mSsl, &mPSK[0], mPSK.size());
    }

    mState = State::kOpen;

exit:
    return;
}

void DtlsSession::Connect(ConnectHandler aOnConnected)
{
    ASSERT(mState == State::kOpen);

    mOnConnected = aOnConnected;
    mState       = State::kConnecting;
}

void DtlsSession::Reconnect()
{
    ASSERT(mIsServer);
    Reset();
    Connect(mOnConnected);
}

void DtlsSession::Disconnect()
{
    VerifyOrExit(mState == State::kConnecting || mState == State::kConnected || mState == State::kDisconnecting);

    switch (mbedtls_ssl_close_notify(&mSsl))
    {
    case MBEDTLS_ERR_SSL_WANT_READ:
    case MBEDTLS_ERR_SSL_WANT_WRITE:
        // Need to continue send the notify, don't close the mNetCtx fd.
        mState = State::kDisconnecting;
        break;

    case 0:
        // Succeed
        // Fall through

    default:
        mState = State::kDisconnected;
        if (mOnConnected != nullptr)
        {
            mOnConnected(*this, Error::kTransportFailed);
            mOnConnected = nullptr;
        }

        // Reset to the initial state before connecting.
        mSocket->Reset();
        Reset();
        break;
    }

exit:

    LOG_DEBUG("DTLS disconnected");
    return;
}

int DtlsSession::HandleMbedtlsExportKeys(void *               aDtlsSession,
                                         const unsigned char *aMasterSecret,
                                         const unsigned char *aKeyBlock,
                                         size_t               aMacLength,
                                         size_t               aKeyLength,
                                         size_t               aIvLength)
{
    auto dtlsSession = reinterpret_cast<DtlsSession *>(aDtlsSession);
    return dtlsSession->HandleMbedtlsExportKeys(aMasterSecret, aKeyBlock, aMacLength, aKeyLength, aIvLength);
}

int DtlsSession::HandleMbedtlsExportKeys(const unsigned char *,
                                         const unsigned char *aKeyBlock,
                                         size_t               aMacLength,
                                         size_t               aKeyLength,
                                         size_t               aIvLength)
{
    Sha256 sha256;

    sha256.Start();
    sha256.Update(aKeyBlock, 2 * static_cast<uint16_t>(aMacLength + aKeyLength + aIvLength));

    mKek.resize(Sha256::kHashSize);
    sha256.Finish(&mKek[0]);

    static_assert(Sha256::kHashSize >= kJoinerRouterKekLength, "Sha256::kHashSize >= kJoinerRouterKekLength");
    mKek.resize(kJoinerRouterKekLength);

    return 0;
}

void DtlsSession::HandleEvent(short aFlags)
{
    Error error = Error::kNone;

    if (mIsServer && !mIsClientIdSet)
    {
        SuccessOrExit(error = SetClientTransportId());
    }

    switch (mState)
    {
    case State::kConnecting:
        error = Handshake();
        if (error == Error::kTransportFailed || mState != State::kConnected)
        {
            break;
        }

        // Fall through

    case State::kConnected:
        if (aFlags & EV_READ)
        {
            while (error == Error::kNone)
            {
                error = Read();
            }
        }
        if ((aFlags & EV_WRITE) && error != Error::kTransportFailed)
        {
            error = TryWrite();
        }
        break;

    case State::kDisconnecting:
        Disconnect();
        break;

    default:
        ASSERT(false);
        break;
    }

exit:
    if (error == Error::kTransportFailed)
    {
        Disconnect();
    }
}

Error DtlsSession::SetClientTransportId()
{
    ASSERT(mIsServer && !mIsClientIdSet);
    Error error = Error::kNone;
    int   rval;
    auto  peerAddr = GetPeerAddr();

    VerifyOrExit(peerAddr.IsValid(), error = Error::kInvalidAddr);

    rval = mbedtls_ssl_set_client_transport_id(&mSsl, reinterpret_cast<const uint8_t *>(&peerAddr.GetRaw()[0]),
                                               peerAddr.GetRaw().size());
    VerifyOrExit(rval == 0, error = Error::kOutOfMemory);

    mIsClientIdSet = true;

exit:
    return error;
}

Error DtlsSession::Read()
{
    int   rval  = 0;
    Error error = Error::kNone;

    uint8_t buf[kMaxContentLength];

    VerifyOrExit(mState == State::kConnected, error = Error::kTransportBusy);

    rval = mbedtls_ssl_read(&mSsl, buf, sizeof(buf));

    if (rval > 0)
    {
        mReceiver(*this, {buf, buf + static_cast<size_t>(rval)});
        ExitNow(error = Error::kNone);
    }

    switch (rval)
    {
    case 0:
        // \c 0 if the read end of the underlying transport was closed
        // - in this case you must stop using the context (see below)
        error = Error::kTransportFailed;
        break;

    case MBEDTLS_ERR_SSL_WANT_READ:
    case MBEDTLS_ERR_SSL_WANT_WRITE:
    case MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS:
    case MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS:
        error = Error::kTransportBusy;
        break;

    case MBEDTLS_ERR_SSL_CLIENT_RECONNECT:
        // Continue reconnection
        mState = State::kConnecting;
        break;

    default:
        error = Error::kTransportFailed;
        break;
    }

exit:
    return error;
}

Error DtlsSession::Write(const ByteArray &aBuf)
{
    int   rval  = 0;
    Error error = Error::kNone;

    VerifyOrExit(mState == State::kConnected, error = Error::kTransportBusy);

    rval = mbedtls_ssl_write(&mSsl, &aBuf[0], aBuf.size());

    if (rval >= 0)
    {
        VerifyOrExit(static_cast<size_t>(rval) == aBuf.size(), error = Error::kInvalidArgs);

        LOG_DEBUG("DTLS successfully write data: {}", utils::Hex(aBuf));
        ExitNow(error = Error::kNone);
    }

    switch (rval)
    {
    case MBEDTLS_ERR_SSL_WANT_READ:
    case MBEDTLS_ERR_SSL_WANT_WRITE:
    case MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS:
    case MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS:
        error = Error::kTransportBusy;
        break;

    case MBEDTLS_ERR_SSL_BAD_INPUT_DATA:
        error = Error::kInvalidArgs;
        break;

    default:
        error = Error::kTransportFailed;
        break;
    }

exit:
    return error;
}

Error DtlsSession::TryWrite()
{
    Error error = Error::kNone;
    while (error == Error::kNone && !mSendQueue.empty())
    {
        error = Write(mSendQueue.front());
        if (error == Error::kNone)
        {
            mSendQueue.pop();
        }
        else
        {
            break;
        }
    }
    return error;
}

Error DtlsSession::Handshake()
{
    int   rval  = 0;
    Error error = Error::kNone;

    VerifyOrExit(mState == State::kConnecting);

    rval = mbedtls_ssl_handshake(&mSsl);

    if (mState == State::kConnecting && mSsl.state == MBEDTLS_SSL_HANDSHAKE_OVER)
    {
        mState = State::kConnected;
        if (mOnConnected != nullptr)
        {
            mOnConnected(*this, Error::kNone);
            mOnConnected = nullptr;
        }
    }

    switch (rval)
    {
    case 0:
        error = Error::kNone;
        break;

    case MBEDTLS_ERR_SSL_WANT_READ:
    case MBEDTLS_ERR_SSL_WANT_WRITE:
    case MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS:
    case MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS:
        error = Error::kTransportBusy;
        break;

    case MBEDTLS_ERR_SSL_HELLO_VERIFY_REQUIRED:
        Reconnect();
        error = Error::kNone;
        break;

    case MBEDTLS_ERR_SSL_TIMEOUT:
        LOG_ERROR("DTLS: Handshake timeout");

        // Fall through

    default:
        error = Error::kTransportFailed;
        break;
    }

exit:
    return error;
}

Error DtlsSession::Send(const ByteArray &aBuf)
{
    Error error = Error::kNone;

    VerifyOrExit(mState == State::kConnecting || mState == State::kConnected, error = Error::kInvalidState);

    if (mSendQueue.empty())
    {
        error = Write(aBuf);
        if (error == Error::kTransportFailed)
        {
            ExitNow();
        }
        else if (error == Error::kTransportBusy)
        {
            mSendQueue.emplace(aBuf);

            // hide Error::kTransportBusy from caller.
            error = Error::kNone;
        }
    }
    else
    {
        mSendQueue.emplace(aBuf);
    }

exit:
    return error;
}

int DtlsSession::DtlsTimer::GetDelay(void *aDtlsTimer)
{
    int  rval;
    auto timer = reinterpret_cast<const DtlsTimer *>(aDtlsTimer);

    if (timer->mCancelled)
    {
        rval = -1;
    }
    else if (!timer->IsRunning())
    {
        rval = 2;
    }
    else if (timer->mIntermediate <= Clock::now())
    {
        rval = 1;
    }
    else
    {
        rval = 0;
    }

    return rval;
}

void DtlsSession::DtlsTimer::SetDelay(void *aDtlsTimer, uint32_t aIntermediate, uint32_t aFinish)
{
    auto timer = reinterpret_cast<DtlsTimer *>(aDtlsTimer);

    if (aFinish == 0)
    {
        timer->mCancelled = true;
        timer->Stop();
    }
    else
    {
        timer->mCancelled = false;
        timer->Start(std::chrono::milliseconds(aFinish));
        timer->mIntermediate = Clock::now() + std::chrono::milliseconds(aIntermediate);
    }
}

void DtlsSession::HandshakeTimerCallback(Timer &)
{
    Error error = Error::kNone;

    switch (mState)
    {
    case State::kConnecting:
        error = Handshake();
        break;

    case State::kConnected:
    case State::kDisconnecting:
    default:
        ASSERT(false);
    }

    if (error == Error::kTransportFailed)
    {
        Disconnect();
    }
}

} // namespace commissioner

} // namespace ot
