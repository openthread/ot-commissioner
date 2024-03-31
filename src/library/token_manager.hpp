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

#if OT_COMM_CONFIG_CCM_ENABLE

#include <mbedtls/pk.h>

#include <commissioner/commissioner.hpp>
#include <commissioner/error.hpp>

#include "library/cbor.hpp"
#include "library/coap_secure.hpp"

namespace ot {

namespace commissioner {

/**
 * The class holds Commissioner Token, signs messages and verifies signatures.
 *
 */
class TokenManager
{
public:
    explicit TokenManager(struct event_base *aEventBase);
    ~TokenManager();

    /**
     * This method initializes the Token Manager.
     *
     * @param[in]  aConfig  The commissioner configuration.
     *
     * @retval  Error::kNone  Successfully initialized the Token Manager.
     * @retval  ...           Failed to initialize the Token Manager.
     *
     */
    Error Init(const Config &aConfig);

    /**
     * This method sets the Commissioner Token (COM_TOK).
     *
     * This method will first validate the Commissioner Token against the Domain CA
     * public key before accepting it. In case `OT_COMM_CONFIG_REFERENCE_DEVICE_ENABLE`
     * is enabled, the token will be always be accepted if `aAlwaysAccept` is true.
     * Otherwise, the Commissioner Token validation must succeed before accepting it.
     *
     * @param[in]  aSignedToken   The COSE-signed Commissioner Token to set to.
     * @param[in]  aAlwaysAccept  A boolean indicates whether we always accept the given
     *                            Commissioner Token. This paramater has effect only
     *                            when `OT_COMM_CONFIG_REFERENCE_DEVICE_ENABLE` is enabled.
     *
     * @retval  Error::kNone  Successfully set the token.
     * @retval  ...           Failed to set the token.
     *
     */
    Error SetToken(const ByteArray &aSignedToken, bool aAlwaysAccept = false);

    /**
     * This method returns the COSE-signed Commissioner Token.
     *
     * @returns  A reference to the signed token.
     *
     */
    const ByteArray &GetToken() const { return mSignedToken; }

    /**
     * This method returns the Domain Name associated with the token (manager).
     *
     * @returns  A reference to the domain name.
     *
     */
    const std::string &GetDomainName() const;

    /**
     * This method cancels Commissioner Token requests.
     *
     */
    void CancelRequests() { mRegistrarClient.CancelRequests(); }

    /**
     * This method requests Commissioner Token from the registrar.
     *
     * @param[in]  aHandler  The handler of the CoAP response.
     * @param[in]  aAddr     The address of the registrar service.
     * @param[in]  aPort     The port of the registrar service.
     *
     */
    void RequestToken(Commissioner::Handler<ByteArray> aHandler, const std::string &aAddr, uint16_t aPort);

    /**
     * This method signs a CoAP message with COSE-sign1.
     *
     * See section 12.5.5 of Thread 1.2 specification for details of
     * MGMT message signing.
     *
     * @param[out]  aSignature  The generated Commissioner Token Signature (COM_TOK_SIG).
     * @param[in]   aMessage    The CoAP message to be signed.
     *
     * @retval  Error::kNone  Successfully signed the message.
     * @retval  ...           Failed to sign the message.
     *
     */
    Error SignMessage(ByteArray &aSignature, const coap::Message &aMessage);

    /**
     * This method validates signature of a CoAP message.
     *
     * See section 12.5.5 of Thread 1.2 specification for details of
     * MGMT message signing.
     *
     * @param[in]  aSignature      The signature of the CoAP message.
     * @param[in]  aSignedMessage  The CoAP message @p aSignature associated to.
     *
     * @retval  Error::kNone  Successfully validated the signature.
     * @retval  ...           Failed to validate the signature.
     *
     */
    Error ValidateSignature(const ByteArray &aSignature, const coap::Message &aSignedMessage);

    /**
     * This method parse public key from PEM/DER encoded certificate.
     *
     * @param[out]  aPublicKey  The parsed public key.
     * @param[in]   aCert       The given PEM/DER encoded certificate.
     *
     * @retval  Error::kNone  Successfully parsed the public key.
     * @retval  ...           Failed to parse the public key.
     *
     */
    static Error ParsePublicKey(mbedtls_pk_context &aPublicKey, const ByteArray &aCert);

    /**
     * This method parse private key from PEM/DER encoded certificate.
     *
     * @param[out]  aPrivateKey     The parsed private key.
     * @param[in]   aPrivateKeyRaw  The private key in format of PEM/DER encoded byte array.
     *
     * @retval  Error::kNone  Successfully parsed the private key.
     * @retval  ...           Failed to parse the private key.
     *
     */
    static Error ParsePrivateKey(mbedtls_pk_context &aPrivateKey, const ByteArray &aPrivateKeyRaw);

private:
    /*
     * Thread Constants.
     */
    static const size_t kMaxCoseKeyIdLength = 16;

    // Move the resource from src to des, leaving the src invalid.
    static void MoveMbedtlsKey(mbedtls_pk_context &aDes, mbedtls_pk_context &aSrc);

    Error ValidateToken(CborMap &aToken, const ByteArray &aSignedToken, const mbedtls_pk_context &aPublicKey) const;
    Error ValidateToken(const ByteArray &aSignedToken, const mbedtls_pk_context &aPublicKey) const;

    void         SendTokenRequest(Commissioner::Handler<ByteArray> aHandler);
    static Error MakeTokenRequest(ByteArray                &aBuf,
                                  const mbedtls_pk_context &aPublicKey,
                                  const std::string        &aId,
                                  const std::string        &aDomainName);

    // Prepare the content to be signed.
    // Described in 12.5.5 of Thread 1.2 spec.
    static Error PrepareSigningContent(ByteArray &aContent, const coap::Message &aMessage);

    // Get the authorized Commissioner Public Key in the Commissioner Token.
    Error GetPublicKeyInToken(ByteArray &aPublicKey) const;

    // Take the KID from COM_TOK's COSE_Key in the CNF claim.
    Error GetKeyId(ByteArray &aKeyId, const CborMap &aToken) const;
    Error GetKeyId(ByteArray &aKeyId) const;

    // The sequence number of this commissioner token.
    // Increased 1 for each signing operation.
    uint64_t mSequenceNumber = 0;

    // The cose signed commissioner token.
    ByteArray mSignedToken;

    std::string        mCommissionerId;
    std::string        mDomainName;
    mbedtls_pk_context mPublicKey;
    mbedtls_pk_context mPrivateKey;
    mbedtls_pk_context mDomainCAPublicKey;

    coap::CoapSecure mRegistrarClient;
};

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_CONFIG_CCM_ENABLE

#endif // OT_COMM_LIBRARY_TOKEN_MANAGER_HPP_
