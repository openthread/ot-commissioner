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

#include "library/token_manager.hpp"

#include <assert.h>

#include <mbedtls/x509_crt.h>

#include "library/cose.hpp"
#include "library/cwt.hpp"
#include "library/logging.hpp"
#include "library/tlv.hpp"
#include "library/uri.hpp"

namespace ot {

namespace commissioner {

TokenManager::TokenManager(struct event_base *aEventBase)
    : mRegistrarClient(aEventBase)
{
    mbedtls_pk_init(&mPublicKey);
    mbedtls_pk_init(&mPrivateKey);
    mbedtls_pk_init(&mDomainCAPublicKey);
}

TokenManager::~TokenManager()
{
    mbedtls_pk_free(&mPrivateKey);
    mbedtls_pk_free(&mPublicKey);
    mbedtls_pk_free(&mDomainCAPublicKey);
}

Error TokenManager::Init(const Config &aConfig)
{
    Error              error = Error::kNone;
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

    error = Error::kNone;

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

Error TokenManager::VerifyToken(CborMap &aToken, const ByteArray &aSignedToken, const mbedtls_pk_context &aPublicKey)
{
    Error              error = Error::kSecurity;
    cose::Sign1Message coseSign;
    CborMap            token;
    const uint8_t *    payload;
    size_t             payloadLength;
    const char *       domainName;
    size_t             domainNameLength;
    const char *       expire;
    size_t             expireLength;

    LOG_INFO(LOG_REGION_TOKEN_MANAGER, "received token, length = {}, {}", aSignedToken.size(),
             utils::Hex(aSignedToken));

    VerifyOrExit(!aSignedToken.empty(), error = Error::kInvalidArgs);
    SuccessOrExit(error = cose::Sign1Message::Deserialize(coseSign, aSignedToken));

    SuccessOrExit(error = coseSign.Validate(aPublicKey));

    VerifyOrExit((payload = coseSign.GetPayload(payloadLength)) != nullptr);

    SuccessOrExit(error = CborValue::Deserialize(token, payload, payloadLength));

    SuccessOrExit(error = token.Get(cwt::kAud, domainName, domainNameLength));

    SuccessOrExit(error = token.Get(cwt::kExp, expire, expireLength));

    // TODO(wgtdkp): make sure it is not expired

    // Ignore the "iss" (issuer) claim

    VerifyOrExit((mDomainName == std::string{domainName, domainNameLength}), error = Error::kSecurity);

    error = Error::kNone;
    CborValue::Move(aToken, token);

exit:
    token.Free();
    coseSign.Free();
    return error;
}

void TokenManager::RequestToken(Commissioner::Handler<ByteArray> aHandler, const std::string &aAddr, uint16_t aPort)
{
    auto onConnected = [this, aHandler](const DtlsSession &, Error aError) {
        if (aError != Error::kNone)
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
    Error     error = Error::kNone;
    ByteArray tokenRequest;

    coap::Request request{coap::Type::kConfirmable, coap::Code::kPost};

    auto onResponse = [this, aHandler](const coap::Response *aResponse, Error aError) {
        Error               error = Error::kFailed;
        coap::ContentFormat contentFormat;

        SuccessOrExit(error = aError);
        VerifyOrDie(aResponse != nullptr);

        VerifyOrExit(aResponse->GetCode() == coap::Code::kChanged, error = Error::kFailed);
        VerifyOrExit(aResponse->GetContentFormat(contentFormat) == Error::kNone, error = Error::kBadFormat);
        VerifyOrExit(contentFormat == coap::ContentFormat::kCoseSign1, error = Error::kBadFormat);

        SuccessOrExit(error = SetToken(aResponse->GetPayload(), mDomainCAPublicKey));

    exit:
        if (error != Error::kNone)
        {
            aHandler(nullptr, error);
        }
        else
        {
            aHandler(&mSignedToken, error);
        }

        // Disconnect from the registrar.
        mRegistrarClient.Disconnect();
    };

    SuccessOrExit(error = request.SetUriPath(uri::kComToken));
    SuccessOrExit(error = request.SetContentFormat(coap::ContentFormat::kCWT));

    SuccessOrExit(error = MakeTokenRequest(tokenRequest, mPublicKey, mCommissionerId, mDomainName));
    request.Append(tokenRequest);
    mRegistrarClient.SendRequest(request, onResponse);

exit:
    if (error != Error::kNone)
    {
        aHandler(nullptr, error);
    }
}

Error TokenManager::SetToken(const ByteArray &aSignedToken, const ByteArray &aCert)
{
    Error              error = Error::kNone;
    mbedtls_pk_context publicKey;

    mbedtls_pk_init(&publicKey);

    // TODO(wgtdkp): verify the certificate with our trust anchor?
    SuccessOrExit(error = ParsePublicKey(publicKey, aCert));
    SuccessOrExit(error = SetToken(aSignedToken, publicKey));

exit:
    mbedtls_pk_free(&publicKey);
    return error;
}

Error TokenManager::SetToken(const ByteArray &aSignedToken, const mbedtls_pk_context &aPublicKey)
{
    Error          error = Error::kNone;
    CborMap        token;
    CborMap        cnf;
    CborMap        coseKey;
    const uint8_t *kid       = nullptr;
    size_t         kidLength = 0;

    SuccessOrExit(error = VerifyToken(token, aSignedToken, aPublicKey));
    SuccessOrExit(error = token.Get(cwt::kCnf, cnf));
    SuccessOrExit(error = cnf.Get(cwt::kCoseKey, coseKey));
    SuccessOrExit(error = coseKey.Get(cose::kKeyId, kid, kidLength));

    mSignedToken = aSignedToken;

    // We need to re-extract the token into a CBOR object, because a CBOR object
    // directly reference to raw data in the signed-token buffer.
    error = VerifyToken(mToken, mSignedToken, aPublicKey);
    assert(error == Error::kNone);

    // The mSequenceNumber is always associated with mToken & mSignedToken.
    mSequenceNumber = 0;
    mKeyId          = {kid, kid + kidLength};

exit:
    return error;
}

Error TokenManager::MakeTokenRequest(ByteArray &               aBuf,
                                     const mbedtls_pk_context &aPublicKey,
                                     const std::string &       aId,
                                     const std::string &       aDomainName)
{
    static const size_t kMaxTokenRequestSize = 1024;

    Error     error = Error::kNone;
    CborMap   tokenRequest;
    CborMap   reqCnf;
    ByteArray encodedCoseKey;
    CborMap   coseKey;
    size_t    encodedLength = 0;

    // Use the commissioner Id as kid(truncated to kMaxCoseKeyIdLength)
    const ByteArray kid = {aId.begin(), aId.begin() + std::min(aId.size(), (size_t)kMaxCoseKeyIdLength)};

    VerifyOrExit(mbedtls_pk_can_do(&aPublicKey, MBEDTLS_PK_ECDSA), error = Error::kInvalidArgs);
    VerifyOrExit(!aId.empty() && !aDomainName.empty(), error = Error::kInvalidArgs);

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

    aBuf.resize(kMaxTokenRequestSize);
    SuccessOrExit(error = tokenRequest.Serialize(&aBuf[0], encodedLength, aBuf.size()));
    aBuf.resize(encodedLength);

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
    Error              error = Error::kNone;
    ByteArray          externalData;
    cose::Sign1Message sign1Msg;

    VerifyOrExit(IsValid(), error = Error::kInvalidState);

    SuccessOrExit(error = PrepareSigningContent(externalData, aMessage));

    SuccessOrExit(error = sign1Msg.Init(cose::kInitFlagsNone));
    SuccessOrExit(error = sign1Msg.AddAttribute(cose::kHeaderAlgorithm, cose::kAlgEcdsaWithSha256, cose::kProtectOnly));
    SuccessOrExit(error = sign1Msg.AddAttribute(cose::kHeaderKeyId, mKeyId, cose::kUnprotectOnly));

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
    Error         error = Error::kNone;
    coap::Message message{aMessage.GetType(), aMessage.GetCode()};
    tlv::TlvSet   tlvSet;
    std::string   uri;
    bool          isActiveSet  = false;
    bool          isPendingSet = false;
    ByteArray     content;

    VerifyOrExit(aMessage.GetUriPath(uri) == Error::kNone, error = Error::kInvalidArgs);

    isActiveSet  = uri == uri::kMgmtActiveSet;
    isPendingSet = uri == uri::kMgmtPendingSet;

    // Prepare serialized URI
    SuccessOrExit(error = message.SetUriPath(uri));
    SuccessOrExit(error = message.Serialize(content));
    content.erase(content.begin(), content.begin() + message.GetHeaderLength());

    // Sort and serialize TLVs
    SuccessOrExit(error = tlv::GetTlvSet(tlvSet, aMessage.GetPayload()));
    for (const auto &tlv : tlvSet)
    {
        if (ShouldBeSerialized(tlv.first, isActiveSet, isPendingSet))
        {
            SuccessOrExit(error = tlv.second->Serialize(content));
        }
    }

    error    = Error::kNone;
    aContent = std::move(content);

exit:
    return error;
}

Error TokenManager::GetPublicKey(CborMap &aPublicKey) const
{
    Error   error = Error::kNone;
    CborMap cnf;

    VerifyOrExit(mToken.IsValid(), error = Error::kInvalidState);
    SuccessOrExit(error = mToken.Get(cwt::kCnf, cnf));
    SuccessOrExit(error = cnf.Get(cwt::kCoseKey, aPublicKey));

exit:
    return error;
}

Error TokenManager::VerifySignature(const ByteArray &aSignature, const coap::Message &aSignedMessage)
{
    Error              error;
    ByteArray          externalData;
    cose::Sign1Message sign1Msg;
    CborMap            publicKey;

    SuccessOrExit(error = cose::Sign1Message::Deserialize(sign1Msg, aSignature));

    SuccessOrExit(error = PrepareSigningContent(externalData, aSignedMessage));
    SuccessOrExit(error = sign1Msg.SetExternalData(externalData));
    SuccessOrExit(error = sign1Msg.Validate(mPublicKey));
    SuccessOrExit(error = GetPublicKey(publicKey));
    SuccessOrExit(error = sign1Msg.Validate(publicKey));

    error = Error::kNone;

exit:
    sign1Msg.Free();
    return error;
}

Error TokenManager::ParsePublicKey(mbedtls_pk_context &aPublicKey, const ByteArray &aCert)
{
    Error            error = Error::kInvalidArgs;
    mbedtls_x509_crt cert;

    mbedtls_x509_crt_init(&cert);

    VerifyOrExit(!aCert.empty());
    VerifyOrExit(mbedtls_x509_crt_parse(&cert, &aCert[0], aCert.size()) == 0);

    error = Error::kNone;

    // Steal the public key from the 'cert' object.
    MoveMbedtlsKey(aPublicKey, cert.pk);

exit:
    mbedtls_x509_crt_free(&cert);

    return error;
}

Error TokenManager::ParsePrivateKey(mbedtls_pk_context &aPrivateKey, const ByteArray &aPrivateKeyRaw)
{
    Error error = Error::kInvalidArgs;

    VerifyOrExit(!aPrivateKeyRaw.empty());
    VerifyOrExit(mbedtls_pk_parse_key(&aPrivateKey, &aPrivateKeyRaw[0], aPrivateKeyRaw.size(), nullptr, 0) == 0);

    error = Error::kNone;

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
