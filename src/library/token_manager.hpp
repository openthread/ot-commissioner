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

#ifndef OT_COMM_LIBRARY_TOKEN_MANAGER_HPP_
#define OT_COMM_LIBRARY_TOKEN_MANAGER_HPP_

#if OT_COMM_CCM_ENABLE

#include <mbedtls/pk.h>

#include <commissioner/commissioner.hpp>
#include <commissioner/error.hpp>

#include "library/cbor.hpp"
#include "library/coap_secure.hpp"

namespace ot {

namespace commissioner {

// Managing of Commissioner Token, signing message, verifying signature.
class TokenManager
{
public:
    explicit TokenManager(struct event_base *aEventBase);
    ~TokenManager();

    // Initialized with Commissioner configuration.
    Error Init(const Config &aConfig);

    bool IsValid() const { return mToken.IsValid(); }

    // Set the signed Commissioner Token when the signature verification succeed.
    // @param[in] aSignedToken  A COSE-signed Commissioner Token.
    // @param[in] aCert         A certificate associates to the signing key of @p aSignedToken.
    Error SetToken(const ByteArray &aSignedToken, const ByteArray &aCert);

    const ByteArray &GetToken() const { return mSignedToken; }

    const std::string &GetDomainName() const;

    void AbortRequests() { mRegistrarClient.AbortRequests(); }

    // Request Commissioner Token from registrar.
    // The protocol is described in `doc/registrar-connector.md`.
    void RequestToken(Commissioner::Handler<ByteArray> aHandler, const std::string &aAddr, uint16_t aPort);

    // Sign a CoAP message with COSE-Sign1.
    // Described in 12.5.5 of Thread 1.2 spec.
    Error SignMessage(ByteArray &aSignature, const coap::Message &aMessage);

    // Verify the signature in a signed CoAP message.
    // Described in 12.5.6 of Thread 1.2 spec.
    Error VerifySignature(const ByteArray &aSignature, const coap::Message &aSignedMessage);

    // Parse the public key from PEM/DER encoded certificate.
    static Error ParsePublicKey(mbedtls_pk_context &aPublicKey, const ByteArray &aCert);

    // Parse the private key from the PEM/DER encoded private key.
    static Error ParsePrivateKey(mbedtls_pk_context &aPrivateKey, const ByteArray &aPrivateKeyRaw);

private:
    /*
     * Thread Constants.
     */
    static const size_t kMaxCoseKeyIdLength = 16;

    // Move the resource from src to des, leaving the src invalid.
    static void MoveMbedtlsKey(mbedtls_pk_context &aDes, mbedtls_pk_context &aSrc);

    // Verifying the signature in the signed Commissioner Token
    // with the public key of the signer.
    Error VerifyToken(CborMap &aToken, const ByteArray &aSignedToken, const mbedtls_pk_context &aPublicKey);

    void         SendTokenRequest(Commissioner::Handler<ByteArray> aHandler);
    static Error MakeTokenRequest(ByteArray &               aBuf,
                                  const mbedtls_pk_context &aPublicKey,
                                  const std::string &       aId,
                                  const std::string &       aDomainName);

    // Prepare the content to be signed.
    // Described in 12.5.5 of Thread 1.2 spec.
    static Error PrepareSigningContent(ByteArray &aContent, const coap::Message &aMessage);

    // Get the authorized Commissioner Public Key in the Commissioner Token.
    Error GetPublicKey(CborMap &aPublicKey) const;

    // Set the signed Commissioner Token when the signature verification succeed.
    // @param[in] aSignedToken  A COSE-signed Commissioner Token.
    // @param[in] aPublicKey    A public key associates to the signing key of @p aSignedToken.
    Error SetToken(const ByteArray &aSignedToken, const mbedtls_pk_context &aPublicKey);

    // The sequence number of this commissioner token.
    // Increased 1 for each signing operation.
    uint64_t mSequenceNumber = 0;

    // Take the KID from COM_TOK's COSE_Key in the CNF claim.
    ByteArray mKeyId;

    // The cose signed commissioner token.
    ByteArray mSignedToken;

    // The parsed commissioner token in CBOR structure.
    CborMap mToken;

    std::string        mCommissionerId;
    std::string        mDomainName;
    mbedtls_pk_context mPublicKey;
    mbedtls_pk_context mPrivateKey;
    mbedtls_pk_context mDomainCAPublicKey;

    coap::CoapSecure mRegistrarClient;
};

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_CCM_ENABLE

#endif // OT_COMM_LIBRARY_TOKEN_MANAGER_HPP_
