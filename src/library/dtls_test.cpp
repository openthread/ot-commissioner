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
 *   This file defines test cases for DTLS implementation with mbedtls.
 */

#include "library/dtls.hpp"

#include <gtest/gtest.h>

#include "library/coap.hpp"

namespace ot {

namespace commissioner {

static const std::string kClientTrustAnchor = "-----BEGIN CERTIFICATE-----\r\n"
                                              "MIIBejCCAR+gAwIBAgIIc5C+m8ijatIwCgYIKoZIzj0EAwIwGDEWMBQGA1UEAwwN\r\n"
                                              "VGhyZWFkR3JvdXBDQTAeFw0xOTA2MTkyMTI0MjdaFw0yNDA2MTcyMTI0MjdaMBgx\r\n"
                                              "FjAUBgNVBAMMDVRocmVhZEdyb3VwQ0EwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNC\r\n"
                                              "AASXse2WkWSTCYW7uKyaKvlFXN/upLEd4uedBov6gDkmtABSUbNPHAgVpMvgP70b\r\n"
                                              "vLY19kMaIt54ZTuHuZU37OFso1MwUTAPBgNVHRMECDAGAQH/AgEDMB0GA1UdDgQW\r\n"
                                              "BBSS6nZAQEqPq08nC/O8N52GzXKA+DAfBgNVHSMEGDAWgBSS6nZAQEqPq08nC/O8\r\n"
                                              "N52GzXKA+DAKBggqhkjOPQQDAgNJADBGAiEA5l70etVXL6pUSU+E/5+8C6yM5HaD\r\n"
                                              "v8WNLEhNNeunmcMCIQCwyjOK804IuUTv7IOw/6y9ulOwTBHtfPJ8rfRyaVbHPQ==\r\n"
                                              "-----END CERTIFICATE-----\r\n";

static const std::string kClientCert = "-----BEGIN CERTIFICATE-----\r\n"
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

static const std::string kClientKey = "-----BEGIN PRIVATE KEY-----\r\n"
                                      "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgYJ/MP0dWA9BkYd4W\r\n"
                                      "s6oRY62hDddaEmrAVm5dtAXE/UGhRANCAAQgMIVb6EaRCz7LFcr4Vy0+tWW9xlSh\r\n"
                                      "Xvr27euqi54WCMXJEMk6IIaPyFBNNw8bJvqXWfZ5g7t4hj7amsvqUST2\r\n"
                                      "-----END PRIVATE KEY-----\r\n";

static const std::string kServerTrustAnchor = "-----BEGIN CERTIFICATE-----\r\n"
                                              "MIIBejCCAR+gAwIBAgIIc5C+m8ijatIwCgYIKoZIzj0EAwIwGDEWMBQGA1UEAwwN\r\n"
                                              "VGhyZWFkR3JvdXBDQTAeFw0xOTA2MTkyMTI0MjdaFw0yNDA2MTcyMTI0MjdaMBgx\r\n"
                                              "FjAUBgNVBAMMDVRocmVhZEdyb3VwQ0EwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNC\r\n"
                                              "AASXse2WkWSTCYW7uKyaKvlFXN/upLEd4uedBov6gDkmtABSUbNPHAgVpMvgP70b\r\n"
                                              "vLY19kMaIt54ZTuHuZU37OFso1MwUTAPBgNVHRMECDAGAQH/AgEDMB0GA1UdDgQW\r\n"
                                              "BBSS6nZAQEqPq08nC/O8N52GzXKA+DAfBgNVHSMEGDAWgBSS6nZAQEqPq08nC/O8\r\n"
                                              "N52GzXKA+DAKBggqhkjOPQQDAgNJADBGAiEA5l70etVXL6pUSU+E/5+8C6yM5HaD\r\n"
                                              "v8WNLEhNNeunmcMCIQCwyjOK804IuUTv7IOw/6y9ulOwTBHtfPJ8rfRyaVbHPQ==\r\n"
                                              "-----END CERTIFICATE-----\r\n";

static const std::string kServerCert = "-----BEGIN CERTIFICATE-----\r\n"
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

static const std::string kServerKey = "-----BEGIN PRIVATE KEY-----\r\n"
                                      "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgYJ/MP0dWA9BkYd4W\r\n"
                                      "s6oRY62hDddaEmrAVm5dtAXE/UGhRANCAAQgMIVb6EaRCz7LFcr4Vy0+tWW9xlSh\r\n"
                                      "Xvr27euqi54WCMXJEMk6IIaPyFBNNw8bJvqXWfZ5g7t4hj7amsvqUST2\r\n"
                                      "-----END PRIVATE KEY-----\r\n";

static const char        *kServerAddr = "::";
static constexpr uint16_t kServerPort = 5683;

TEST(DtlsTest, MbedtlsClientServer)
{
    const ByteArray kHello{'h', 'e', 'l', 'l', 'o'};

    DtlsConfig config;

    // Setup dtls server
    config.mCaChain = ByteArray{kServerTrustAnchor.begin(), kServerTrustAnchor.end()};
    config.mOwnCert = ByteArray{kServerCert.begin(), kServerCert.end()};
    config.mOwnKey  = ByteArray{kServerKey.begin(), kServerKey.end()};

    config.mCaChain.push_back(0);
    config.mOwnCert.push_back(0);
    config.mOwnKey.push_back(0);

    auto eventBase = event_base_new();
    ASSERT_NE(eventBase, nullptr);

    auto serverSocket = std::make_shared<UdpSocket>(eventBase);
    EXPECT_EQ(serverSocket->Bind(kServerAddr, kServerPort), 0);

    DtlsSession dtlsServer{eventBase, true, serverSocket};
    EXPECT_EQ(dtlsServer.Init(config), ErrorCode::kNone);

    dtlsServer.SetReceiver([&kHello, eventBase](Endpoint &, const ByteArray &aBuf) {
        EXPECT_EQ(aBuf, kHello);

        event_base_loopbreak(eventBase);
    });

    auto serverConnected = [](const DtlsSession &aSession, Error aError) {
        EXPECT_EQ(aError, ErrorCode::kNone);
        EXPECT_EQ(aSession.GetState(), DtlsSession::State::kConnected);
    };
    dtlsServer.Connect(serverConnected);

    // Setup dtls client
    config.mCaChain = ByteArray{kClientTrustAnchor.begin(), kClientTrustAnchor.end()};
    config.mOwnCert = ByteArray{kClientCert.begin(), kClientCert.end()};
    config.mOwnKey  = ByteArray{kClientKey.begin(), kClientKey.end()};

    config.mCaChain.push_back(0);
    config.mOwnCert.push_back(0);
    config.mOwnKey.push_back(0);

    auto clientSocket = std::make_shared<UdpSocket>(eventBase);
    EXPECT_EQ(clientSocket->Connect(kServerAddr, kServerPort), 0);

    DtlsSession dtlsClient{eventBase, false, clientSocket};
    EXPECT_EQ(dtlsClient.Init(config), ErrorCode::kNone);

    auto clientConnected = [&kHello](DtlsSession &aSession, Error aError) {
        EXPECT_EQ(aError, ErrorCode::kNone);
        EXPECT_EQ(aSession.GetState(), DtlsSession::State::kConnected);
        EXPECT_EQ(aSession.Send(kHello, MessageSubType::kNone), ErrorCode::kNone);
    };
    dtlsClient.Connect(clientConnected);

    int fail = event_base_loop(eventBase, EVLOOP_NO_EXIT_ON_EMPTY);
    ASSERT_EQ(fail, 0);
    event_base_free(eventBase);
}

} // namespace commissioner

} // namespace ot
