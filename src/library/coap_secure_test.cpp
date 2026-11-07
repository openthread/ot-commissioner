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
 *   This file defines test cases for CoAP implementation.
 */

#include "library/coap_secure.hpp"

#include <gtest/gtest.h>

namespace ot {

namespace commissioner {

namespace coap {

static const std::string kClientTrustAnchor = "-----BEGIN CERTIFICATE-----\r\n"
                                              "MIIBhTCCASugAwIBAgIUEZrQnf8iH3PYpbtc7PMhd+5EMSswCgYIKoZIzj0EAwIw\r\n"
                                              "GDEWMBQGA1UEAwwNVGhyZWFkR3JvdXBDQTAeFw0yNTA5MjQwNDUxMDlaFw0zNTA5\r\n"
                                              "MjIwNDUxMDlaMBgxFjAUBgNVBAMMDVRocmVhZEdyb3VwQ0EwWTATBgcqhkjOPQIB\r\n"
                                              "BggqhkjOPQMBBwNCAAR5d2C22dtBQfu0E69YVKUdBlSwdvd1maeyvW7sxpNswasX\r\n"
                                              "GnKjUKHW9950m4Pw6YvV+5Emxw23YdvN0IY2+nQMo1MwUTAdBgNVHQ4EFgQUzmMx\r\n"
                                              "td34Zih6C4aYNdaZECjgQV8wHwYDVR0jBBgwFoAUzmMxtd34Zih6C4aYNdaZECjg\r\n"
                                              "QV8wDwYDVR0TAQH/BAUwAwEB/zAKBggqhkjOPQQDAgNIADBFAiAK6EBelCHpjaPx\r\n"
                                              "c7SssfmUGzb1u44YahVxlh5gZbuCmwIhAIYeycNpRVOVEAXuoJYeG1Ez7i+CVeNR\r\n"
                                              "7N9vrIylB8A+\r\n"
                                              "-----END CERTIFICATE-----\r\n";

static const std::string kClientCert = "-----BEGIN CERTIFICATE-----\r\n"
                                       "MIIBcjCCARmgAwIBAgIUTsd8PPWTr5Dl8P1jj8V3tlmDGDswCgYIKoZIzj0EAwIw\r\n"
                                       "GDEWMBQGA1UEAwwNVGhyZWFkR3JvdXBDQTAeFw0yNTA5MjQwNDUxMDlaFw0zNTA5\r\n"
                                       "MjIwNDUxMDlaMBcxFTATBgNVBAMMDFRocmVhZENsaWVudDBZMBMGByqGSM49AgEG\r\n"
                                       "CCqGSM49AwEHA0IABJjtiRe7qsIvGC0fblGEM0vi36HFcJ4jX9JEBWUAR4kqMu8t\r\n"
                                       "X619Kgf6wyZsmSuBQfESI5A3lFwrP+pmAPT+FiejQjBAMB0GA1UdDgQWBBSn1HEr\r\n"
                                       "V2jDNiS7R/tHJDZyUvnN1DAfBgNVHSMEGDAWgBTOYzG13fhmKHoLhpg11pkQKOBB\r\n"
                                       "XzAKBggqhkjOPQQDAgNHADBEAiBHDbT44MGbo+ZQNmFW8m8JWv8vDnxtkaTbEVRu\r\n"
                                       "0XT7RwIgTEznRgFQ0aiJz8AYNjT+DgZVzZEq5ROQnUOZqPUh26Y=\r\n"
                                       "-----END CERTIFICATE-----\r\n";

static const std::string kClientKey = "-----BEGIN EC PARAMETERS-----\r\n"
                                      "BggqhkjOPQMBBw==\r\n"
                                      "-----END EC PARAMETERS-----\r\n"
                                      "-----BEGIN EC PRIVATE KEY-----\r\n"
                                      "MHcCAQEEIFVHUtrU9IUeM44w0KtZeg7ulLE7vFx8hs6+xNIK/3fqoAoGCCqGSM49\r\n"
                                      "AwEHoUQDQgAEmO2JF7uqwi8YLR9uUYQzS+LfocVwniNf0kQFZQBHiSoy7y1frX0q\r\n"
                                      "B/rDJmyZK4FB8RIjkDeUXCs/6mYA9P4WJw==\r\n"
                                      "-----END EC PRIVATE KEY-----\r\n";

static const std::string kServerTrustAnchor = "-----BEGIN CERTIFICATE-----\r\n"
                                              "MIIBhTCCASugAwIBAgIUEZrQnf8iH3PYpbtc7PMhd+5EMSswCgYIKoZIzj0EAwIw\r\n"
                                              "GDEWMBQGA1UEAwwNVGhyZWFkR3JvdXBDQTAeFw0yNTA5MjQwNDUxMDlaFw0zNTA5\r\n"
                                              "MjIwNDUxMDlaMBgxFjAUBgNVBAMMDVRocmVhZEdyb3VwQ0EwWTATBgcqhkjOPQIB\r\n"
                                              "BggqhkjOPQMBBwNCAAR5d2C22dtBQfu0E69YVKUdBlSwdvd1maeyvW7sxpNswasX\r\n"
                                              "GnKjUKHW9950m4Pw6YvV+5Emxw23YdvN0IY2+nQMo1MwUTAdBgNVHQ4EFgQUzmMx\r\n"
                                              "td34Zih6C4aYNdaZECjgQV8wHwYDVR0jBBgwFoAUzmMxtd34Zih6C4aYNdaZECjg\r\n"
                                              "QV8wDwYDVR0TAQH/BAUwAwEB/zAKBggqhkjOPQQDAgNIADBFAiAK6EBelCHpjaPx\r\n"
                                              "c7SssfmUGzb1u44YahVxlh5gZbuCmwIhAIYeycNpRVOVEAXuoJYeG1Ez7i+CVeNR\r\n"
                                              "7N9vrIylB8A+\r\n"
                                              "-----END CERTIFICATE-----\r\n";

static const std::string kServerCert = "-----BEGIN CERTIFICATE-----\r\n"
                                       "MIIBdjCCARygAwIBAgIUTsd8PPWTr5Dl8P1jj8V3tlmDGDowCgYIKoZIzj0EAwIw\r\n"
                                       "GDEWMBQGA1UEAwwNVGhyZWFkR3JvdXBDQTAeFw0yNTA5MjQwNDUxMDlaFw0zNTA5\r\n"
                                       "MjIwNDUxMDlaMBoxGDAWBgNVBAMMD1RocmVhZFJlZ2lzdHJhcjBZMBMGByqGSM49\r\n"
                                       "AgEGCCqGSM49AwEHA0IABGr5hdFY+5eaF1vhw6wG+0Mybw0tauCxG04X7OqXv7/P\r\n"
                                       "V7Y+teABvZkorhF2b332Z7Pqk/6k+wuCX1N5VAZJtyijQjBAMB0GA1UdDgQWBBQf\r\n"
                                       "ASGIrYEzMd1F/eYF1IzmZ5M5bTAfBgNVHSMEGDAWgBTOYzG13fhmKHoLhpg11pkQ\r\n"
                                       "KOBBXzAKBggqhkjOPQQDAgNIADBFAiEA4zluVAVVDfsCCuv4OSwx9o1P7w+QvmEC\r\n"
                                       "xhJPt7eGQRYCIEgzvrcQ4VPinEe8t3CkIrrHe/zQrkHw9ZhQnLv509XW\r\n"
                                       "-----END CERTIFICATE-----\r\n";

static const std::string kServerKey = "-----BEGIN EC PARAMETERS-----\r\n"
                                      "BggqhkjOPQMBBw==\r\n"
                                      "-----END EC PARAMETERS-----\r\n"
                                      "-----BEGIN EC PRIVATE KEY-----\r\n"
                                      "MHcCAQEEIMdyKql6JZNPhCw+KSY/zbYKRor5qoebmt9kQQ73MzWcoAoGCCqGSM49\r\n"
                                      "AwEHoUQDQgAEavmF0Vj7l5oXW+HDrAb7QzJvDS1q4LEbThfs6pe/v89Xtj614AG9\r\n"
                                      "mSiuEXZvffZns+qT/qT7C4JfU3lUBkm3KA==\r\n"
                                      "-----END EC PRIVATE KEY-----\r\n";

static const char        *kServerAddr = "::";
static constexpr uint16_t kServerPort = 5683;

TEST(CoapsTest, CoapsClientServerHello)
{
    DtlsConfig config;

    // Setup coap secure server
    config.mCaChain = ByteArray{kServerTrustAnchor.begin(), kServerTrustAnchor.end()};
    config.mOwnCert = ByteArray{kServerCert.begin(), kServerCert.end()};
    config.mOwnKey  = ByteArray{kServerKey.begin(), kServerKey.end()};

    config.mCaChain.push_back(0);
    config.mOwnCert.push_back(0);
    config.mOwnKey.push_back(0);

    auto eventBase = event_base_new();
    ASSERT_NE(eventBase, nullptr);

    CoapSecure coapsServer{eventBase, true};
    Resource   resHello{"/hello", [&coapsServer](const Request &aRequest) {
                          EXPECT_EQ(aRequest.GetType(), Type::kConfirmable);
                          EXPECT_EQ(aRequest.GetCode(), Code::kPost);

                          Response response{Type::kAcknowledgment, Code::kChanged};
                          response.Append("world");
                          EXPECT_EQ(coapsServer.SendResponse(aRequest, response), ErrorCode::kNone);
                      }};
    EXPECT_EQ(coapsServer.AddResource(resHello), ErrorCode::kNone);

    auto onServerConnected = [&coapsServer](const DtlsSession &aSession, Error aError) {
        EXPECT_EQ(aError, ErrorCode::kNone);
        EXPECT_EQ(&aSession, &coapsServer.GetDtlsSession());
        EXPECT_EQ(aSession.GetLocalPort(), kServerPort);
    };

    EXPECT_EQ(coapsServer.Init(config), ErrorCode::kNone);
    EXPECT_EQ(coapsServer.Start(onServerConnected, kServerAddr, kServerPort), ErrorCode::kNone);

    // Setup coap secure client
    config.mCaChain = ByteArray{kClientTrustAnchor.begin(), kClientTrustAnchor.end()};
    config.mOwnCert = ByteArray{kClientCert.begin(), kClientCert.end()};
    config.mOwnKey  = ByteArray{kClientKey.begin(), kClientKey.end()};

    config.mCaChain.push_back(0);
    config.mOwnCert.push_back(0);
    config.mOwnKey.push_back(0);

    config.mHostname = "ThreadRegistrar";

    CoapSecure coapsClient{eventBase, false};
    EXPECT_EQ(coapsClient.Init(config), ErrorCode::kNone);
    auto onClientConnected = [&coapsClient, eventBase](const DtlsSession &aSession, Error aError) {
        EXPECT_EQ(aError, ErrorCode::kNone);
        EXPECT_EQ(aSession.GetPeerPort(), kServerPort);

        Request request{Type::kConfirmable, Code::kPost};
        EXPECT_EQ(request.SetUriPath("/hello"), ErrorCode::kNone);
        auto onResponse = [eventBase](const Response *aResponse, Error aError) {
            EXPECT_EQ(aError, ErrorCode::kNone);
            EXPECT_NE(aResponse, nullptr);
            EXPECT_EQ(aResponse->GetType(), Type::kAcknowledgment);
            EXPECT_EQ(aResponse->GetCode(), Code::kChanged);
            EXPECT_EQ(aResponse->GetPayloadAsString(), "world");

            event_base_loopbreak(eventBase);
        };
        coapsClient.SendRequest(request, onResponse);
    };
    coapsClient.Connect(onClientConnected, kServerAddr, kServerPort);

    EXPECT_EQ(event_base_loop(eventBase, EVLOOP_NO_EXIT_ON_EMPTY), 0);
    event_base_free(eventBase);
}

} // namespace coap

} // namespace commissioner

} // namespace ot
