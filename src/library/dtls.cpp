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

#include "library/dtls.hpp"

#include <mbedtls/debug.h>
#include <mbedtls/error.h>
#include <mbedtls/platform.h>

#include "library/logging.hpp"
#include "library/openthread/sha256.hpp"

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
        LOG_CRIT(LOG_REGION_MBEDTLS, "{}, {}: {}", file, line, str);
        break;

    case 2:
        LOG_WARN(LOG_REGION_MBEDTLS, "{}, {}: {}", file, line, str);
        break;

    case 3:
        LOG_INFO(LOG_REGION_MBEDTLS, "{}, {}: {}", file, line, str);
        break;

    case 4:
    default:
        LOG_DEBUG(LOG_REGION_MBEDTLS, "{}, {}: {}", file, line, str);
        break;
    }
}

/**
 * This function convert mbedtls error to OT Commissioner error.
 *
 * For the implementation details, please reference to <mbedtls/error.h>.
 *
 */
static Error ErrorFromMbedtlsError(int aMbedtlsError)
{
    // See <mbedtls/error.h> for the constants.
    static constexpr int kMbedtlsErrorLowLevelNetBegin        = -0x0052;
    static constexpr int kMbedtlsErrorLowLevelNetEnd          = -0x0042;
    static constexpr int kMbedtlsErrorHighLevelModuleIdMask   = 0x7000;
    static constexpr int kMbedtlsErrorHighLevelModuleIdOffset = 12;
    static constexpr int kMbedtlsErrorHighLevelModuleIdCipher = 6;
    static constexpr int kMbedtlsErrorHighLevelModuleIdSsl    = 7;

    VerifyOrDie(aMbedtlsError <= 0);

    Error error;

    uint16_t highLevelModuleId = (static_cast<uint16_t>(-aMbedtlsError) & kMbedtlsErrorHighLevelModuleIdMask) >>
                                 kMbedtlsErrorHighLevelModuleIdOffset;

    if (aMbedtlsError == 0)
    {
        error = Error::kNone;
    }
    else if (aMbedtlsError == MBEDTLS_ERR_SSL_WANT_READ || aMbedtlsError == MBEDTLS_ERR_SSL_WANT_WRITE ||
             aMbedtlsError == MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS || aMbedtlsError == MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS)
    {
        error = Error::kTransportBusy;
    }
    else if (aMbedtlsError >= kMbedtlsErrorLowLevelNetBegin && aMbedtlsError <= kMbedtlsErrorLowLevelNetEnd)
    {
        // Low-level NET error.
        error = Error::kTransportFailed;
    }
    else if (highLevelModuleId == kMbedtlsErrorHighLevelModuleIdCipher ||
             highLevelModuleId == kMbedtlsErrorHighLevelModuleIdSsl)
    {
        // High-level SSL or CIPHER error.
        error = Error::kSecurity;
    }
    else
    {
        error = Error::kFailed;
    }

    return error;
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
    return ErrorFromMbedtlsError(rval);
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
    VerifyOrDie(mState == State::kOpen);

    mOnConnected = aOnConnected;
    mState       = State::kConnecting;
}

void DtlsSession::Reconnect()
{
    VerifyOrDie(mIsServer);
    Reset();
    Connect(mOnConnected);
}

void DtlsSession::Disconnect(Error aError)
{
    VerifyOrExit(mState == State::kConnecting || mState == State::kConnected);

    // Send close notify if the connected session is aborted by user.
    if (mState == State::kConnected && aError == Error::kAbort)
    {
        // We don't care if the notify has been successfully delivered.
        mbedtls_ssl_close_notify(&mSsl);
    }

    mState = State::kDisconnected;
    if (mOnConnected != nullptr)
    {
        mOnConnected(*this, aError);
        mOnConnected = nullptr;
    }

    // Reset to the initial state.
    mSocket->Reset();
    Reset();

    LOG_DEBUG(LOG_REGION_DTLS, "session={} disconnected", static_cast<void *>(this));

exit:
    return;
}

std::string DtlsSession::GetStateString() const
{
    std::string stateString = "UNKNOWN";
    switch (mState)
    {
    case State::kOpen:
        stateString = "OPEN";
        break;
    case State::kConnecting:
        stateString = "CONNECTING";
        break;
    case State::kConnected:
        stateString = "CONNECTED";
        break;
    case State::kDisconnected:
        stateString = "DISCONNECTED";
        break;
    }
    return stateString;
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
        if (ShouldStop(error) || mState != State::kConnected)
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
        if ((aFlags & EV_WRITE) && !ShouldStop(error))
        {
            error = TryWrite();
        }
        break;

    default:
        VerifyOrDie(false);
        break;
    }

exit:
    if (ShouldStop(error))
    {
        Disconnect(error);
    }
}

Error DtlsSession::SetClientTransportId()
{
    VerifyOrDie(mIsServer && !mIsClientIdSet);
    Error error    = Error::kNone;
    int   rval     = 0;
    auto  peerAddr = GetPeerAddr();

    VerifyOrExit(peerAddr.IsValid(), error = Error::kInvalidAddr);

    rval = mbedtls_ssl_set_client_transport_id(&mSsl, reinterpret_cast<const uint8_t *>(&peerAddr.GetRaw()[0]),
                                               peerAddr.GetRaw().size());
    SuccessOrExit(error = ErrorFromMbedtlsError(rval));

    mIsClientIdSet = true;

exit:
    return error;
}

bool DtlsSession::ShouldStop(Error aError)
{
    return aError != Error::kNone && aError != Error::kTransportBusy;
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
        LOG_DEBUG(LOG_REGION_DTLS, "session(={}) successfully read data: {}", static_cast<void *>(this),
                  utils::Hex({buf, buf + rval}));
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

    case MBEDTLS_ERR_SSL_CLIENT_RECONNECT:
        // Continue reconnection
        mState = State::kConnecting;
        break;

    default:
        error = ErrorFromMbedtlsError(rval);
        break;
    }

exit:
    return error;
}

Error DtlsSession::Write(const ByteArray &aBuf, MessageSubType aSubType)
{
    int   rval  = 0;
    Error error = Error::kNone;

    VerifyOrExit(mState == State::kConnected, error = Error::kTransportBusy);

    mSocket->SetSubType(aSubType);
    rval = mbedtls_ssl_write(&mSsl, &aBuf[0], aBuf.size());
    mSocket->SetSubType(MessageSubType::kNone);

    if (rval >= 0)
    {
        VerifyOrExit(static_cast<size_t>(rval) == aBuf.size(), error = Error::kInvalidArgs);

        LOG_DEBUG(LOG_REGION_DTLS, "session(={}) successfully write data: {}", static_cast<void *>(this),
                  utils::Hex(aBuf));
        ExitNow(error = Error::kNone);
    }

    error = ErrorFromMbedtlsError(rval);

exit:
    return error;
}

Error DtlsSession::TryWrite()
{
    Error error = Error::kNone;
    while (error == Error::kNone && !mSendQueue.empty())
    {
        auto &messagePair = mSendQueue.front();
        error             = Write(messagePair.first, messagePair.second);
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
    case MBEDTLS_ERR_SSL_HELLO_VERIFY_REQUIRED:
        Reconnect();
        error = Error::kNone;
        break;

    case MBEDTLS_ERR_SSL_TIMEOUT:
        LOG_ERROR(LOG_REGION_DTLS, "session(={}) handshake timeout", static_cast<void *>(this));

        // Fall through

    default:
        error = ErrorFromMbedtlsError(rval);
        break;
    }

exit:
    return error;
}

Error DtlsSession::Send(const ByteArray &aBuf, MessageSubType aSubType)
{
    Error error = Error::kNone;

    VerifyOrExit(mState == State::kConnecting || mState == State::kConnected, error = Error::kInvalidState);

    if (mSendQueue.empty())
    {
        error = Write(aBuf, aSubType);
        if (error == Error::kTransportBusy)
        {
            mSendQueue.emplace(aBuf, aSubType);

            // hide Error::kTransportBusy from caller.
            error = Error::kNone;
        }
    }
    else
    {
        mSendQueue.emplace(aBuf, aSubType);
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
    default:
        VerifyOrDie(false);
    }

    if (ShouldStop(error))
    {
        Disconnect(error);
    }
}

} // namespace commissioner

} // namespace ot
