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

#if OT_COMM_CCM_ENABLE

#include "library/token_manager.hpp"

#include <catch2/catch.hpp>

#include "library/commissioner_impl.hpp"
#include "library/uri.hpp"

namespace ot {

namespace commissioner {

static const std::string kCommTrustAnchor = "-----BEGIN CERTIFICATE-----\r\n"
                                            "MIIBejCCAR+gAwIBAgIIc5C+m8ijatIwCgYIKoZIzj0EAwIwGDEWMBQGA1UEAwwN\r\n"
                                            "VGhyZWFkR3JvdXBDQTAeFw0xOTA2MTkyMTI0MjdaFw0yNDA2MTcyMTI0MjdaMBgx\r\n"
                                            "FjAUBgNVBAMMDVRocmVhZEdyb3VwQ0EwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNC\r\n"
                                            "AASXse2WkWSTCYW7uKyaKvlFXN/upLEd4uedBov6gDkmtABSUbNPHAgVpMvgP70b\r\n"
                                            "vLY19kMaIt54ZTuHuZU37OFso1MwUTAPBgNVHRMECDAGAQH/AgEDMB0GA1UdDgQW\r\n"
                                            "BBSS6nZAQEqPq08nC/O8N52GzXKA+DAfBgNVHSMEGDAWgBSS6nZAQEqPq08nC/O8\r\n"
                                            "N52GzXKA+DAKBggqhkjOPQQDAgNJADBGAiEA5l70etVXL6pUSU+E/5+8C6yM5HaD\r\n"
                                            "v8WNLEhNNeunmcMCIQCwyjOK804IuUTv7IOw/6y9ulOwTBHtfPJ8rfRyaVbHPQ==\r\n"
                                            "-----END CERTIFICATE-----\r\n";

static const std::string kCommCert = "-----BEGIN CERTIFICATE-----\r\n"
                                     "MIICATCCAaegAwIBAgIIJU8KN/Bcw4cwCgYIKoZIzj0EAwIwGDEWMBQGA1UEAwwN\r\n"
                                     "VGhyZWFkR3JvdXBDQTAeFw0xOTA2MTkyMTM2MTFaFw0yNDA2MTcyMTM2MTFaMBox\r\n"
                                     "GDAWBgNVBAMMD1RocmVhZFJlZ2lzdHJhcjBZMBMGByqGSM49AgEGCCqGSM49AwEH\r\n"
                                     "A0IABCAwhVvoRpELPssVyvhXLT61Zb3GVKFe+vbt66qLnhYIxckQyTogho/IUE03\r\n"
                                     "Dxsm+pdZ9nmDu3iGPtqay+pRJPajgdgwgdUwDwYDVR0TBAgwBgEB/wIBAjALBgNV\r\n"
                                     "HQ8EBAMCBeAwbAYDVR0RBGUwY6RhMF8xCzAJBgNVBAYTAlVTMRUwEwYDVQQKDAxU\r\n"
                                     "aHJlYWQgR3JvdXAxFzAVBgNVBAMMDlRlc3QgUmVnaXN0cmFyMSAwHgYJKoZIhvcN\r\n"
                                     "AQkBFhFtYXJ0aW5Ac3Rva29lLm5ldDBHBgNVHSMEQDA+gBSS6nZAQEqPq08nC/O8\r\n"
                                     "N52GzXKA+KEcpBowGDEWMBQGA1UEAwwNVGhyZWFkR3JvdXBDQYIIc5C+m8ijatIw\r\n"
                                     "CgYIKoZIzj0EAwIDSAAwRQIgbI7Vrg348jGCENRtT3GbV5FaEqeBaVTeHlkCA99z\r\n"
                                     "RVACIQDGDdZSWXAR+AlfmrDecYnmp5Vgz8eTyjm9ZziIFXPUwA==\r\n"
                                     "-----END CERTIFICATE-----\r\n";

static const std::string kCommKey = "-----BEGIN PRIVATE KEY-----\r\n"
                                    "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgYJ/MP0dWA9BkYd4W\r\n"
                                    "s6oRY62hDddaEmrAVm5dtAXE/UGhRANCAAQgMIVb6EaRCz7LFcr4Vy0+tWW9xlSh\r\n"
                                    "Xvr27euqi54WCMXJEMk6IIaPyFBNNw8bJvqXWfZ5g7t4hj7amsvqUST2\r\n"
                                    "-----END PRIVATE KEY-----\r\n";

// Pre-generated COSE_SIGN1 signed COMT_COK issued by Domain CA
// (signed by Domain CA private key).
static const std::string kSignedToken = "d28443a10126a058b0a40366546872656164047818323031392d31322d303654"
                                        "30393a32303a30312e3732365a01782aefbfbdefbfbd7640404aefbfbdefbfbd"
                                        "4f270befbfbdefbfbd37efbfbdefbfbdefbfbd72efbfbdefbfbd08a101a5024f"
                                        "4f542d636f6d6d697373696f6e6572225820c5c910c93a20868fc8504d370f1b"
                                        "26fa9759f67983bb78863eda9acbea5124f601022158202030855be846910b3e"
                                        "cb15caf8572d3eb565bdc654a15efaf6edebaa8b9e160820015840c0dae82ebb"
                                        "19664ed999313665d02f976e5290d6e48597da77963009e57cfd083fea4e07ca"
                                        "78fc6fa25239a7a1c3b54a4b761947a9e9e29782955c2d00d8c9f6";

TEST_CASE("signing-message", "[token]")
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
    REQUIRE(eventBase != nullptr);

    TokenManager tokenManager{eventBase};

    REQUIRE(tokenManager.Init(config) == Error::kNone);

    ByteArray signedToken;
    REQUIRE(utils::Hex(signedToken, kSignedToken) == Error::kNone);
    REQUIRE(!tokenManager.IsValid());
    REQUIRE(tokenManager.SetToken(signedToken, config.mTrustAnchor) == Error::kNone);
    REQUIRE(tokenManager.IsValid());
    REQUIRE(tokenManager.GetToken() == signedToken);

    coap::Message pet{coap::Type::kConfirmable, coap::Code::kPost};
    REQUIRE(pet.SetUriPath(uri::kPetitioning) == Error::kNone);
    REQUIRE(AppendTlv(pet, {tlv::Type::kCommissionerId, config.mId}) == Error::kNone);

    ByteArray signature;
    REQUIRE(tokenManager.SignMessage(signature, pet) == Error::kNone);
    REQUIRE(tokenManager.VerifySignature(signature, pet) == Error::kNone);
}

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_CCM_ENABLE
