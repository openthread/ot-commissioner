/*
 *  Copyright (c) 2019, The OpenThread Commissioner Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file implements the Commissioner Token Manager.
 */

#if OT_COMM_CONFIG_CCM_ENABLE

#include "library/token_manager.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "commissioner/commissioner.hpp"
#include "commissioner/defines.hpp"
#include "commissioner/error.hpp"
#include "common/error_macros.hpp"
#include "common/utils.hpp"
#include "event2/event.h"
#include "library/cbor.hpp"
#include "library/coap.hpp"
#include "library/cose.hpp"
#include "library/cwt.hpp"
#include "library/dtls.hpp"
#include "library/mbedtls_error.hpp"
#include "library/tlv.hpp"
#include "library/uri.hpp"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/pk.h"
#include "mbedtls/x509_crt.h"

namespace ot {

namespace commissioner {

TokenManager::TokenManager(struct event_base *aEventBase)
    : mRegistrarClient(aEventBase)
{
    mbedtls_entropy_init(&mEntropy);
    mbedtls_ctr_drbg_init(&mCtrDrbg);
    mbedtls_ctr_drbg_seed(&mCtrDrbg, mbedtls_entropy_func, &mEntropy, nullptr, 0);
    mbedtls_pk_init(&mPublicKey);
    mbedtls_pk_init(&mPrivateKey);
    mbedtls_pk_init(&mDomainCAPublicKey);
}

TokenManager::~TokenManager()
{
    mbedtls_entropy_free(&mEntropy);
    mbedtls_ctr_drbg_free(&mCtrDrbg);
    mbedtls_pk_free(&mPrivateKey);
    mbedtls_pk_free(&mPublicKey);
    mbedtls_pk_free(&mDomainCAPublicKey);
}

Error TokenManager::Init(const Config &aConfig)
{
    Error              error;
    mbedtls_pk_context publicKey;
    mbedtls_pk_context privateKey;
    mbedtls_pk_context trustAnchorPublicKey;

    mbedtls_pk_init(&publicKey);
    mbedtls_pk_init(&privateKey);
    mbedtls_pk_init(&trustAnchorPublicKey);

    SuccessOrExit(error = ParsePublicKey(publicKey, aConfig.mCertificate));
    SuccessOrExit(error = ParsePrivateKey(privateKey, aConfig.mPrivateKey));
    SuccessOrExit(error = ParsePublicKey(trustAnchorPublicKey, aConfig.mTrustAnchor));

    SuccessOrExit(error = mRegistrarClient.Init(GetDtlsConfig(aConfig)));

    mCommissionerId = aConfig.mId;
    mDomainName     = aConfig.mDomainName;

    // Move mbedtls keys
    MoveMbedtlsKey(mPublicKey, publicKey);
    MoveMbedtlsKey(mPrivateKey, privateKey);
    MoveMbedtlsKey(mDomainCAPublicKey, trustAnchorPublicKey);

exit:
    mbedtls_pk_free(&trustAnchorPublicKey);
    mbedtls_pk_free(&privateKey);
    mbedtls_pk_free(&publicKey);
    return error;
}

Error TokenManager::ValidateToken(CborMap                  &aToken,
                                  const ByteArray          &aSignedToken,
                                  const mbedtls_pk_context &aPublicKey) const
{
    Error              error;
    cose::Sign1Message coseSign;
    CborMap            token;
    const uint8_t     *payload;
    size_t             payloadLength;
    const char        *domainName;
    size_t             domainNameLength;
    const char        *expire;
    size_t             expireLength;

    VerifyOrExit(!aSignedToken.empty(), error = ERROR_INVALID_ARGS("the signed COM_TOK is empty"));
    SuccessOrExit(error = cose::Sign1Message::Deserialize(coseSign, aSignedToken));

    SuccessOrExit(error = coseSign.Validate(aPublicKey));

    VerifyOrExit((payload = coseSign.GetPayload(payloadLength)) != nullptr,
                 error = ERROR_BAD_FORMAT("cannot find payload in the signed COM_TOK"));

    SuccessOrExit(error = CborValue::Deserialize(token, payload, payloadLength));

    SuccessOrExit(error = token.Get(cwt::kAud, domainName, domainNameLength));

    SuccessOrExit(error = token.Get(cwt::kExp, expire, expireLength));

    // TODO(wgtdkp): make sure it is not expired

    // Ignore the "iss" (issuer) claim

    if (std::string{domainName, domainNameLength} != mDomainName)
    {
        ExitNow(error = ERROR_SECURITY("the Domain Name ({}) in COM_TOK doesn't match the configured Domain Name ({})",
                                       std::string{domainName, domainNameLength}, mDomainName));
    }

    CborValue::Move(aToken, token);

exit:
    token.Free();
    coseSign.Free();
    return error;
}

void TokenManager::RequestToken(Commissioner::Handler<ByteArray> aHandler, const std::string &aAddr, uint16_t aPort)
{
    auto onConnected = [this, aHandler](const DtlsSession &, Error aError) {
        if (aError != ErrorCode::kNone)
        {
            aHandler(nullptr, aError);
        }
        else
        {
            SendTokenRequest(aHandler);
        }
    };

    mRegistrarClient.Connect(onConnected, aAddr, aPort);
}

void TokenManager::SendTokenRequest(Commissioner::Handler<ByteArray> aHandler)
{
    Error     error;
    ByteArray tokenRequest;

    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};

    auto onResponse = [this, aHandler](const coap::Response *aResponse, Error aError) {
        Error               error;
        coap::ContentFormat contentFormat;

        SuccessOrExit(error = aError);
        VerifyOrDie(aResponse != nullptr);

        VerifyOrExit(aResponse->GetCode() == coap::Code::kChanged,
                     error = ERROR_BAD_FORMAT("expect response code as CoAP::CHANGED, but got {}",
                                              static_cast<int>(aResponse->GetCode())));
        VerifyOrExit(aResponse->GetContentFormat(contentFormat) == ErrorCode::kNone,
                     error = ERROR_BAD_FORMAT("cannot find valid CoAP Content Format option"));
        VerifyOrExit(
            contentFormat == coap::ContentFormat::kCoseSign1,
            error = ERROR_BAD_FORMAT("CoAP Content Format requires to be application/cose; cose-type=\"cose-sign1\""));

        SuccessOrExit(error = SetToken(aResponse->GetPayload()));

    exit:
        if (error != ErrorCode::kNone)
        {
            aHandler(nullptr, error);
        }
        else
        {
            aHandler(&mSignedToken, error);
        }

        // Disconnect from the registrar.
        mRegistrarClient.Disconnect(ERROR_NONE);
    };

    SuccessOrExit(error = request.SetUriPath(uri::kComToken));
    SuccessOrExit(error = request.SetContentFormat(coap::ContentFormat::kCWT));

    SuccessOrExit(error = MakeTokenRequest(tokenRequest, mPublicKey, mCommissionerId, mDomainName));
    request.Append(tokenRequest);
    mRegistrarClient.SendRequest(request, onResponse);

exit:
    if (error != ErrorCode::kNone)
    {
        aHandler(nullptr, error);
    }
}

Error TokenManager::SetToken(const ByteArray &aSignedToken, bool aAlwaysAccept)
{
    Error error;

    (void)aAlwaysAccept;
    error = ValidateToken(aSignedToken, mDomainCAPublicKey);

#if OT_COMM_CONFIG_REFERENCE_DEVICE_ENABLE
    if (aAlwaysAccept)
    {
        if (error != ErrorCode::kNone)
        {
            LOG_WARN(LOG_REGION_TOKEN_MANAGER, "validating token failed: {}", error.ToString());
        }
        SuccessOrExit(error = ERROR_NONE);
    }
    else
#endif
    {
        SuccessOrExit(error);
    }

    mSignedToken    = aSignedToken;
    mSequenceNumber = 0;
exit:
    return error;
}

Error TokenManager::ValidateToken(const ByteArray &aSignedToken, const mbedtls_pk_context &aPublicKey) const
{
    Error     error;
    CborMap   token;
    ByteArray keyId;

    SuccessOrExit(error = ValidateToken(token, aSignedToken, aPublicKey));
    SuccessOrExit(error = GetKeyId(keyId, token));

exit:
    token.Free();
    return error;
}

Error TokenManager::GetKeyId(ByteArray &aKeyId, const CborMap &aToken) const
{
    Error          error;
    CborMap        cnf;
    CborMap        coseKey;
    const uint8_t *keyId = nullptr;
    size_t         keyIdLength;

    SuccessOrExit(error = aToken.Get(cwt::kCnf, cnf));
    SuccessOrExit(error = cnf.Get(cwt::kCoseKey, coseKey));
    SuccessOrExit(error = coseKey.Get(cose::kKeyId, keyId, keyIdLength));

    aKeyId = {keyId, keyId + keyIdLength};

exit:
    return error;
}

Error TokenManager::GetKeyId(ByteArray &aKeyId) const
{
    Error   error;
    CborMap token;

    SuccessOrExit(error = ValidateToken(token, mSignedToken, mDomainCAPublicKey));
    SuccessOrExit(error = GetKeyId(aKeyId, token));

exit:
    token.Free();
    return error;
}

Error TokenManager::MakeTokenRequest(ByteArray                &aBuf,
                                     const mbedtls_pk_context &aPublicKey,
                                     const std::string        &aId,
                                     const std::string        &aDomainName)
{
    static constexpr size_t kMaxTokenRequestSize = 1024;

    Error     error;
    CborMap   tokenRequest;
    CborMap   reqCnf;
    ByteArray encodedCoseKey;
    CborMap   coseKey;
    size_t    encodedLength = 0;
    uint8_t   tokenBuf[kMaxTokenRequestSize];

    // Use the commissioner Id as kid(truncated to kMaxCoseKeyIdLength)
    const ByteArray kid = {aId.begin(), aId.begin() + std::min(aId.size(), static_cast<size_t>(kMaxCoseKeyIdLength))};

    VerifyOrExit(mbedtls_pk_can_do(&aPublicKey, MBEDTLS_PK_ECDSA),
                 error = ERROR_INVALID_ARGS("the public key is not a ECDSA key"));
    VerifyOrExit(!aId.empty(), error = ERROR_INVALID_ARGS("the ID is empty"));
    VerifyOrExit(!aDomainName.empty(), error = ERROR_INVALID_ARGS("the Domain Name is empty"));

    SuccessOrExit(error = tokenRequest.Init());

    // CWT grant type = CLIENT_CRED
    SuccessOrExit(error = tokenRequest.Put(cwt::kGrantType, cwt::kGrantTypeClientCred));

    // CWT client id
    SuccessOrExit(error = tokenRequest.Put(cwt::kClientId, aId.c_str()));

    // CWT request audience
    SuccessOrExit(error = tokenRequest.Put(cwt::kAud, aDomainName.c_str()));

    // CWT req_cnf
    SuccessOrExit(error = reqCnf.Init());
    SuccessOrExit(error = cose::MakeCoseKey(encodedCoseKey, aPublicKey, kid));
    SuccessOrExit(error = CborMap::Deserialize(coseKey, &encodedCoseKey[0], encodedCoseKey.size()));
    SuccessOrExit(error = reqCnf.Put(cwt::kCoseKey, coseKey));
    SuccessOrExit(error = tokenRequest.Put(cwt::kReqCnf, reqCnf));

    SuccessOrExit(error = tokenRequest.Serialize(tokenBuf, encodedLength, sizeof(tokenBuf)));

    aBuf.assign(tokenBuf, tokenBuf + encodedLength);

exit:
    coseKey.Free();
    reqCnf.Free();
    tokenRequest.Free();
    return error;
}

static inline bool ShouldBeSerialized(tlv::Type aTlvType, bool aIsActiveSet, bool aIsPendingSet)
{
    if (aIsPendingSet)
    {
        // Delay Timer TLV is excluded from signing
        return aTlvType != tlv::Type::kDelayTimer && tlv::IsDatasetParameter(false, aTlvType);
    }

    if (aIsActiveSet)
    {
        return tlv::IsDatasetParameter(true, aTlvType);
    }

    return aTlvType != tlv::Type::kCommissionerToken && aTlvType != tlv::Type::kCommissionerSignature &&
           aTlvType != tlv::Type::kCommissionerPenSignature && aTlvType != tlv::Type::kThreadCommissionerToken &&
           aTlvType != tlv::Type::kThreadCommissionerSignature;
}

Error TokenManager::SignMessage(ByteArray &aSignature, const coap::Message &aMessage)
{
    Error              error;
    ByteArray          externalData;
    cose::Sign1Message sign1Msg;
    ByteArray          keyId;

    SuccessOrExit(error = PrepareSigningContent(externalData, aMessage));

    SuccessOrExit(error = sign1Msg.Init(cose::kInitFlagsNone));
    SuccessOrExit(error = sign1Msg.AddAttribute(cose::kHeaderAlgorithm, cose::kAlgEcdsaWithSha256, cose::kProtectOnly));
#if OT_COMM_CONFIG_REFERENCE_DEVICE_ENABLE
    // In case of getting Key ID failed, Use all-zero value by default.
    keyId.resize(kMaxCoseKeyIdLength);
    std::fill(keyId.begin(), keyId.end(), 0);
    IgnoreError(GetKeyId(keyId));
#else
    SuccessOrExit(error = GetKeyId(keyId));
#endif
    SuccessOrExit(error = sign1Msg.AddAttribute(cose::kHeaderKeyId, keyId, cose::kUnprotectOnly));

    // TODO(wgtdkp): set cose::kHeaderIV to mSequenceNumber.
    // SuccessOrExit(error = sign1Msg.AddAttribute(cose::kHeaderIV, , cose::kProtectOnly));
    SuccessOrExit(error = sign1Msg.SetContent({}));

    // The serialized message as external data for COSE signing.
    SuccessOrExit(error = sign1Msg.SetExternalData(externalData));

    SuccessOrExit(error = sign1Msg.Sign(mPrivateKey));
    SuccessOrExit(error = sign1Msg.Serialize(aSignature));

    // TODO(wgtdkp): synchronize to persist store.
    ++mSequenceNumber;

exit:
    sign1Msg.Free();
    return error;
}

Error TokenManager::PrepareSigningContent(ByteArray &aContent, const coap::Message &aMessage)
{
    Error         error;
    coap::Message message{aMessage.GetType(), aMessage.GetCode()};
    tlv::TlvSet   tlvSet;
    std::string   signingUri;
    bool          isActiveSet  = false;
    bool          isPendingSet = false;
    ByteArray     content;

    VerifyOrExit(aMessage.GetUriPath(signingUri) == ErrorCode::kNone,
                 error = ERROR_INVALID_ARGS("the CoAP message has no valid URI Path option"));

    isActiveSet  = signingUri == uri::kMgmtActiveSet;
    isPendingSet = signingUri == uri::kMgmtPendingSet;

    if (signingUri == uri::kPetitioning)
    {
        signingUri = uri::kLeaderPetitioning;
    }
    else if (signingUri == uri::kKeepAlive)
    {
        signingUri = uri::kLeaderKeepAlive;
    }

    // Prepare serialized URI
    SuccessOrExit(error = message.SetUriPath(signingUri));
    SuccessOrExit(error = message.Serialize(content));
    content.erase(content.begin(), content.begin() + message.GetHeaderLength());

    // Sort and serialize TLVs
    SuccessOrExit(error = tlv::GetTlvSet(tlvSet, aMessage.GetPayload()));
    for (const auto &tlv : tlvSet)
    {
        if (ShouldBeSerialized(tlv.first, isActiveSet, isPendingSet))
        {
            tlv.second->Serialize(content);
        }
    }

    aContent = std::move(content);

exit:
    return error;
}

Error TokenManager::GetPublicKeyInToken(ByteArray &aPublicKey) const
{
    static constexpr size_t kMaxSignedTokenLength = 1024;

    Error   error;
    CborMap token;
    CborMap cnf;
    CborMap publicKey;
    size_t  publicKeyLength;

    SuccessOrExit(error = ValidateToken(token, mSignedToken, mDomainCAPublicKey));
    SuccessOrExit(error = token.Get(cwt::kCnf, cnf));
    SuccessOrExit(error = cnf.Get(cwt::kCoseKey, publicKey));

    aPublicKey.resize(kMaxSignedTokenLength);
    SuccessOrDie(publicKey.Serialize(aPublicKey.data(), publicKeyLength, aPublicKey.size()));
    aPublicKey.resize(publicKeyLength);

exit:
    token.Free();
    return error;
}

Error TokenManager::ValidateSignature(const ByteArray &aSignature, const coap::Message &aSignedMessage)
{
    Error              error;
    ByteArray          externalData;
    cose::Sign1Message sign1Msg;
    ByteArray          rawPublicKeyInToken;
    CborMap            publicKeyInToken;

    VerifyOrExit(!aSignature.empty(), error = ERROR_INVALID_ARGS("the signature is empty"));
    SuccessOrExit(error = cose::Sign1Message::Deserialize(sign1Msg, aSignature));

    SuccessOrExit(error = PrepareSigningContent(externalData, aSignedMessage));
    SuccessOrExit(error = sign1Msg.SetExternalData(externalData));
    SuccessOrExit(error = sign1Msg.Validate(mPublicKey));
    SuccessOrExit(error = GetPublicKeyInToken(rawPublicKeyInToken));
    SuccessOrExit(error =
                      CborValue::Deserialize(publicKeyInToken, rawPublicKeyInToken.data(), rawPublicKeyInToken.size()));
    SuccessOrExit(error = sign1Msg.Validate(publicKeyInToken));

exit:
    sign1Msg.Free();
    return error;
}

Error TokenManager::ParsePublicKey(mbedtls_pk_context &aPublicKey, const ByteArray &aCert)
{
    Error            error;
    mbedtls_x509_crt cert;

    mbedtls_x509_crt_init(&cert);

    VerifyOrExit(!aCert.empty(), error = ERROR_INVALID_ARGS("the raw certificate is empty"));
    if (int fail = mbedtls_x509_crt_parse(&cert, aCert.data(), aCert.size()))
    {
        error = ErrorFromMbedtlsError(fail);
        ExitNow(error = {ErrorCode::kInvalidArgs, error.GetMessage()});
    }

    // Steal the public key from the 'cert' object.
    MoveMbedtlsKey(aPublicKey, cert.pk);

exit:
    mbedtls_x509_crt_free(&cert);

    return error;
}

Error TokenManager::ParsePrivateKey(mbedtls_pk_context &aPrivateKey, const ByteArray &aPrivateKeyRaw)
{
    Error error;

    VerifyOrExit(!aPrivateKeyRaw.empty(), error = ERROR_INVALID_ARGS("the raw private key is empty"));
    if (int fail = mbedtls_pk_parse_key(&aPrivateKey, aPrivateKeyRaw.data(), aPrivateKeyRaw.size(), nullptr, 0,
                                        mbedtls_ctr_drbg_random, &mCtrDrbg))
    {
        error = ErrorFromMbedtlsError(fail);
        ExitNow(error = {ErrorCode::kInvalidArgs, error.GetMessage()});
    }

exit:
    return error;
}

void TokenManager::MoveMbedtlsKey(mbedtls_pk_context &aDes, mbedtls_pk_context &aSrc)
{
    mbedtls_pk_free(&aDes);
    aDes = aSrc;
    mbedtls_pk_init(&aSrc);
}

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_CONFIG_CCM_ENABLE
