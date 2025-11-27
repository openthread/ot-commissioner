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

// Helper function to set up client configuration
void SetUpClientConfig(DtlsConfig &config)
{
    config.mCaChain = ByteArray{kClientTrustAnchor.begin(), kClientTrustAnchor.end()};
    config.mOwnCert = ByteArray{kClientCert.begin(), kClientCert.end()};
    config.mOwnKey  = ByteArray{kClientKey.begin(), kClientKey.end()};

    config.mCaChain.push_back(0);
    config.mOwnCert.push_back(0);
    config.mOwnKey.push_back(0);
}

// Helper function to set up server configuration
void SetUpServerConfig(DtlsConfig &config)
{
    config.mCaChain = ByteArray{kServerTrustAnchor.begin(), kServerTrustAnchor.end()};
    config.mOwnCert = ByteArray{kServerCert.begin(), kServerCert.end()};
    config.mOwnKey  = ByteArray{kServerKey.begin(), kServerKey.end()};

    config.mCaChain.push_back(0);
    config.mOwnCert.push_back(0);
    config.mOwnKey.push_back(0);
}

// Helper function to create and configure event base
struct event_base *CreateEventBase()
{
    auto eventBase = event_base_new();
    EXPECT_NE(eventBase, nullptr);
    return eventBase;
}

TEST(DtlsTest, MbedtlsClientServer)
{
    const ByteArray kHello{'h', 'e', 'l', 'l', 'o'};

    DtlsConfig config;

    // Setup dtls server
    SetUpServerConfig(config);

    auto eventBase = CreateEventBase();

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
    SetUpClientConfig(config);
    config.mHostname = "ThreadRegistrar";

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

TEST(DtlsTest, ClientHostnameVerificationValid)
{
    DtlsConfig config;

    // Setup client configuration with valid hostname
    SetUpClientConfig(config);
    config.mHostname = "ThreadRegistrar";

    auto eventBase = CreateEventBase();

    auto        clientSocket = std::make_shared<UdpSocket>(eventBase);
    DtlsSession dtlsClient{eventBase, false, clientSocket};

    // Should succeed with valid hostname
    EXPECT_EQ(dtlsClient.Init(config), ErrorCode::kNone);

    event_base_free(eventBase);
}

TEST(DtlsTest, ClientHostnameVerificationEmpty)
{
    DtlsConfig config;

    // Setup client configuration without hostname
    SetUpClientConfig(config);
    config.mHostname = ""; // Leave hostname empty - should skip hostname verification

    auto eventBase = CreateEventBase();

    auto        clientSocket = std::make_shared<UdpSocket>(eventBase);
    DtlsSession dtlsClient{eventBase, false, clientSocket};

    // Should succeed even with empty hostname (no verification)
    EXPECT_EQ(dtlsClient.Init(config), ErrorCode::kNone);

    event_base_free(eventBase);
}

TEST(DtlsTest, ServerIgnoresHostname)
{
    DtlsConfig config;

    // Setup server configuration with hostname (should be ignored)
    SetUpServerConfig(config);
    config.mHostname = "SomeHostname"; // Set hostname on server - should be ignored

    auto eventBase = CreateEventBase();

    auto        serverSocket = std::make_shared<UdpSocket>(eventBase);
    DtlsSession dtlsServer{eventBase, true, serverSocket};

    // Should succeed - server ignores hostname setting
    EXPECT_EQ(dtlsServer.Init(config), ErrorCode::kNone);

    event_base_free(eventBase);
}

TEST(DtlsTest, ClientHostnameVerificationWithSpecialCharacters)
{
    DtlsConfig config;

    // Setup client configuration
    SetUpClientConfig(config);
    config.mHostname = "thread-registrar.local"; // Test hostname with special characters

    auto eventBase = CreateEventBase();

    auto        clientSocket = std::make_shared<UdpSocket>(eventBase);
    DtlsSession dtlsClient{eventBase, false, clientSocket};

    // Should succeed - hostname is set correctly even with special chars
    EXPECT_EQ(dtlsClient.Init(config), ErrorCode::kNone);

    event_base_free(eventBase);
}

TEST(DtlsTest, ClientHostnameVerificationIntegration)
{
    const ByteArray kHello{'h', 'e', 'l', 'l', 'o'};
    bool            connectionSucceeded = false;
    bool            messageReceived     = false;

    DtlsConfig config;

    // Setup dtls server
    SetUpServerConfig(config);

    auto eventBase = CreateEventBase();

    auto serverSocket = std::make_shared<UdpSocket>(eventBase);
    EXPECT_EQ(serverSocket->Bind(kServerAddr, kServerPort + 1), 0); // Use different port

    DtlsSession dtlsServer{eventBase, true, serverSocket};
    EXPECT_EQ(dtlsServer.Init(config), ErrorCode::kNone);

    dtlsServer.SetReceiver([&](Endpoint &, const ByteArray &aBuf) {
        EXPECT_EQ(aBuf, kHello);
        messageReceived = true;
        event_base_loopbreak(eventBase);
    });

    auto serverConnected = [&](const DtlsSession &aSession, Error aError) {
        EXPECT_EQ(aError, ErrorCode::kNone);
        EXPECT_EQ(aSession.GetState(), DtlsSession::State::kConnected);
    };
    dtlsServer.Connect(serverConnected);

    // Setup dtls client with hostname verification
    SetUpClientConfig(config);
    config.mHostname = "ThreadRegistrar"; // Use correct hostname that matches server certificate CN

    auto clientSocket = std::make_shared<UdpSocket>(eventBase);
    EXPECT_EQ(clientSocket->Connect(kServerAddr, kServerPort + 1), 0);

    DtlsSession dtlsClient{eventBase, false, clientSocket};
    EXPECT_EQ(dtlsClient.Init(config), ErrorCode::kNone);

    auto clientConnected = [&](DtlsSession &aSession, Error aError) {
        EXPECT_EQ(aError, ErrorCode::kNone);
        EXPECT_EQ(aSession.GetState(), DtlsSession::State::kConnected);
        connectionSucceeded = true;
        EXPECT_EQ(aSession.Send(kHello, MessageSubType::kNone), ErrorCode::kNone);
    };
    dtlsClient.Connect(clientConnected);

    // Set a timeout to prevent hanging
    struct timeval timeout = {5, 0}; // 5 seconds
    event_base_loopexit(eventBase, &timeout);

    int fail = event_base_loop(eventBase, EVLOOP_NO_EXIT_ON_EMPTY);
    ASSERT_EQ(fail, 0);

    // Verify that the connection was established and message was received
    EXPECT_TRUE(connectionSucceeded);
    EXPECT_TRUE(messageReceived);

    event_base_free(eventBase);
}

TEST(DtlsTest, ClientHostnameVerificationInvalid)
{
    bool connectionFailed = false;

    DtlsConfig config;

    // Setup dtls server
    SetUpServerConfig(config);

    auto eventBase = CreateEventBase();

    auto serverSocket = std::make_shared<UdpSocket>(eventBase);
    EXPECT_EQ(serverSocket->Bind(kServerAddr, kServerPort + 2), 0); // Use different port

    DtlsSession dtlsServer{eventBase, true, serverSocket};
    EXPECT_EQ(dtlsServer.Init(config), ErrorCode::kNone);

    dtlsServer.SetReceiver([&](Endpoint &, const ByteArray &) {
        FAIL() << "Server should not receive data from client with invalid hostname";
    });

    auto serverConnected = [&](const DtlsSession &, Error) {
        FAIL() << "Server should not connect to client with invalid hostname";
        event_base_loopbreak(eventBase);
    };
    dtlsServer.Connect(serverConnected);

    // Setup dtls client with invalid hostname verification
    SetUpClientConfig(config);
    config.mHostname = "InvalidHostname"; // Use incorrect hostname

    auto clientSocket = std::make_shared<UdpSocket>(eventBase);
    EXPECT_EQ(clientSocket->Connect(kServerAddr, kServerPort + 2), 0);

    DtlsSession dtlsClient{eventBase, false, clientSocket};
    EXPECT_EQ(dtlsClient.Init(config), ErrorCode::kNone);

    auto clientConnected = [&](DtlsSession &aSession, Error aError) {
        EXPECT_NE(aError.GetCode(), ErrorCode::kNone);
        EXPECT_EQ(aSession.GetState(), DtlsSession::State::kDisconnected);
        connectionFailed = true;
        event_base_loopbreak(eventBase);
    };
    dtlsClient.Connect(clientConnected);

    // Set a timeout to prevent hanging
    struct timeval timeout = {5, 0}; // 5 seconds
    event_base_loopexit(eventBase, &timeout);

    int fail = event_base_loop(eventBase, EVLOOP_NO_EXIT_ON_EMPTY);
    ASSERT_EQ(fail, 0);

    // Verify that the connection failed
    EXPECT_TRUE(connectionFailed);

    event_base_free(eventBase);
}

} // namespace commissioner

} // namespace ot
