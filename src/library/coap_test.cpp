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

#include "library/coap.hpp"

#include <catch2/catch.hpp>

namespace ot {

namespace commissioner {

namespace coap {

TEST_CASE("coap-message-header", "[coap]")
{
    SECTION("serialize default constructed message")
    {
        auto      message = std::make_shared<Message>(Type::kAcknowledgment, Code::kGet);
        ByteArray buffer;
        Error     error;
        REQUIRE(message->Serialize(buffer).NoError());
        REQUIRE(buffer.size() == 4);
        REQUIRE(buffer[0] == ((1 << 6) | (utils::to_underlying(message->GetType()) << 4) | 0));
        REQUIRE(buffer[1] == utils::to_underlying(message->GetCode()));
        REQUIRE(buffer[2] == 0);
        REQUIRE(buffer[3] == 0);

        message = Message::Deserialize(error, buffer);
        REQUIRE(message != nullptr);
        REQUIRE(error.NoError());

        REQUIRE(message->GetType() == Type::kAcknowledgment);
        REQUIRE(message->GetCode() == Code::kGet);
    }

    SECTION("incomplete input buffer")
    {
        ByteArray buffer{0xcc};
        Error     error;
        REQUIRE(Message::Deserialize(error, buffer) == nullptr);
        REQUIRE(error.GetCode() == ErrorCode::kBadFormat);
    }

    SECTION("minimum Message header with invalid Version")
    {
        ByteArray buffer{0xc0, 0x00, 0x00, 0x00};
        Error     error;
        REQUIRE(Message::Deserialize(error, buffer) == nullptr);
        REQUIRE(error.GetCode() == ErrorCode::kBadFormat);
    }

    SECTION("minimum Message header with valid Version")
    {
        ByteArray buffer{0x40, 0x00, 0x00, 0x00};
        Error     error;
        auto      message = Message::Deserialize(error, buffer);
        REQUIRE(message != nullptr);
        REQUIRE(error.NoError());

        REQUIRE(message->GetVersion() == kVersion1);
        REQUIRE(message->GetType() == Type::kConfirmable);
        REQUIRE(message->GetToken().size() == 0);
        REQUIRE(message->GetCode() == utils::from_underlying<Code>(0));
        REQUIRE(message->GetMessageId() == 0);
    }

    SECTION("non-zero token length but token is missing")
    {
        ByteArray buffer{0x41, 0x00, 0x00, 0x00};
        Error     error;
        REQUIRE(Message::Deserialize(error, buffer) == nullptr);
        REQUIRE(error.GetCode() == ErrorCode::kBadFormat);
    }

    SECTION("non-zero token length and token is provided")
    {
        ByteArray buffer{0x41, 0x00, 0x00, 0x00, 0xfa};
        Error     error;

        auto message = Message::Deserialize(error, buffer);
        REQUIRE(message != nullptr);
        REQUIRE(error.NoError());

        REQUIRE(message->GetToken() == ByteArray{0xfa});
    }

    SECTION("token length tool long")
    {
        ByteArray buffer{0x49, 0x00, 0x00, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
        Error     error;

        REQUIRE(Message::Deserialize(error, buffer) == nullptr);
        REQUIRE(error.GetCode() == ErrorCode::kBadFormat);
    }
}

TEST_CASE("coap-message-options", "[coap]")
{
    SECTION("silently ignore option with invalid option number '0'")
    {
        ByteArray buffer{0x40, 0x00, 0x00, 0x00, 0x00};
        Error     error;

        auto message = Message::Deserialize(error, buffer);
        REQUIRE(message != nullptr);
        REQUIRE(error.NoError());

        REQUIRE(message->GetOptionNum() == 0);
    }

    SECTION("silently ignore unrecognized elective option")
    {
        ByteArray buffer{0x40, 0x00, 0x00, 0x00, 0xc3, 0x11, 0x22, 0x33};
        Error     error;

        auto message = Message::Deserialize(error, buffer);
        REQUIRE(message != nullptr);
        REQUIRE(error.NoError());

        REQUIRE(message->GetOptionNum() == 0);
    }

    SECTION("reject if unrecognized option is critical")
    {
        ByteArray buffer{0x40, 0x00, 0x00, 0x00, 0x19, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
        Error     error;

        auto message = Message::Deserialize(error, buffer);
        REQUIRE(message == nullptr);
        REQUIRE(error.GetCode() == ErrorCode::kBadFormat);
    }

    SECTION("single option serialization & deserialization")
    {
        auto      message = std::make_shared<Message>(Type::kConfirmable, Code::kGet);
        ByteArray buffer;
        Error     error;

        REQUIRE(message->SetContentFormat(ContentFormat::kCBOR).NoError());
        REQUIRE(message->Serialize(buffer).NoError());

        REQUIRE(buffer.size() == 4 + 2);
        REQUIRE(buffer[4] == 0xc1);
        REQUIRE(buffer[5] == utils::to_underlying(ContentFormat::kCBOR));

        message = Message::Deserialize(error, buffer);
        REQUIRE(message != nullptr);
        REQUIRE(error.NoError());

        REQUIRE(message->GetOptionNum() == 1);

        ContentFormat contentFormat;
        REQUIRE(message->GetContentFormat(contentFormat).NoError());
        REQUIRE(contentFormat == ContentFormat::kCBOR);
    }

    SECTION("single URI path option serialization & deserialization") {}

    SECTION("multiple options serialization & deserialization")
    {
        auto      message = std::make_shared<Message>(Type::kConfirmable, Code::kGet);
        ByteArray buffer;
        Error     error;

        REQUIRE(message->SetUriPath(".well-known/est/rv/").NoError());
        REQUIRE(message->SetContentFormat(ContentFormat::kCBOR).NoError());
        REQUIRE(message->SetAccept(ContentFormat::kCoseSign1).NoError());
        REQUIRE(message->GetOptionNum() == 3);
        REQUIRE(message->Serialize(buffer).NoError());

        auto expectedBuffer = ByteArray{0x40, 0x01,
                                        0x00, 0x00, // header
                                        0xbb, '.',
                                        'w',  'e',
                                        'l',  'l',
                                        '-',  'k',
                                        'n',  'o',
                                        'w',  'n', // uri-path
                                        0x03, 'e',
                                        's',  't', // uri-path
                                        0x02, 'r',
                                        'v',                                              // uri-path
                                        0x11, utils::to_underlying(ContentFormat::kCBOR), // content-format
                                        0x51, utils::to_underlying(ContentFormat::kCoseSign1)};

        REQUIRE(buffer == expectedBuffer);

        message = Message::Deserialize(error, buffer);
        REQUIRE(message != nullptr);
        REQUIRE(error.NoError());

        REQUIRE(message->GetOptionNum() == 3);

        ContentFormat contentFormat;
        REQUIRE(message->GetContentFormat(contentFormat).NoError());
        REQUIRE(contentFormat == ContentFormat::kCBOR);

        std::string uriPath;
        REQUIRE(message->GetUriPath(uriPath).NoError());

        // ".well-known/est/rv/" is normalized to "/.well-known/est/rv"
        REQUIRE(uriPath == "/.well-known/est/rv");

        ContentFormat accept;
        REQUIRE(message->GetAccept(accept).NoError());
        REQUIRE(accept == ContentFormat::kCoseSign1);
    }
}

TEST_CASE("coap-message-payload", "[coap]")
{
    SECTION("payload marker present but there is no payload")
    {
        ByteArray buffer{0x40, 0x00, 0x00, 0x00, 0xFF};
        Error     error;
        auto      message = Message::Deserialize(error, buffer);
        REQUIRE(message == nullptr);
        REQUIRE(error.GetCode() == ErrorCode::kBadFormat);
    }

    SECTION("payload marker present and there is payload")
    {
        ByteArray buffer{0x40, 0x00, 0x00, 0x00, 0xFF, 0xfa, 0xce};
        Error     error;
        auto      message = Message::Deserialize(error, buffer);
        REQUIRE(message != nullptr);
        REQUIRE(error.NoError());
        REQUIRE(message->GetPayload() == ByteArray{0xfa, 0xce});
    }

    SECTION("non-empty payload serialization / deserialization")
    {
        Message message{Type::kConfirmable, Code::kDelete};
        message.Append("hello");

        ByteArray buffer;
        REQUIRE(message.Serialize(buffer).NoError());

        Error error;
        auto  msg = Message::Deserialize(error, buffer);
        REQUIRE(msg != nullptr);
        REQUIRE(error.NoError());
        REQUIRE(msg->GetPayload() == ByteArray{'h', 'e', 'l', 'l', 'o'});
    }
}

class MockEndpoint : public Endpoint
{
public:
    MockEndpoint(struct event_base *aEventBase, const Address &aAddr, uint16_t aPort)
        : mAddr(aAddr)
        , mPort(aPort)
        , mPeer(nullptr)
        , mDropMessage(false)
    {
        event_assign(&mSendEvent, aEventBase, -1, EV_PERSIST, SendEventCallback, this);
        event_add(&mSendEvent, nullptr);
    }

    ~MockEndpoint() override {}

    void SetPeer(MockEndpoint *aPeer) { mPeer = aPeer; }

    Error Send(const ByteArray &aBuf, MessageSubType aSubType) override
    {
        (void)aSubType;

        if (!mDropMessage)
        {
            mSendQueue.emplace(aBuf);
            event_active(&mPeer->mSendEvent, 0, 0);
        }
        return Error();
    }

    Address GetPeerAddr() const override { return mPeer->mAddr; }

    uint16_t GetPeerPort() const override { return mPeer->mPort; }

    void SetDropMessage(bool aDropMessage) { mDropMessage = aDropMessage; }

private:
    static void SendEventCallback(evutil_socket_t, short, void *aEndpoint)
    {
        auto endpoint = reinterpret_cast<MockEndpoint *>(aEndpoint);
        while (!endpoint->mPeer->mSendQueue.empty())
        {
            auto &packet = endpoint->mPeer->mSendQueue.front();
            endpoint->mReceiver(*endpoint, packet);
            endpoint->mPeer->mSendQueue.pop();
        }
    }

    struct event mSendEvent;

    Address               mAddr;
    uint16_t              mPort;
    MockEndpoint *        mPeer;
    bool                  mDropMessage;
    std::queue<ByteArray> mSendQueue;
};

TEST_CASE("coap-message-confirmable", "[coap]")
{
    Address localhost;
    REQUIRE(localhost.Set("127.0.0.1").NoError());

    auto eventBase = event_base_new();
    REQUIRE(eventBase != nullptr);

    MockEndpoint peer0{eventBase, localhost, 5683};
    MockEndpoint peer1{eventBase, localhost, 5684};
    peer0.SetPeer(&peer1);
    peer1.SetPeer(&peer0);

    Coap coap0{eventBase, peer0};
    Coap coap1{eventBase, peer1};

    REQUIRE(
        coap1
            .AddResource(
                {"/hello",
                 [&coap1](const Request &aRequest) {
                     REQUIRE(aRequest.IsRequest());
                     REQUIRE(aRequest.GetType() == Type::kConfirmable);
                     REQUIRE(aRequest.GetCode() == Code::kGet);

                     ContentFormat contentFormat;
                     REQUIRE(aRequest.GetContentFormat(contentFormat).NoError());
                     REQUIRE(contentFormat == ContentFormat::kTextPlain);

                     std::string uriPath;
                     REQUIRE(aRequest.GetUriPath(uriPath).NoError());
                     REQUIRE(uriPath == "/hello");

                     REQUIRE(aRequest.GetPayload() == ByteArray{'h', 'e', 'l', 'l', 'o', ',', ' ', 'C', 'o', 'A', 'P'});

                     Response response{Type::kAcknowledgment, Code::kContent};
                     REQUIRE(response.SetContentFormat(ContentFormat::kTextPlain).NoError());
                     response.Append("Ack...");
                     REQUIRE(coap1.SendResponse(aRequest, response).NoError());
                 }})
            .NoError());

    SECTION("basic confirmable message send/recv")
    {
        Message request{Type::kConfirmable, Code::kGet};
        REQUIRE(request.SetUriPath("/hello").NoError());
        REQUIRE(request.SetContentFormat(ContentFormat::kTextPlain).NoError());
        request.Append("hello, CoAP");

        coap0.SendRequest(request, [&eventBase](const Response *aResponse, Error aError) {
            REQUIRE(aResponse != nullptr);
            REQUIRE(aError.NoError());

            REQUIRE(aResponse->GetType() == Type::kAcknowledgment);

            ContentFormat contentFormat;
            REQUIRE(aResponse->GetContentFormat(contentFormat).NoError());
            REQUIRE(contentFormat == ContentFormat::kTextPlain);

            REQUIRE(aResponse->GetPayload() == ByteArray{'A', 'c', 'k', '.', '.', '.'});

            event_base_loopbreak(eventBase);
        });
    }

    SECTION("basic confirmable message retransmission")
    {
        Message request{Type::kConfirmable, Code::kGet};
        REQUIRE(request.SetUriPath("/hello").NoError());
        REQUIRE(request.SetContentFormat(ContentFormat::kTextPlain).NoError());
        request.Append("hello, CoAP");

        // Drop the message to trigger retransmission
        peer0.SetDropMessage(true);
        coap0.SendRequest(request, [&eventBase](const Response *aResponse, Error aError) {
            REQUIRE(aResponse != nullptr);
            REQUIRE(aError.NoError());

            REQUIRE(aResponse->GetType() == Type::kAcknowledgment);

            ContentFormat contentFormat;
            REQUIRE(aResponse->GetContentFormat(contentFormat).NoError());
            REQUIRE(contentFormat == ContentFormat::kTextPlain);

            REQUIRE(aResponse->GetPayload() == ByteArray{'A', 'c', 'k', '.', '.', '.'});

            event_base_loopbreak(eventBase);
        });
        peer0.SetDropMessage(false);
    }

    SECTION("basic confirmable message timeout")
    {
        Message request{Type::kConfirmable, Code::kGet};
        REQUIRE(request.SetUriPath("/hello").NoError());
        REQUIRE(request.SetContentFormat(ContentFormat::kTextPlain).NoError());
        request.Append("hello, CoAP");

        // Drop the message to trigger timeout
        peer0.SetDropMessage(true);
        coap0.SendRequest(request, [&eventBase](const Response *aResponse, Error aError) {
            REQUIRE(aResponse == nullptr);
            REQUIRE(aError.GetCode() == ErrorCode::kTimeout);

            event_base_loopbreak(eventBase);
        });
    }

    REQUIRE(event_base_loop(eventBase, EVLOOP_NO_EXIT_ON_EMPTY) == 0);
    event_base_free(eventBase);
}

// TODO(wgtdkp): test with multiple outstanding requests / pressure tests.

// TODO(wgtdkp): add test cases to cover all CoAP APIs.

} // namespace coap

} // namespace commissioner

} // namespace ot
