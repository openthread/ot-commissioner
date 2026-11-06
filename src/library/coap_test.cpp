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

#include <cstdint>
#include <memory>
#include <queue>
#include <string>

#include "commissioner/defines.hpp"
#include "commissioner/error.hpp"
#include "common/address.hpp"
#include "common/error_macros.hpp"
#include "common/utils.hpp"
#include "event2/event.h"
#include "event2/event_struct.h"
#include "event2/util.h"
#include "gtest/gtest.h"
#include "library/coap.hpp"
#include "library/endpoint.hpp"
#include "library/message.hpp"

namespace ot {

namespace commissioner {

namespace coap {

TEST(CoapTest, CoapMessageHeader_SerializeDefaultConstructedMessage)
{
    auto      message = std::make_shared<Message>(Type::kAcknowledgment, Code::kGet);
    ByteArray buffer;
    Error     error;

    EXPECT_EQ(message->Serialize(buffer), ErrorCode::kNone);
    EXPECT_EQ(buffer.size(), 4);
    EXPECT_EQ(buffer[0], ((1 << 6) | (utils::to_underlying(message->GetType()) << 4) | 0));
    EXPECT_EQ(buffer[1], utils::to_underlying(message->GetCode()));
    EXPECT_EQ(buffer[2], 0);
    EXPECT_EQ(buffer[3], 0);

    message = Message::Deserialize(error, buffer);
    EXPECT_NE(message, nullptr);
    EXPECT_EQ(error, ErrorCode::kNone);

    EXPECT_EQ(message->GetType(), Type::kAcknowledgment);
    EXPECT_EQ(message->GetCode(), Code::kGet);
}

TEST(CoapTest, CoapMessageHeader_IncompleteInputBuffer)
{
    ByteArray buffer{0xcc};
    Error     error;

    EXPECT_EQ(Message::Deserialize(error, buffer), nullptr);
    EXPECT_EQ(error, ErrorCode::kBadFormat);
}

TEST(CoapTest, CoapMessageHeader_InvalidVersion)
{
    ByteArray buffer{0xc0, 0x00, 0x00, 0x00};
    Error     error;

    EXPECT_EQ(Message::Deserialize(error, buffer), nullptr);
    EXPECT_EQ(error, ErrorCode::kBadFormat);
}

TEST(CoapTest, CoapMessageHeader_ValidVersion)
{
    ByteArray buffer{0x40, 0x00, 0x00, 0x00};
    Error     error;
    auto      message = Message::Deserialize(error, buffer);

    EXPECT_NE(message, nullptr);
    EXPECT_EQ(error, ErrorCode::kNone);

    EXPECT_EQ(message->GetVersion(), kVersion1);
    EXPECT_EQ(message->GetType(), Type::kConfirmable);
    EXPECT_EQ(message->GetToken().size(), 0);
    EXPECT_EQ(message->GetCode(), utils::from_underlying<Code>(0));
    EXPECT_EQ(message->GetMessageId(), 0);
}

TEST(CoapTest, CoapMessageHeader_TokenIsMissing)
{
    ByteArray buffer{0x41, 0x00, 0x00, 0x00};
    Error     error;

    EXPECT_EQ(Message::Deserialize(error, buffer), nullptr);
    EXPECT_EQ(error, ErrorCode::kBadFormat);
}

TEST(CoapTest, CoapMessageHeader_TokenIsPresent)
{
    ByteArray buffer{0x41, 0x00, 0x00, 0x00, 0xfa};
    Error     error;
    auto      message = Message::Deserialize(error, buffer);

    EXPECT_NE(message, nullptr);
    EXPECT_EQ(error, ErrorCode::kNone);

    EXPECT_EQ(message->GetToken(), ByteArray{0xfa});
}

TEST(CoapTest, CoapMessageHeader_TokenLengthIsZero)
{
    ByteArray buffer{'`', 0, 0, 1};
    Error     error;
    auto      message = Message::Deserialize(error, buffer);

    EXPECT_NE(message, nullptr);
    EXPECT_EQ(error, ErrorCode::kNone);

    EXPECT_EQ(message->GetToken(), ByteArray{});
}

TEST(CoapTest, CoapMessageHeader_TokenLengthIsTooLong)
{
    ByteArray buffer{0x49, 0x00, 0x00, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
    Error     error;

    EXPECT_EQ(Message::Deserialize(error, buffer), nullptr);
    EXPECT_EQ(error, ErrorCode::kBadFormat);
}

TEST(CoapTest, CoapMessageOptions_InvalidOptionNumber)
{
    ByteArray buffer{0x40, 0x00, 0x00, 0x00, 0x00};
    Error     error;

    auto message = Message::Deserialize(error, buffer);
    EXPECT_NE(message, nullptr);
    EXPECT_EQ(error, ErrorCode::kNone);

    EXPECT_EQ(message->GetOptionNum(), 0);
}

TEST(CoapTest, CoapMessageOptions_UnrecognizedElectiveOption)
{
    ByteArray buffer{0x40, 0x00, 0x00, 0x00, 0xc3, 0x11, 0x22, 0x33};
    Error     error;

    auto message = Message::Deserialize(error, buffer);

    EXPECT_NE(message, nullptr);
    EXPECT_EQ(error, ErrorCode::kNone);
    EXPECT_EQ(message->GetOptionNum(), 0);
}

TEST(CoapTest, CoapMessageOptions_UnrecognizedCriticalOption)
{
    ByteArray buffer{0x40, 0x00, 0x00, 0x00, 0x19, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
    Error     error;

    auto message = Message::Deserialize(error, buffer);

    EXPECT_EQ(message, nullptr);
    EXPECT_EQ(error, ErrorCode::kBadFormat);
}

TEST(CoapTest, CoapMessageOptions_SingleOptionSerializationAndDeserialization)
{
    auto      message = std::make_shared<Message>(Type::kConfirmable, Code::kGet);
    ByteArray buffer;
    Error     error;

    EXPECT_EQ(message->SetContentFormat(ContentFormat::kCBOR), ErrorCode::kNone);
    EXPECT_EQ(message->Serialize(buffer), ErrorCode::kNone);

    EXPECT_EQ(buffer.size(), 4 + 2);
    EXPECT_EQ(buffer[4], 0xc1);
    EXPECT_EQ(buffer[5], utils::to_underlying(ContentFormat::kCBOR));

    message = Message::Deserialize(error, buffer);
    EXPECT_NE(message, nullptr);
    EXPECT_EQ(error, ErrorCode::kNone);

    EXPECT_EQ(message->GetOptionNum(), 1);

    ContentFormat contentFormat;
    EXPECT_EQ(message->GetContentFormat(contentFormat), ErrorCode::kNone);
    EXPECT_EQ(contentFormat, ContentFormat::kCBOR);
}

TEST(CoapTest, CoapMessageOptions_SingleUrlPathOptionSerializationAndDeserialization)
{
    // TODO(wgtdkp):
}

TEST(CoapTest, CoapMessageOptions_MultipleOptionsSerializationAndDeserialization)
{
    auto      message = std::make_shared<Message>(Type::kConfirmable, Code::kGet);
    ByteArray buffer;
    Error     error;

    EXPECT_EQ(message->SetUriPath(".well-known/est/rv/"), ErrorCode::kNone);
    EXPECT_EQ(message->SetContentFormat(ContentFormat::kCBOR), ErrorCode::kNone);
    EXPECT_EQ(message->SetAccept(ContentFormat::kCoseSign1), ErrorCode::kNone);
    EXPECT_EQ(message->GetOptionNum(), 3);
    EXPECT_EQ(message->Serialize(buffer), ErrorCode::kNone);

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

    EXPECT_EQ(buffer, expectedBuffer);

    message = Message::Deserialize(error, buffer);
    EXPECT_NE(message, nullptr);
    EXPECT_EQ(error, ErrorCode::kNone);

    EXPECT_EQ(message->GetOptionNum(), 3);

    ContentFormat contentFormat;
    EXPECT_EQ(message->GetContentFormat(contentFormat), ErrorCode::kNone);
    EXPECT_EQ(contentFormat, ContentFormat::kCBOR);

    std::string uriPath;
    EXPECT_EQ(message->GetUriPath(uriPath), ErrorCode::kNone);

    // ".well-known/est/rv/" is normalized to "/.well-known/est/rv"
    EXPECT_EQ(uriPath, "/.well-known/est/rv");

    ContentFormat accept;
    EXPECT_EQ(message->GetAccept(accept), ErrorCode::kNone);
    EXPECT_EQ(accept, ContentFormat::kCoseSign1);
}

TEST(CoapTest, CoapMessagePayload_payloadIsMissing)
{
    ByteArray buffer{0x40, 0x00, 0x00, 0x00, 0xFF};
    Error     error;
    auto      message = Message::Deserialize(error, buffer);

    EXPECT_EQ(message, nullptr);
    EXPECT_EQ(error, ErrorCode::kBadFormat);
}

TEST(CoapTest, CoapMessagePayload_payloadIsPresent)
{
    ByteArray buffer{0x40, 0x00, 0x00, 0x00, 0xFF, 0xfa, 0xce};
    Error     error;
    auto      message = Message::Deserialize(error, buffer);

    EXPECT_NE(message, nullptr);
    EXPECT_EQ(error, ErrorCode::kNone);
    EXPECT_EQ(message->GetPayload(), (ByteArray{0xfa, 0xce}));
}

TEST(CoapTest, CoapMessagePayload_NonEmptyPayloadSerializationAndDeserialization)
{
    ByteArray buffer;
    Error     error;

    Message message{Type::kConfirmable, Code::kDelete};
    message.Append("hello");
    EXPECT_EQ(message.Serialize(buffer), ErrorCode::kNone);

    auto msg = Message::Deserialize(error, buffer);

    EXPECT_NE(msg, nullptr);
    EXPECT_EQ(error, ErrorCode::kNone);
    EXPECT_EQ(msg->GetPayloadAsString(), "hello");
}

class MessageIdEndpoint : public Endpoint
{
public:
    Error Send(const ByteArray &aBuf, MessageSubType) override
    {
        Error error;
        mMessage = Message::Deserialize(error, aBuf);
        return error;
    }

    Address GetPeerAddr() const override { return {}; }

    uint16_t GetPeerPort() const override { return 5683; }

    std::shared_ptr<Message> mMessage;
};

class CoapMessageIdTest : public testing::Test
{
protected:
    void SetUp() override { ASSERT_NE(mEventBase, nullptr); }

    std::unique_ptr<event_base, decltype(&event_base_free)> mEventBase{event_base_new(), event_base_free};
    MessageIdEndpoint                                       mEndpoint;
};

TEST_F(CoapMessageIdTest, SequentialRequests)
{
    Coap    coap{mEventBase.get(), mEndpoint};
    Request request{Type::kNonConfirmable, Code::kGet};

    coap.SendRequest(request, nullptr);
    ASSERT_NE(mEndpoint.mMessage, nullptr);
    uint16_t messageId = mEndpoint.mMessage->GetMessageId();

    for (uint32_t i = 0; i <= UINT16_MAX; ++i)
    {
        coap.SendRequest(request, nullptr);
        ASSERT_NE(mEndpoint.mMessage, nullptr);
        EXPECT_EQ(mEndpoint.mMessage->GetMessageId(), static_cast<uint16_t>(messageId + 1));
        messageId = mEndpoint.mMessage->GetMessageId();
    }
}

TEST_F(CoapMessageIdTest, RandomizedInitialMessageIds)
{
    uint16_t firstMessageId = 0;
    bool     different      = false;

    for (uint8_t i = 0; i < 8; ++i)
    {
        Coap    coap{mEventBase.get(), mEndpoint};
        Request request{Type::kNonConfirmable, Code::kGet};

        coap.SendRequest(request, nullptr);
        ASSERT_NE(mEndpoint.mMessage, nullptr);

        if (i == 0)
        {
            firstMessageId = mEndpoint.mMessage->GetMessageId();
        }
        else if (mEndpoint.mMessage->GetMessageId() != firstMessageId)
        {
            different = true;
        }
    }

    EXPECT_TRUE(different);
}

TEST_F(CoapMessageIdTest, ResponseMessageIds)
{
    Coap    coap{mEventBase.get(), mEndpoint};
    Request request{Type::kNonConfirmable, Code::kGet};

    coap.SendRequest(request, nullptr);
    ASSERT_NE(mEndpoint.mMessage, nullptr);
    uint16_t messageId = mEndpoint.mMessage->GetMessageId();

    Error error;
    auto  receivedRequest = Message::Deserialize(error, ByteArray{0x40, 0x01, 0x12, 0x34});
    ASSERT_EQ(error, ErrorCode::kNone);
    ASSERT_NE(receivedRequest, nullptr);

    for (Type type : {Type::kAcknowledgment, Type::kConfirmable, Type::kNonConfirmable})
    {
        Response response{type, Code::kContent};

        ASSERT_EQ(coap.SendResponse(*receivedRequest, response), ErrorCode::kNone);
        ASSERT_NE(mEndpoint.mMessage, nullptr);

        if (type == Type::kAcknowledgment)
        {
            EXPECT_EQ(mEndpoint.mMessage->GetMessageId(), receivedRequest->GetMessageId());
        }
        else
        {
            EXPECT_EQ(mEndpoint.mMessage->GetMessageId(), static_cast<uint16_t>(++messageId));
        }
    }

    request.SetType(Type::kConfirmable);
    ASSERT_EQ(coap.SendHeaderResponse(Code::kContent, request), ErrorCode::kNone);
    ASSERT_NE(mEndpoint.mMessage, nullptr);
    EXPECT_EQ(mEndpoint.mMessage->GetType(), Type::kAcknowledgment);
    EXPECT_EQ(mEndpoint.mMessage->GetMessageId(), 0);
}

TEST_F(CoapMessageIdTest, NonConfirmableResponseWrapsToZero)
{
    Coap    coap{mEventBase.get(), mEndpoint};
    Request request{Type::kNonConfirmable, Code::kGet};

    for (uint32_t i = 0; i <= UINT16_MAX; ++i)
    {
        coap.SendRequest(request, nullptr);
        ASSERT_NE(mEndpoint.mMessage, nullptr);
        if (mEndpoint.mMessage->GetMessageId() == UINT16_MAX)
        {
            break;
        }
    }
    ASSERT_EQ(mEndpoint.mMessage->GetMessageId(), UINT16_MAX);

    Error error;
    auto  receivedRequest = Message::Deserialize(error, ByteArray{0x50, 0x01, 0x12, 0x34});
    ASSERT_EQ(error, ErrorCode::kNone);
    ASSERT_NE(receivedRequest, nullptr);
    ASSERT_EQ(coap.SendHeaderResponse(Code::kContent, *receivedRequest), ErrorCode::kNone);
    ASSERT_NE(mEndpoint.mMessage, nullptr);
    EXPECT_EQ(mEndpoint.mMessage->GetType(), Type::kNonConfirmable);
    EXPECT_EQ(mEndpoint.mMessage->GetMessageId(), 0);

    Response response{Type::kNonConfirmable, Code::kContent};
    ASSERT_EQ(coap.SendResponse(*receivedRequest, response), ErrorCode::kNone);
    ASSERT_NE(mEndpoint.mMessage, nullptr);
    EXPECT_EQ(mEndpoint.mMessage->GetMessageId(), 1);
}

TEST_F(CoapMessageIdTest, SeparateResponseIsNotReplayedForPeerRequest)
{
    for (Type type : {Type::kConfirmable, Type::kNonConfirmable})
    {
        SCOPED_TRACE(static_cast<int>(type));
        Coap     coap{mEventBase.get(), mEndpoint};
        uint32_t handled = 0;

        coap.SendRequest(Request{Type::kNonConfirmable, Code::kGet}, nullptr);
        ASSERT_NE(mEndpoint.mMessage, nullptr);
        uint16_t peerMessageId = static_cast<uint16_t>(mEndpoint.mMessage->GetMessageId() + 2);

        coap.SetDefaultHandler([&](const Request &aRequest) {
            Response response{type, ++handled == 1 ? Code::kContent : Code::kChanged};
            EXPECT_EQ(coap.SendResponse(aRequest, response), ErrorCode::kNone);
        });

        ByteArray request{0x41, 0x01, 0x00, 0x00, 0xaa, 0xb4, 't', 'e', 's', 't'};
        request[2] = static_cast<uint8_t>(peerMessageId >> 8);
        request[3] = static_cast<uint8_t>(peerMessageId);
        coap.Receive(request);
        ASSERT_EQ(handled, 1);
        ASSERT_NE(mEndpoint.mMessage, nullptr);

        peerMessageId = mEndpoint.mMessage->GetMessageId();
        request[2]    = static_cast<uint8_t>(peerMessageId >> 8);
        request[3]    = static_cast<uint8_t>(peerMessageId);
        request[4]    = 0xbb;
        mEndpoint.mMessage.reset();
        coap.Receive(request);

        EXPECT_EQ(handled, 2);
        ASSERT_NE(mEndpoint.mMessage, nullptr);
        EXPECT_EQ(mEndpoint.mMessage->GetToken(), ByteArray{0xbb});
        EXPECT_EQ(mEndpoint.mMessage->GetCode(), Code::kChanged);
        EXPECT_EQ(coap.GetCachedResponsesNum(), 0);
    }
}

TEST_F(CoapMessageIdTest, AcknowledgmentIsReplayedForDuplicateRequest)
{
    Coap            coap{mEventBase.get(), mEndpoint};
    uint32_t        handled = 0;
    const ByteArray request{0x41, 0x01, 0x12, 0x34, 0xaa, 0xb4, 't', 'e', 's', 't'};

    coap.SetDefaultHandler([&](const Request &aRequest) {
        ++handled;
        Response response{Type::kAcknowledgment, Code::kContent};
        response.Append("cached");
        EXPECT_EQ(coap.SendResponse(aRequest, response), ErrorCode::kNone);
    });

    coap.Receive(request);
    ASSERT_EQ(handled, 1);
    ASSERT_EQ(coap.GetCachedResponsesNum(), 1);

    mEndpoint.mMessage.reset();
    coap.Receive(request);

    EXPECT_EQ(handled, 1);
    ASSERT_NE(mEndpoint.mMessage, nullptr);
    EXPECT_EQ(mEndpoint.mMessage->GetType(), Type::kAcknowledgment);
    EXPECT_EQ(mEndpoint.mMessage->GetMessageId(), 0x1234);
    EXPECT_EQ(mEndpoint.mMessage->GetToken(), ByteArray{0xaa});
    EXPECT_EQ(mEndpoint.mMessage->GetPayloadAsString(), "cached");
    EXPECT_EQ(coap.GetCachedResponsesNum(), 1);
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

    ~MockEndpoint() override = default;

    void SetPeer(MockEndpoint *aPeer) { mPeer = aPeer; }

    Error Send(const ByteArray &aBuf, MessageSubType aSubType) override
    {
        (void)aSubType;

        if (!mDropMessage)
        {
            mSendQueue.emplace(aBuf);
            event_active(&mPeer->mSendEvent, 0, 0);
        }
        return ERROR_NONE;
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
    MockEndpoint         *mPeer;
    bool                  mDropMessage;
    std::queue<ByteArray> mSendQueue;
};

TEST(CoapTest, CoapMessageConfirmable_BasicSendRecv)
{
    Address localhost;
    EXPECT_EQ(localhost.Set("127.0.0.1"), ErrorCode::kNone);

    auto eventBase = event_base_new();
    EXPECT_NE(eventBase, nullptr);

    MockEndpoint peer0{eventBase, localhost, 5683};
    MockEndpoint peer1{eventBase, localhost, 5684};
    peer0.SetPeer(&peer1);
    peer1.SetPeer(&peer0);

    Coap coap0{eventBase, peer0};
    Coap coap1{eventBase, peer1};

    EXPECT_EQ(coap1.AddResource({"/hello",
                                 [&coap1](const Request &aRequest) {
                                     EXPECT_TRUE(aRequest.IsRequest());
                                     EXPECT_EQ(aRequest.GetType(), Type::kConfirmable);
                                     EXPECT_EQ(aRequest.GetCode(), Code::kGet);

                                     ContentFormat contentFormat;
                                     EXPECT_EQ(aRequest.GetContentFormat(contentFormat), ErrorCode::kNone);
                                     EXPECT_EQ(contentFormat, ContentFormat::kTextPlain);

                                     std::string uriPath;
                                     EXPECT_EQ(aRequest.GetUriPath(uriPath), ErrorCode::kNone);
                                     EXPECT_EQ(uriPath, "/hello");

                                     EXPECT_EQ(aRequest.GetPayloadAsString(), "hello, CoAP");

                                     Response response{Type::kAcknowledgment, Code::kContent};
                                     EXPECT_EQ(response.SetContentFormat(ContentFormat::kTextPlain), ErrorCode::kNone);
                                     response.Append("Ack...");
                                     EXPECT_EQ(coap1.SendResponse(aRequest, response), ErrorCode::kNone);
                                 }}),
              ErrorCode::kNone);

    Message request{Type::kConfirmable, Code::kGet};
    EXPECT_EQ(request.SetUriPath("/hello"), ErrorCode::kNone);
    EXPECT_EQ(request.SetContentFormat(ContentFormat::kTextPlain), ErrorCode::kNone);
    request.Append("hello, CoAP");

    coap0.SendRequest(request, [&eventBase](const Response *aResponse, Error aError) {
        EXPECT_NE(aResponse, nullptr);
        EXPECT_EQ(aError, ErrorCode::kNone);
        EXPECT_EQ(aResponse->GetType(), Type::kAcknowledgment);

        ContentFormat contentFormat;
        EXPECT_EQ(aResponse->GetContentFormat(contentFormat), ErrorCode::kNone);
        EXPECT_EQ(contentFormat, ContentFormat::kTextPlain);

        EXPECT_EQ(aResponse->GetPayloadAsString(), "Ack...");

        event_base_loopbreak(eventBase);
    });

    EXPECT_EQ(event_base_loop(eventBase, EVLOOP_NO_EXIT_ON_EMPTY), 0);
    event_base_free(eventBase);
}

TEST(CoapTest, CoapMessageConfirmable_SeparateAck)
{
    Address localhost;
    EXPECT_EQ(localhost.Set("127.0.0.1"), ErrorCode::kNone);

    auto eventBase = event_base_new();
    EXPECT_NE(eventBase, nullptr);

    MockEndpoint peer0{eventBase, localhost, 5683};
    MockEndpoint peer1{eventBase, localhost, 5684};
    peer0.SetPeer(&peer1);
    peer1.SetPeer(&peer0);

    Coap coap0{eventBase, peer0};
    Coap coap1{eventBase, peer1};

    EXPECT_EQ(coap1.AddResource({"/hello",
                                 [&coap1](const Request &aRequest) {
                                     EXPECT_TRUE(aRequest.IsRequest());
                                     EXPECT_EQ(aRequest.GetType(), Type::kConfirmable);
                                     EXPECT_EQ(aRequest.GetCode(), Code::kGet);

                                     ContentFormat contentFormat;
                                     EXPECT_EQ(aRequest.GetContentFormat(contentFormat), ErrorCode::kNone);
                                     EXPECT_EQ(contentFormat, ContentFormat::kTextPlain);

                                     std::string uriPath;
                                     EXPECT_EQ(aRequest.GetUriPath(uriPath), ErrorCode::kNone);
                                     EXPECT_EQ(uriPath, "/hello");

                                     EXPECT_EQ(aRequest.GetPayloadAsString(), "hello, CoAP");

                                     EXPECT_EQ(coap1.SendAck(aRequest), ErrorCode::kNone);

                                     Response response{Type::kAcknowledgment, Code::kContent};
                                     EXPECT_EQ(response.SetContentFormat(ContentFormat::kTextPlain), ErrorCode::kNone);
                                     response.Append("Ack...");
                                     EXPECT_EQ(coap1.SendResponse(aRequest, response), ErrorCode::kNone);
                                 }}),
              ErrorCode::kNone);

    Message request{Type::kConfirmable, Code::kGet};
    EXPECT_EQ(request.SetUriPath("/hello"), ErrorCode::kNone);
    EXPECT_EQ(request.SetContentFormat(ContentFormat::kTextPlain), ErrorCode::kNone);
    request.Append("hello, CoAP");

    coap0.SendRequest(request, [&eventBase](const Response *aResponse, Error aError) {
        EXPECT_NE(aResponse, nullptr);
        EXPECT_EQ(aError, ErrorCode::kNone);
        EXPECT_EQ(aResponse->GetType(), Type::kAcknowledgment);

        ContentFormat contentFormat;
        EXPECT_EQ(aResponse->GetContentFormat(contentFormat), ErrorCode::kNone);
        EXPECT_EQ(contentFormat, ContentFormat::kTextPlain);

        EXPECT_EQ(aResponse->GetPayloadAsString(), "Ack...");

        event_base_loopbreak(eventBase);
    });

    EXPECT_EQ(event_base_loop(eventBase, EVLOOP_NO_EXIT_ON_EMPTY), 0);
    event_base_free(eventBase);
}

TEST(CoapTest, CoapMessageConfirmable_Retransmission)
{
    Address localhost;
    EXPECT_EQ(localhost.Set("127.0.0.1"), ErrorCode::kNone);

    auto eventBase = event_base_new();
    EXPECT_NE(eventBase, nullptr);

    MockEndpoint peer0{eventBase, localhost, 5683};
    MockEndpoint peer1{eventBase, localhost, 5684};
    peer0.SetPeer(&peer1);
    peer1.SetPeer(&peer0);

    Coap coap0{eventBase, peer0};
    Coap coap1{eventBase, peer1};

    EXPECT_EQ(coap1.AddResource({"/hello",
                                 [&coap1](const Request &aRequest) {
                                     EXPECT_TRUE(aRequest.IsRequest());
                                     EXPECT_EQ(aRequest.GetType(), Type::kConfirmable);
                                     EXPECT_EQ(aRequest.GetCode(), Code::kGet);

                                     ContentFormat contentFormat;
                                     EXPECT_EQ(aRequest.GetContentFormat(contentFormat), ErrorCode::kNone);
                                     EXPECT_EQ(contentFormat, ContentFormat::kTextPlain);

                                     std::string uriPath;
                                     EXPECT_EQ(aRequest.GetUriPath(uriPath), ErrorCode::kNone);
                                     EXPECT_EQ(uriPath, "/hello");

                                     EXPECT_EQ(aRequest.GetPayloadAsString(), "hello, CoAP");

                                     Response response{Type::kAcknowledgment, Code::kContent};
                                     EXPECT_EQ(response.SetContentFormat(ContentFormat::kTextPlain), ErrorCode::kNone);
                                     response.Append("Ack...");
                                     EXPECT_EQ(coap1.SendResponse(aRequest, response), ErrorCode::kNone);
                                 }}),
              ErrorCode::kNone);

    Message request{Type::kConfirmable, Code::kGet};
    EXPECT_EQ(request.SetUriPath("/hello"), ErrorCode::kNone);
    EXPECT_EQ(request.SetContentFormat(ContentFormat::kTextPlain), ErrorCode::kNone);
    request.Append("hello, CoAP");

    // Drop the message to trigger retransmission
    peer0.SetDropMessage(true);
    coap0.SendRequest(request, [&eventBase](const Response *aResponse, Error aError) {
        EXPECT_NE(aResponse, nullptr);
        EXPECT_EQ(aError, ErrorCode::kNone);
        EXPECT_EQ(aResponse->GetType(), Type::kAcknowledgment);

        ContentFormat contentFormat;
        EXPECT_EQ(aResponse->GetContentFormat(contentFormat), ErrorCode::kNone);
        EXPECT_EQ(contentFormat, ContentFormat::kTextPlain);

        EXPECT_EQ(aResponse->GetPayloadAsString(), "Ack...");

        event_base_loopbreak(eventBase);
    });
    peer0.SetDropMessage(false);

    EXPECT_EQ(event_base_loop(eventBase, EVLOOP_NO_EXIT_ON_EMPTY), 0);
    event_base_free(eventBase);
}

TEST(CoapTest, CoapMessageConfirmable_Timeout)
{
    Address localhost;
    EXPECT_EQ(localhost.Set("127.0.0.1"), ErrorCode::kNone);

    auto eventBase = event_base_new();
    EXPECT_NE(eventBase, nullptr);

    MockEndpoint peer0{eventBase, localhost, 5683};
    MockEndpoint peer1{eventBase, localhost, 5684};
    peer0.SetPeer(&peer1);
    peer1.SetPeer(&peer0);

    Coap coap0{eventBase, peer0};
    Coap coap1{eventBase, peer1};

    EXPECT_EQ(coap1.AddResource({"/hello",
                                 [&coap1](const Request &aRequest) {
                                     EXPECT_TRUE(aRequest.IsRequest());
                                     EXPECT_EQ(aRequest.GetType(), Type::kConfirmable);
                                     EXPECT_EQ(aRequest.GetCode(), Code::kGet);

                                     ContentFormat contentFormat;
                                     EXPECT_EQ(aRequest.GetContentFormat(contentFormat), ErrorCode::kNone);
                                     EXPECT_EQ(contentFormat, ContentFormat::kTextPlain);

                                     std::string uriPath;
                                     EXPECT_EQ(aRequest.GetUriPath(uriPath), ErrorCode::kNone);
                                     EXPECT_EQ(uriPath, "/hello");

                                     EXPECT_EQ(aRequest.GetPayloadAsString(), "hello, CoAP");

                                     Response response{Type::kAcknowledgment, Code::kContent};
                                     EXPECT_EQ(response.SetContentFormat(ContentFormat::kTextPlain), ErrorCode::kNone);
                                     response.Append("Ack...");
                                     EXPECT_EQ(coap1.SendResponse(aRequest, response), ErrorCode::kNone);
                                 }}),
              ErrorCode::kNone);

    Message request{Type::kConfirmable, Code::kGet};
    EXPECT_EQ(request.SetUriPath("/hello"), ErrorCode::kNone);
    EXPECT_EQ(request.SetContentFormat(ContentFormat::kTextPlain), ErrorCode::kNone);
    request.Append("hello, CoAP");

    // Drop the message to trigger timeout
    peer0.SetDropMessage(true);
    coap0.SendRequest(request, [&eventBase](const Response *aResponse, Error aError) {
        EXPECT_EQ(aResponse, nullptr);
        EXPECT_EQ(aError, ErrorCode::kTimeout);

        event_base_loopbreak(eventBase);
    });

    EXPECT_EQ(event_base_loop(eventBase, EVLOOP_NO_EXIT_ON_EMPTY), 0);
    event_base_free(eventBase);
}

class CaptureEndpoint : public Endpoint
{
public:
    Error Send(const ByteArray &aBuf, MessageSubType) override
    {
        mSent = aBuf;
        return ERROR_NONE;
    }

    Address GetPeerAddr() const override { return {}; }

    uint16_t GetPeerPort() const override { return 5683; }

    ByteArray mSent;
};

TEST(CoapTest, CoapResponse_RequestWithoutToken)
{
    std::unique_ptr<event_base, decltype(&event_base_free)> eventBase{event_base_new(), event_base_free};
    CaptureEndpoint                                         endpoint;
    Coap                                                    coap{eventBase.get(), endpoint};
    Error                                                   error;

    auto request = Message::Deserialize(error, ByteArray{0x40, 0x01, 0x12, 0x34});
    ASSERT_NE(request, nullptr);
    ASSERT_EQ(error, ErrorCode::kNone);
    ASSERT_EQ(request->GetToken(), ByteArray{});

    Response response{Type::kAcknowledgment, Code::kContent};
    EXPECT_EQ(coap.SendResponse(*request, response), ErrorCode::kNone);

    auto sent = Message::Deserialize(error, endpoint.mSent);
    ASSERT_NE(sent, nullptr);
    EXPECT_EQ(sent->GetMessageId(), 0x1234);
    EXPECT_EQ(sent->GetToken(), ByteArray{});
}

// TODO(wgtdkp): test with multiple outstanding requests / pressure tests.

// TODO(wgtdkp): add test cases to cover all CoAP APIs.

} // namespace coap

} // namespace commissioner

} // namespace ot
