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
 *   This file defines test cases for DTLS implementation with mbedtls.
 */

#include <catch2/catch.hpp>

#include "library/socket.hpp"

namespace ot {

namespace commissioner {

static std::string kServerAddr = "::";
static uint16_t    kServerPort = 9527;
static std::string kClientAddr = "::";
static uint16_t    kClientPort = 12345;

TEST_CASE("UDP-socket-hello", "[socket]")
{
    const ByteArray kHello{'h', 'e', 'l', 'l', 'o'};
    const ByteArray kWorld{'w', 'o', 'r', 'l', 'd'};

    auto eventBase = event_base_new();
    REQUIRE(eventBase != nullptr);

    UdpSocket serverSocket{eventBase};
    serverSocket.SetEventHandler([&](short aFlags) {
        uint8_t buf[1024];
        int     len;

        REQUIRE(serverSocket.GetLocalPort() == kServerPort);

        if (aFlags & EV_READ)
        {
            REQUIRE((len = serverSocket.Receive(buf, sizeof(buf))) > 0);
            REQUIRE(ByteArray{buf, buf + len} == kHello);

            REQUIRE((len = serverSocket.Send(&kWorld[0], kWorld.size())) > 0);
            REQUIRE(static_cast<size_t>(len) == kWorld.size());
        }
    });
    REQUIRE(serverSocket.Bind(kServerAddr, kServerPort) == 0);
    REQUIRE(serverSocket.GetLocalPort() == kServerPort);

    UdpSocket clientSocket{eventBase};
    clientSocket.SetEventHandler([&](short aFlags) {
        uint8_t buf[1024];
        int     len;

        if (aFlags & EV_READ)
        {
            REQUIRE((len = clientSocket.Receive(buf, sizeof(buf))) > 0);
            REQUIRE(ByteArray{buf, buf + len} == kWorld);

            event_base_loopbreak(eventBase);
        }
    });

    REQUIRE(clientSocket.Connect(kServerAddr, kServerPort) == 0);
    REQUIRE(clientSocket.GetPeerPort() == kServerPort);
    REQUIRE(clientSocket.Send(&kHello[0], kHello.size()) == kHello.size());

    REQUIRE(event_base_loop(eventBase, EVLOOP_NO_EXIT_ON_EMPTY) == 0);
    event_base_free(eventBase);
}

TEST_CASE("mock-socket-hello", "[socket]")
{
    const ByteArray kHello{'h', 'e', 'l', 'l', 'o'};
    const ByteArray kWorld{'w', 'o', 'r', 'l', 'd'};

    auto eventBase = event_base_new();
    REQUIRE(eventBase != nullptr);

    auto clientSocket = std::make_shared<MockSocket>(eventBase, Address::FromString(kClientAddr), kClientPort);
    auto serverSocket = std::make_shared<MockSocket>(eventBase, Address::FromString(kServerAddr), kServerPort);

    clientSocket->Connect(serverSocket);
    serverSocket->Connect(clientSocket);

    REQUIRE(clientSocket->IsConnected());
    REQUIRE(serverSocket->IsConnected());

    REQUIRE(clientSocket->GetPeerAddr() == serverSocket->GetLocalAddr());
    REQUIRE(clientSocket->GetPeerPort() == serverSocket->GetLocalPort());
    REQUIRE(clientSocket->GetLocalAddr() == serverSocket->GetPeerAddr());
    REQUIRE(clientSocket->GetLocalPort() == serverSocket->GetPeerPort());

    serverSocket->SetEventHandler([&](short aFlags) {
        if (aFlags & EV_READ)
        {
            uint8_t buf[1024];
            int     len = serverSocket->Receive(buf, sizeof(buf));
            REQUIRE(len > 0);
            REQUIRE(static_cast<size_t>(len) == kHello.size());

            len = serverSocket->Send(&kWorld[0], kWorld.size());
            REQUIRE(len > 0);
            REQUIRE(static_cast<size_t>(len) == kWorld.size());
        }
    });
    clientSocket->SetEventHandler([&](short aFlags) {
        if (aFlags & EV_READ)
        {
            uint8_t buf[1024];
            int     len = clientSocket->Receive(buf, sizeof(buf));
            REQUIRE(len > 0);
            REQUIRE(static_cast<size_t>(len) == kWorld.size());

            event_base_loopbreak(eventBase);
        }
    });

    int len = clientSocket->Send(&kHello[0], kHello.size());
    REQUIRE(len > 0);
    REQUIRE(static_cast<size_t>(len) == kHello.size());

    REQUIRE(event_base_loop(eventBase, EVLOOP_NO_EXIT_ON_EMPTY) == 0);
    event_base_free(eventBase);
}

} // namespace commissioner

} // namespace ot
