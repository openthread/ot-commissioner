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
 *   This file defines test cases for token management.
 */

#if OT_COMM_CONFIG_CCM_ENABLE

#include "library/token_manager.hpp"

#include <gtest/gtest.h>

#include "library/commissioner_impl.hpp"
#include "library/uri.hpp"

namespace ot {

namespace commissioner {

static const std::string kCommTrustAnchor = "-----BEGIN CERTIFICATE-----\r\n"
                                            "MIIB9TCCAZugAwIBAgIBAzAKBggqhkjOPQQDAjBTMREwDwYDVQQDDAhkb21haW5j\r\n"
                                            "YTETMBEGA1UECwwKT3BlblRocmVhZDEPMA0GA1UECgwGR29vZ2xlMQswCQYDVQQH\r\n"
                                            "DAJTSDELMAkGA1UEBhMCQ04wHhcNMjAwNzE2MDgxNzI3WhcNMjUwNzE1MDgxNzI3\r\n"
                                            "WjBTMREwDwYDVQQDDAhkb21haW5jYTETMBEGA1UECwwKT3BlblRocmVhZDEPMA0G\r\n"
                                            "A1UECgwGR29vZ2xlMQswCQYDVQQHDAJTSDELMAkGA1UEBhMCQ04wWTATBgcqhkjO\r\n"
                                            "PQIBBggqhkjOPQMBBwNCAAQZBl5N2EWL7XNls/iGq/aT50bfwpt6hR7dy1NjIePo\r\n"
                                            "AU1Z1rxUOO/y2LplF33ruWaiWEQgvCxxMdwouPUWG4kvo2AwXjAdBgNVHQ4EFgQU\r\n"
                                            "ntrCM5X/cijrfa7IfRgt+ehXb1cwHwYDVR0jBBgwFoAUntrCM5X/cijrfa7IfRgt\r\n"
                                            "+ehXb1cwDAYDVR0TBAUwAwEB/zAOBgNVHQ8BAf8EBAMCAYYwCgYIKoZIzj0EAwID\r\n"
                                            "SAAwRQIhAKrMTukuzKduEGJ+n+qRYNjOyEgSj3zDRtQPD/K9rYt0AiAS1Jkf1QQi\r\n"
                                            "r5mw4uBcR81ktDEjxFUJ78VfzSooCWlpjQ==\r\n"
                                            "-----END CERTIFICATE-----\r\n";

static const std::string kCommCert = "-----BEGIN CERTIFICATE-----\r\n"
                                     "MIIB5TCCAYygAwIBAgIBAzAKBggqhkjOPQQDAjBTMREwDwYDVQQDDAhkb21haW5j\r\n"
                                     "YTETMBEGA1UECwwKT3BlblRocmVhZDEPMA0GA1UECgwGR29vZ2xlMQswCQYDVQQH\r\n"
                                     "DAJTSDELMAkGA1UEBhMCQ04wHhcNMjAwNzE3MDMyMDA4WhcNMjUwNzE2MDMyMDA4\r\n"
                                     "WjBXMRUwEwYDVQQDDAxjb21taXNzaW9uZXIxEzARBgNVBAsMCk9wZW5UaHJlYWQx\r\n"
                                     "DzANBgNVBAoMBkdvb2dsZTELMAkGA1UEBwwCU0gxCzAJBgNVBAYTAkNOMFkwEwYH\r\n"
                                     "KoZIzj0CAQYIKoZIzj0DAQcDQgAE2yJWLNu4accOABbL+8B7TsoD8r0nZzZTYA9b\r\n"
                                     "BPeE7SkmJag3q2/rxu+t43/TC42/ymXBAEN60LlWg18//PMEpKNNMEswHQYDVR0O\r\n"
                                     "BBYEFJsDINaJgyWtUYf/D+tWIgxXfqbGMB8GA1UdIwQYMBaAFJ7awjOV/3Io632u\r\n"
                                     "yH0YLfnoV29XMAkGA1UdEwQCMAAwCgYIKoZIzj0EAwIDRwAwRAIgUeCqd8nlhCGw\r\n"
                                     "vr0jAGRZsrxag4lRnFqPtM78eQIRvf8CIHUcc4JwB7zUGDiI3vMjbeJn1oyRyirn\r\n"
                                     "teLOKBXMZguj\r\n"
                                     "-----END CERTIFICATE-----\r\n";

static const std::string kCommKey = "-----BEGIN EC PRIVATE KEY-----\r\n"
                                    "MHcCAQEEIMzoNnqSk3ObV6ObkK1t7V3pq4uSBMOvtXLSOD9ifM4koAoGCCqGSM49\r\n"
                                    "AwEHoUQDQgAE2yJWLNu4accOABbL+8B7TsoD8r0nZzZTYA9bBPeE7SkmJag3q2/r\r\n"
                                    "xu+t43/TC42/ymXBAEN60LlWg18//PMEpA==\r\n"
                                    "-----END EC PRIVATE KEY-----\r\n";

// Pre-generated COSE_SIGN1 signed COMT_COK issued by Domain CA
// (signed by Domain CA private key).
static const std::string kSignedToken = "d28443a10126a058aea40366546872656164047818323032302d30392d313054"
                                        "31353a35313a33332e3638345a017828efbfbdefbfbdefbfbd33efbfbdefbfbd"
                                        "7228efbfbd7defbfbdefbfbd7d182defbfbdefbfbd576f5708a101a5024f4f54"
                                        "2d636f6d6d697373696f6e657201022001215820db22562cdbb869c70e0016cb"
                                        "fbc07b4eca03f2bd27673653600f5b04f784ed292258202625a837ab6febc6ef"
                                        "ade37fd30b8dbfca65c100437ad0b956835f3ffcf304a4584004895222f0797d"
                                        "7c482e1505a76ad6f69911ed5f7a2a341b4a417d109916659d4c824fa8433049"
                                        "b099d7443f65fd752d3c14d14a8f9b936fee0dc7ad6bd25ef1";

TEST(TokenTest, SigningMessage)
{
    Config config;

    config.mDomainName = "Thread";

    config.mTrustAnchor = {kCommTrustAnchor.begin(), kCommTrustAnchor.end()};
    config.mTrustAnchor.push_back(0);

    config.mCertificate = {kCommCert.begin(), kCommCert.end()};
    config.mCertificate.push_back(0);

    config.mPrivateKey = {kCommKey.begin(), kCommKey.end()};
    config.mPrivateKey.push_back(0);

    struct event_base *eventBase = event_base_new();
    EXPECT_NE(eventBase, nullptr);

    TokenManager tokenManager{eventBase};

    EXPECT_EQ(tokenManager.Init(config), ErrorCode::kNone);

    ByteArray signedToken;
    EXPECT_EQ(utils::Hex(signedToken, kSignedToken), ErrorCode::kNone);
    EXPECT_EQ(tokenManager.SetToken(signedToken), ErrorCode::kNone);
    EXPECT_EQ(tokenManager.GetToken(), signedToken);

    coap::Message pet{coap::Type::kConfirmable, coap::Code::kPost};
    EXPECT_EQ(pet.SetUriPath(uri::kPetitioning), ErrorCode::kNone);
    EXPECT_EQ(AppendTlv(pet, {tlv::Type::kCommissionerId, config.mId}), ErrorCode::kNone);

    ByteArray signature;
    EXPECT_EQ(tokenManager.SignMessage(signature, pet), ErrorCode::kNone);
    EXPECT_EQ(tokenManager.ValidateSignature(signature, pet), ErrorCode::kNone);
}

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_CONFIG_CCM_ENABLE
