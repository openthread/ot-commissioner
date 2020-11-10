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

/**
 * @file
 *   This file implements the CoAP.
 */

#include "library/coap.hpp"

#include <algorithm>

#include <ctype.h>
#include <memory.h>

#include "common/error_macros.hpp"
#include "common/utils.hpp"
#include "library/logging.hpp"
#include "library/openthread/random.hpp"

namespace ot {

namespace commissioner {

namespace coap {

Message::Message(Type aType, Code aCode)
    : Message()
{
    SetType(aType);
    SetCode(aCode);
}

Message::Message()
    : mSubType(MessageSubType::kNone)
{
    SetVersion(kVersion1);
}

size_t Message::GetHeaderLength() const
{
    ByteArray buf;
    Error     error = Serialize(mHeader, buf);

    ASSERT(error == ErrorCode::kNone);
    return buf.size();
}

const OptionValue *Message::GetOption(OptionType aNumber) const
{
    auto value = mOptions.find(aNumber);
    return value == mOptions.end() ? nullptr : &value->second;
}

bool Message::IsTokenEqual(const Message &aMessage) const
{
    return GetToken() == aMessage.GetToken();
}

Error Message::AppendOption(OptionType aNumber, const OptionValue &aValue)
{
    Error error;

    VerifyOrExit(IsValidOption(aNumber, aValue),
                 error = ERROR_INVALID_ARGS("invalid CoAP option (number={})", aNumber));

    if (aNumber == OptionType::kUriPath)
    {
        std::string uriPath = aValue.GetStringValue();
        SuccessOrExit(error = NormalizeUriPath(uriPath));
        mOptions[aNumber] = mOptions[aNumber].GetStringValue() + uriPath;
    }
    else
    {
        // We don't allow multiple options of the same type.
        mOptions.emplace(aNumber, aValue);
    }

exit:
    return error;
}

Error Message::GetOption(std::string &aValue, OptionType aNumber) const
{
    Error error;
    auto  option = GetOption(aNumber);

    VerifyOrExit(option != nullptr, error = ERROR_NOT_FOUND("CoAP option (number={}) not found", aNumber));

    aValue = option->GetStringValue();

exit:
    return error;
}

Error Message::GetOption(uint32_t &aValue, OptionType aNumber) const
{
    Error error;
    auto  option = GetOption(aNumber);

    VerifyOrExit(option != nullptr, error = ERROR_NOT_FOUND("CoAP option (number={}) not found", aNumber));

    aValue = option->GetUint32Value();

exit:
    return error;
}

Error Message::GetOption(ByteArray &aValue, OptionType aNumber) const
{
    Error error;
    auto  option = GetOption(aNumber);

    VerifyOrExit(option != nullptr, error = ERROR_NOT_FOUND("CoAP option (number={}) not found", aNumber));

    aValue = option->GetOpaqueValue();

exit:
    return error;
}

Error Message::SetUriPath(const std::string &aUriPath)
{
    Error       error;
    std::string NormalizedUriPath = aUriPath;

    SuccessOrExit(error = NormalizeUriPath(NormalizedUriPath));
    SuccessOrExit(error = AppendOption(OptionType::kUriPath, NormalizedUriPath));

exit:
    return error;
}

Error Message::GetContentFormat(ContentFormat &aContentFormat) const
{
    Error    error;
    uint32_t value;

    SuccessOrExit(error = GetOption(value, OptionType::kContentFormat));
    aContentFormat = utils::from_underlying<ContentFormat>(value);

exit:
    return error;
}

Error Message::GetAccept(ContentFormat &aAcceptFormat) const
{
    Error error;

    auto option = GetOption(OptionType::kAccept);
    VerifyOrExit(option != nullptr, error = ERROR_NOT_FOUND("cannot find valid CoAP Accept option"));
    aAcceptFormat = utils::from_underlying<ContentFormat>(option->GetUint32Value());

exit:
    return error;
}

Error Message::Serialize(const Header &aHeader, ByteArray &aBuf) const
{
    Error error;

    VerifyOrExit(aHeader.IsValid(), error = ERROR_INVALID_ARGS("serialize an invalid CoAP message header"));

    aBuf.push_back((aHeader.mVersion << 6) | (aHeader.mType << 4) | aHeader.mTokenLength);
    aBuf.push_back(aHeader.mCode);
    aBuf.push_back((aHeader.mMessageId & 0xff00) >> 8);
    aBuf.push_back((aHeader.mMessageId & 0x00ff));
    aBuf.insert(aBuf.end(), aHeader.mToken, aHeader.mToken + aHeader.mTokenLength);

exit:
    return error;
}

Error Message::Deserialize(Header &aHeader, const ByteArray &aBuf, size_t &aOffset)
{
    Error  error;
    Header header;
    size_t offset = aOffset;

    VerifyOrExit(offset < aBuf.size(), error = ERROR_BAD_FORMAT("premature end of CoAP message header"));
    header.mVersion     = aBuf[offset] >> 6;
    header.mType        = aBuf[offset] >> 4;
    header.mTokenLength = aBuf[offset++];

    VerifyOrExit(offset < aBuf.size(), error = ERROR_BAD_FORMAT("premature end of CoAP message header"));
    header.mCode = aBuf[offset++];

    VerifyOrExit(offset + 1 < aBuf.size(), error = ERROR_BAD_FORMAT("premature end of CoAP message header"));
    header.mMessageId = aBuf[offset++];
    header.mMessageId = (header.mMessageId << 8) | aBuf[offset++];

    VerifyOrExit(offset + header.mTokenLength <= aBuf.size(),
                 error = ERROR_BAD_FORMAT("premature end of CoAP message header"));
    memcpy(header.mToken, &aBuf[offset], std::min(header.mTokenLength, kMaxTokenLength));
    offset += header.mTokenLength;

    aHeader = header;
    aOffset = offset;

exit:
    return error;
}

bool Message::Header::IsValid() const
{
    return mVersion == kVersion1 && mTokenLength <= kMaxTokenLength;
}

Error Message::Serialize(OptionType         aOptionNumber,
                         const OptionValue &aOptionValue,
                         uint16_t           aLastOptionNumber,
                         ByteArray &        aBuf) const
{
    Error    error;
    uint16_t delta = utils::to_underlying(aOptionNumber) - aLastOptionNumber;
    uint16_t length;
    uint16_t valueLength = aOptionValue.GetLength();
    size_t   firstByte   = aBuf.size();
    size_t   extend      = aBuf.size() + 1;

    VerifyOrDie(utils::to_underlying(aOptionNumber) >= aLastOptionNumber);

    VerifyOrExit(IsValidOption(aOptionNumber, aOptionValue),
                 error = ERROR_INVALID_ARGS("option (number={}) is not valid", aOptionNumber));

    length = 1;
    length += delta < kOption1ByteExtension ? 0 : (delta < kOption2ByteExtension ? 1 : 2);
    length += valueLength < kOption1ByteExtension ? 0 : (valueLength < kOption2ByteExtension ? 1 : 2);

    aBuf.resize(aBuf.size() + length);

    if (delta < kOption1ByteExtensionOffset)
    {
        aBuf[firstByte] = (delta << kOptionDeltaOffset) & kOptionDeltaMask;
    }
    else if (delta < kOption2ByteExtensionOffset)
    {
        aBuf[firstByte] |= kOption1ByteExtension << kOptionDeltaOffset;
        aBuf[extend++] = (delta - kOption1ByteExtensionOffset) & 0xff;
    }
    else
    {
        aBuf[firstByte] |= kOption2ByteExtension << kOptionDeltaOffset;
        delta -= kOption2ByteExtensionOffset;
        aBuf[extend++] = delta >> 8;
        aBuf[extend++] = delta & 0xff;
    }

    // Insert option length.
    if (valueLength < kOption1ByteExtensionOffset)
    {
        aBuf[firstByte] |= static_cast<uint8_t>(valueLength);
    }
    else if (valueLength < kOption2ByteExtensionOffset)
    {
        aBuf[firstByte] |= kOption1ByteExtension;
        aBuf[extend++] = (valueLength - kOption1ByteExtensionOffset) & 0xff;
    }
    else
    {
        uint16_t deltaLength = valueLength - kOption2ByteExtensionOffset;
        aBuf[firstByte] |= kOption2ByteExtension;
        aBuf[extend++] = deltaLength >> 8;
        aBuf[extend++] = deltaLength & 0xff;
    }

    VerifyOrDie(length + firstByte == extend);

    aBuf.insert(aBuf.end(), aOptionValue.GetOpaqueValue().begin(), aOptionValue.GetOpaqueValue().end());

exit:
    return error;
}

Error Message::Deserialize(OptionType &     aOptionNumber,
                           OptionValue &    aOptionValue,
                           uint16_t         aLastOptionNumber,
                           const ByteArray &aBuf,
                           size_t &         aOffset)
{
    Error    error;
    uint16_t delta;
    uint16_t length;
    uint16_t valueLength;
    size_t   firstByte = aOffset;
    size_t   extend    = aOffset + 1;

    VerifyOrExit(firstByte < aBuf.size(), error = ERROR_BAD_FORMAT("premature end of a CoAP option"));

    delta       = aBuf[firstByte] >> 4;
    valueLength = aBuf[firstByte] & 0x0f;

    length = 1;
    length += delta < kOption1ByteExtension ? 0 : (delta < kOption2ByteExtension ? 1 : 2);
    length += valueLength < kOption1ByteExtension ? 0 : (valueLength < kOption2ByteExtension ? 1 : 2);

    VerifyOrExit(firstByte + length <= aBuf.size(), error = ERROR_BAD_FORMAT("premature end of a CoAP option"));

    if (delta < kOption1ByteExtension)
    {
        // do nothing
    }
    else if (delta == kOption1ByteExtension)
    {
        delta = kOption1ByteExtensionOffset + aBuf[extend++];
    }
    else if (delta == kOption2ByteExtension)
    {
        delta = kOption2ByteExtensionOffset + ((static_cast<uint16_t>(aBuf[extend]) << 8) | aBuf[extend + 1]);
        extend += 2;
    }
    else
    {
        // we have delta == 0x0f

        VerifyOrExit(valueLength == 0x0f,
                     error = ERROR_BAD_FORMAT("invalid CoAP option (firstByte={:X})", aBuf[firstByte]));
        ExitNow(error = ERROR_NOT_FOUND("cannot find more CoAP option"));
    }

    if (valueLength < kOption1ByteExtension)
    {
        // do nothing
    }
    else if (valueLength == kOption1ByteExtension)
    {
        valueLength = kOption1ByteExtensionOffset + aBuf[extend++];
    }
    else if (valueLength == kOption2ByteExtension)
    {
        valueLength = kOption2ByteExtensionOffset + ((static_cast<uint16_t>(aBuf[extend]) << 8) | aBuf[extend + 1]);
        extend += 2;
    }
    else
    {
        ExitNow(error = ERROR_BAD_FORMAT("invalid CoAP option (firstByte={:X})", aBuf[firstByte]));
    }

    VerifyOrDie(firstByte + length == extend);

    VerifyOrExit(valueLength + extend <= aBuf.size(), error = ERROR_BAD_FORMAT("premature end of a CoAP option"));

    aOptionNumber = utils::from_underlying<OptionType>(aLastOptionNumber + delta);
    aOptionValue  = ByteArray{aBuf.begin() + extend, aBuf.begin() + extend + valueLength};

    aOffset += length + valueLength;

exit:
    return error;
}

Coap::RequestHolder::RequestHolder(const RequestPtr aRequest, ResponseHandler aHandler)
    : mRequest(aRequest)
    , mHandler(aHandler)
    , mRetransmissionCount(0)
    , mAcknowledged(false)
{
    uint32_t lowBound    = 1000 * kAckTimeout;
    uint32_t upperBound  = 1000 * kAckTimeout * kAckRandomFactorNumerator / kAckRandomFactorDenominator;
    uint32_t delay       = random::non_crypto::GetUint32InRange(lowBound, upperBound);
    mRetransmissionDelay = std::chrono::milliseconds(delay);
    mNextTimerShot       = Clock::now() + mRetransmissionDelay;
}

Coap::Coap(struct event_base *aEventBase, Endpoint &aEndpoint)
    : mMessageId(0)
    , mRequestsCache(aEventBase, [this](Timer &aTimer) { Retransmit(aTimer); })
    , mResponsesCache(aEventBase, std::chrono::seconds(kExchangeLifetime))
    , mDefaultHandler(nullptr)
    , mEndpoint(aEndpoint)
{
    using namespace std::placeholders;
    mEndpoint.SetReceiver([this](Endpoint &aEndpoint, const ByteArray &aBuf) { Receive(aEndpoint, aBuf); });
}

void Coap::ClearRequestsAndResponses(void)
{
    CancelRequests();
    mResponsesCache.Clear();
}

void Coap::CancelRequests()
{
    while (!mRequestsCache.IsEmpty())
    {
        std::string uri = "UNKNOWN_URI";
        mRequestsCache.Front().mRequest->GetUriPath(uri).IgnoreError();
        FinalizeTransaction(mRequestsCache.Front(), nullptr, ERROR_CANCELLED("request to {} was cancelled", uri));
    }
}

Error Coap::AddResource(const Resource &aResource)
{
    Error error;
    auto  res = mResources.find(aResource.GetUriPath());

    if (res != mResources.end())
    {
        ExitNow(error = ERROR_ALREADY_EXISTS("CoAP resource {} already exists", aResource.GetUriPath()));
    }
    mResources.emplace(aResource.GetUriPath(), aResource);

exit:
    return error;
}

void Coap::RemoveResource(const Resource &aResource)
{
    mResources.erase(aResource.GetUriPath());
}

void Coap::SetDefaultHandler(RequestHandler aHandler)
{
    mDefaultHandler = aHandler;
}

void Coap::SendRequest(const Request &aRequest, ResponseHandler aHandler)
{
    Error error;
    auto  request = std::make_shared<Request>(aRequest);

    VerifyOrExit(request->IsConfirmable() || request->IsNonConfirmable(),
                 error = ERROR_INVALID_ARGS("a CoAP request is neither Confirmable nor NON-Confirmable"));

    VerifyOrDie(request->GetMessageId() == 0);
    request->SetMessageId(AllocMessageId());
    request->SetToken(kDefaultTokenLength);

    SuccessOrExit(error = Send(*request));

    if (request->IsConfirmable())
    {
        mRequestsCache.Put(request, aHandler);
    }

exit:
    if (error != ErrorCode::kNone && aHandler != nullptr)
    {
        aHandler(nullptr, error);
    }
}

Error Coap::SendResponse(const Request &aRequest, Response &aResponse)
{
    // Set message id to request's id
    if (aResponse.GetMessageId() == 0)
    {
        aResponse.SetMessageId(aRequest.GetMessageId());
    }

    // Set the token to request's token
    aResponse.SetToken(aRequest.GetToken());

    // Set message info
    aResponse.SetEndpoint(aRequest.GetEndpoint());

    // Enqueue response
    mResponsesCache.Put(aResponse);

    return Send(aResponse);
}

Error Coap::SendEmptyChanged(const Request &aRequest)
{
    if (!aRequest.IsConfirmable())
    {
        return ERROR_INVALID_ARGS("the CoAP request is not Confirmable");
    }
    return SendHeaderResponse(Code::kChanged, aRequest);
}

void Coap::Receive(Endpoint &aEndpoint, const ByteArray &aBuf)
{
    Error error;

    auto message = Message::Deserialize(error, aBuf);
    ReceiveMessage(aEndpoint, message, error);
}

void Coap::ReceiveMessage(Endpoint &aEndpoint, std::shared_ptr<Message> aMessage, Error error)
{
    if (error != ErrorCode::kNone)
    {
        // Silently drop a bad formatted message
        LOG_INFO(LOG_REGION_COAP, "drop a CoAP message in bad format: {}", error.GetMessage());
    }
    else
    {
        ASSERT(aMessage != nullptr);

        aMessage->SetEndpoint(&aEndpoint);
        if (aMessage->IsRequest())
        {
            HandleRequest(*aMessage);
        }
        else
        {
            HandleResponse(*aMessage);
        }
    }
}

void Coap::HandleRequest(const Request &aRequest)
{
    Error                                error;
    const Response *                     response = nullptr;
    std::string                          uriPath;
    decltype(mResources)::const_iterator resource;

    SuccessOrExit(error = aRequest.GetUriPath(uriPath));

    response = mResponsesCache.Match(aRequest);
    if (response != nullptr)
    {
        LOG_INFO(LOG_REGION_COAP, "server(={}) found cached CoAP response for resource {}", static_cast<void *>(this),
                 uriPath);
        ExitNow(error = Send(*response));
    }

    resource = mResources.find(uriPath);
    if (resource != mResources.end())
    {
        resource->second.HandleRequest(aRequest);
        ExitNow(error = ERROR_NONE);
    }
    else if (mDefaultHandler != nullptr)
    {
        mDefaultHandler(aRequest);
        ExitNow(error = ERROR_NONE);
    }
    else
    {
        IgnoreError(SendNotFound(aRequest));
        ExitNow(error = ERROR_NONE);
    }

exit:
    if (error != ErrorCode::kNone)
    {
        LOG_INFO(LOG_REGION_COAP, "server(={}) handle request failed: {}", static_cast<void *>(this), error.ToString());
    }
    return;
}

void Coap::HandleResponse(const Response &aResponse)
{
    const RequestHolder *requestHolder = nullptr;
    std::string          requestUri    = "UNKNOWN_URI";

    requestHolder = mRequestsCache.Match(aResponse);
    if (requestHolder == nullptr)
    {
        if (aResponse.IsConfirmable() || aResponse.IsNonConfirmable())
        {
            IgnoreError(SendReset(aResponse));
        }
        ExitNow();
    }

    requestHolder->mRequest->GetUriPath(requestUri).IgnoreError();
    switch (aResponse.GetType())
    {
    case Type::kReset:
        if (aResponse.IsEmpty())
        {
            FinalizeTransaction(*requestHolder, nullptr, ERROR_ABORTED("request to {} was reset by peer", requestUri));
        }

        // Silently ignore non-empty reset messages (RFC 7252, p. 4.2).
        break;

    case Type::kAcknowledgment:
        if (aResponse.IsEmpty())
        {
            // Empty acknowledgment.
            if (requestHolder->mRequest->IsConfirmable())
            {
                requestHolder->mAcknowledged = true;
            }

            // Remove the message if response is not expected, otherwise await response.
            if (requestHolder->mHandler == nullptr)
            {
                mRequestsCache.Eliminate(*requestHolder);
            }
        }
        else if (aResponse.IsResponse() && aResponse.IsTokenEqual(*requestHolder->mRequest))
        {
            // Piggybacked response.
            FinalizeTransaction(*requestHolder, &aResponse, ERROR_NONE);
        }

        // Silently ignore acknowledgments carrying requests (RFC 7252, p. 4.2)
        // or with no token match (RFC 7252, p. 5.3.2).
        break;

    case Type::kConfirmable:
        // Send empty ACK if it is a CON message.
        IgnoreError(SendAck(aResponse));

        // fall through

    case Type::kNonConfirmable:
        // Separate response.
        FinalizeTransaction(*requestHolder, &aResponse, ERROR_NONE);
        break;
    }

exit:
    return;
}

void Coap::Retransmit(Timer &)
{
    auto now = Clock::now();

    LOG_DEBUG(LOG_REGION_COAP, "client(={}) retransmit timer triggered", static_cast<void *>(this));

    while (!mRequestsCache.IsEmpty() && mRequestsCache.Earliest() < now)
    {
        auto requestHolder = mRequestsCache.Eliminate();

        std::string uri = "UNKOWN_URI";
        requestHolder.mRequest->GetUriPath(uri).IgnoreError();

        if ((requestHolder.mRequest->IsConfirmable()) && (requestHolder.mRetransmissionCount < kMaxRetransmit))
        {
            // Increment retransmission counter and timer.
            ++requestHolder.mRetransmissionCount;
            requestHolder.mRetransmissionDelay *= 2;
            requestHolder.mNextTimerShot = now + requestHolder.mRetransmissionDelay;

            mRequestsCache.Put(requestHolder);

            std::string uri;
            IgnoreError(requestHolder.mRequest->GetUriPath(uri));

            // Retransmit
            if (!requestHolder.mAcknowledged)
            {
                LOG_INFO(LOG_REGION_COAP, "client(={}) retransmit request {}, retransmit count = {}",
                         static_cast<void *>(this), uri, requestHolder.mRetransmissionCount);

                auto error = Send(*requestHolder.mRequest);
                if (error != ErrorCode::kNone)
                {
                    LOG_WARN(LOG_REGION_COAP, "client(={}) retransmit request {} failed: {}", static_cast<void *>(this),
                             uri, error.ToString());
                    FinalizeTransaction(requestHolder, nullptr, error);
                }
            }
            else
            {
                LOG_DEBUG(LOG_REGION_COAP, "client(={}) request to {} has been acknowledged, won't retransmit",
                          static_cast<void *>(this), uri);
            }
        }
        else
        {
            // No expected response or acknowledgment.
            FinalizeTransaction(requestHolder, nullptr, ERROR_TIMEOUT("request to {} timeout", uri));
        }
    }

    mRequestsCache.UpdateTimer();
}

Error Coap::SendHeaderResponse(Code aCode, const Request &aRequest)
{
    Error    error;
    Response response;

    switch (aRequest.GetType())
    {
    case Type::kConfirmable:
        response.SetType(Type::kAcknowledgment);
        response.SetCode(aCode);
        response.SetMessageId(aRequest.GetMessageId());
        break;

    case Type::kNonConfirmable:
        response.SetType(Type::kNonConfirmable);
        response.SetCode(aCode);
        response.SetMessageId(AllocMessageId());
        break;

    default:
        ExitNow(error = ERROR_INVALID_ARGS("a CoAP request is neither Confirmable nor NON-Confirmable"));
        break;
    }

    SuccessOrExit(error = SendResponse(aRequest, response));

exit:
    return error;
}

Error Coap::SendEmptyMessage(Type aType, const Request &aRequest)
{
    Error    error;
    Response response{aType, Code::kEmpty};

    VerifyOrExit(aRequest.IsConfirmable(), error = ERROR_INVALID_ARGS("CoAP request is not Confirmable"));

    response.SetMessageId(aRequest.GetMessageId());
    response.SetEndpoint(aRequest.GetEndpoint());

    SuccessOrExit(error = Send(response));

exit:
    return error;
}

Error Coap::Send(const Message &aMessage)
{
    Error     error;
    ByteArray data;
    SuccessOrExit(error = aMessage.Serialize(data));

    if (aMessage.GetEndpoint() == nullptr)
    {
        aMessage.SetEndpoint(&mEndpoint);
    }

    error = aMessage.GetEndpoint()->Send(data, aMessage.GetSubType());
    SuccessOrExit(error);

exit:
    return error;
}

void Coap::FinalizeTransaction(const RequestHolder &aRequestHolder, const Response *aResponse, Error aResult)
{
    if (aRequestHolder.mHandler != nullptr)
    {
        auto handler = aRequestHolder.mHandler;

        // The user-provided handler may do anything that causing this
        // handler be recursively called (For example, user stops the CoAP
        // instance which will finalize all transactions). Set this handler
        // to null to stop a handler being recalled.
        aRequestHolder.mHandler = nullptr;
        handler(aResponse, aResult);
    }
    mRequestsCache.Eliminate(aRequestHolder);
}

const Response *Coap::ResponsesCache::Match(const Request &aRequest) const
{
    for (const auto &kv : mContainer)
    {
        const auto &response = kv.second;
        if (response.GetEndpoint() != aRequest.GetEndpoint())
        {
            continue;
        }
        if (response.GetMessageId() != aRequest.GetMessageId())
        {
            continue;
        }
        return &response;
    }
    return nullptr;
}

void Coap::ResponsesCache::Clear()
{
    mTimer.Stop();
    mContainer.clear();
}

void Coap::ResponsesCache::Eliminate()
{
    auto now = Clock::now();

    while (!mContainer.empty())
    {
        auto  earliest = mContainer.begin();
        auto  deadTime = earliest->first;
        auto &response = earliest->second;
        if (deadTime > now)
        {
            break;
        }
        mContainer.erase(earliest);
        LOG_INFO(LOG_REGION_COAP, "server(={}) remove response cache: token={}, messageId={}",
                 static_cast<void *>(this), utils::Hex(response.GetToken()), response.GetMessageId());
    }
}

void Coap::RequestsCache::Put(const RequestPtr aRequest, ResponseHandler aHandler)
{
    Put({aRequest, aHandler});
}

void Coap::RequestsCache::Put(const RequestHolder &aRequestHolder)
{
    mContainer.emplace(aRequestHolder);
    UpdateTimer();
}

Coap::RequestHolder Coap::RequestsCache::Eliminate()
{
    VerifyOrDie(!IsEmpty());
    auto ret = *mContainer.begin();

    mContainer.erase(mContainer.begin());
    UpdateTimer();

    return ret;
}

void Coap::RequestsCache::Eliminate(const RequestHolder &aRequestHolder)
{
    for (auto holder = mContainer.begin(); holder != mContainer.end(); ++holder)
    {
        if (holder->mRequest == aRequestHolder.mRequest)
        {
            mContainer.erase(holder);
            break;
        }
    }

    UpdateTimer();
}

void Coap::RequestsCache::UpdateTimer()
{
    if (IsEmpty())
    {
        mRetransmissionTimer.Stop();
    }
    else if (!mRetransmissionTimer.IsRunning() || Earliest() < mRetransmissionTimer.GetFireTime())
    {
        mRetransmissionTimer.Start(Earliest());
    }
}

const Coap::RequestHolder *Coap::RequestsCache::Match(const Response &aResponse) const
{
    for (const auto &requestHolder : mContainer)
    {
        const auto request = requestHolder.mRequest;

        if (aResponse.GetEndpoint() == request->GetEndpoint())
        {
            switch (aResponse.GetType())
            {
            case Type::kReset:
            case Type::kAcknowledgment:
                if (aResponse.GetMessageId() == request->GetMessageId())
                {
                    return &requestHolder;
                }
                break;
            case Type::kConfirmable:
            case Type::kNonConfirmable:
                if (aResponse.IsTokenEqual(*request))
                {
                    return &requestHolder;
                }
                break;
            }
        }
    }
    return nullptr;
}

Error Message::Serialize(ByteArray &aBuf) const
{
    Error    error;
    uint16_t lastOptionNumber;

    SuccessOrExit(error = Serialize(mHeader, aBuf));

    lastOptionNumber = 0;
    for (const auto &option : mOptions)
    {
        const auto &number = option.first;
        const auto &value  = option.second;
        if (number == OptionType::kUriPath)
        {
            std::list<std::string> uriPathSegments;
            SuccessOrExit(error = SplitUriPath(uriPathSegments, value.GetStringValue()));
            for (const auto &segment : uriPathSegments)
            {
                SuccessOrExit(error = Serialize(number, segment, lastOptionNumber, aBuf));
                lastOptionNumber = utils::to_underlying(number);
            }
        }
        else
        {
            SuccessOrExit(error = Serialize(number, value, lastOptionNumber, aBuf));
            lastOptionNumber = utils::to_underlying(number);
        }
    }

    if (mPayload.size() > 0)
    {
        aBuf.push_back(kPayloadMarker);
        aBuf.insert(aBuf.end(), mPayload.begin(), mPayload.end());
    }

exit:
    return error;
}

Error Message::SplitUriPath(std::list<std::string> &aUriPathList, const std::string &aUriPath)
{
    auto uri = aUriPath;

    size_t begin = 0;
    while (true)
    {
        auto pos = uri.find_first_of('/', begin);
        if (pos == std::string::npos)
        {
            break;
        }
        if (pos > begin)
        {
            aUriPathList.emplace_back(uri.substr(begin, pos - begin));
        }
        begin = pos + 1;
    }

    if (begin < uri.size())
    {
        aUriPathList.emplace_back(uri.substr(begin));
    }

    if (aUriPathList.empty())
    {
        // For example, uri path is "/", "//"
        aUriPathList.emplace_back("/");
    }

    return ERROR_NONE;
}

// Normalize the uri path by:
//   stripping spaces at the two ends;
//   adding '/' to the beginning if there is none;
//   applying percent-decode;
Error Message::NormalizeUriPath(std::string &aUriPath)
{
    auto   xdigit = [](char c) { return 'A' <= c && c <= 'F' ? c - 'A' : 'a' <= c && c <= 'f' ? c - 'a' : c - '0'; };
    size_t begin = 0, end = aUriPath.size();
    Error  error;

    while (begin < aUriPath.size() && isspace(aUriPath[begin]))
    {
        ++begin;
    }
    while (end >= 1 && isspace(aUriPath[end - 1]))
    {
        --end;
    }
    aUriPath = aUriPath.substr(begin, end - begin);

    if (aUriPath.empty() || aUriPath[0] != '/')
    {
        aUriPath = "/" + aUriPath;
    }

    while (true)
    {
        auto pos = aUriPath.find_first_of('%', begin);
        if (pos == std::string::npos)
        {
            break;
        }
        if (pos + 2 >= aUriPath.size())
        {
            ExitNow(error = ERROR_INVALID_ARGS("{} is not a valid CoAP URI path", aUriPath));
        }
        if (!isxdigit(aUriPath[pos + 1]) || !isxdigit(aUriPath[pos + 2]))
        {
            ExitNow(error = ERROR_INVALID_ARGS("{} is not a valid CoAP URI path", aUriPath));
        }

        char c = (xdigit(aUriPath[pos + 1]) << 4) | xdigit(aUriPath[pos + 2]);
        aUriPath.replace(pos, 3, 1, c);
        begin = pos + 1;
    }

exit:
    return error;
}

std::shared_ptr<Message> Message::Deserialize(Error &aError, const ByteArray &aBuf)
{
    Error    error;
    size_t   offset = 0;
    uint16_t lastOptionNumber;
    auto     message = std::make_shared<Message>();

    SuccessOrExit(error = Deserialize(message->mHeader, aBuf, offset));
    VerifyOrExit(message->mHeader.IsValid(), error = ERROR_BAD_FORMAT("invalid CoAP message header"));

    lastOptionNumber = 0;
    while (offset < aBuf.size() && aBuf[offset] != kPayloadMarker)
    {
        OptionType  number;
        OptionValue value;

        SuccessOrExit(error = Deserialize(number, value, lastOptionNumber, aBuf, offset));

        if (IsValidOption(number, value))
        {
            // AppendOption will do further validation before adding the option.
            error = message->AppendOption(number, value);
        }
        else
        {
            error = ERROR_BAD_FORMAT("bad CoAP option (number={}", number);
        }

        if (error != ErrorCode::kNone)
        {
            if (IsCriticalOption(number))
            {
                // Stop if any unrecognized option is critical.
                ExitNow();
            }
            else
            {
                // Ignore non-critical option error.
                error = ERROR_NONE;
            }
        }

        lastOptionNumber = utils::to_underlying(number);
    }

    if (offset < aBuf.size())
    {
        if (aBuf[offset++] == kPayloadMarker)
        {
            VerifyOrExit(offset < aBuf.size(), error = ERROR_BAD_FORMAT("payload marker followed by empty payload"));
            message->mPayload.assign(aBuf.begin() + offset, aBuf.end());
        }
        else
        {
            // Ignore extra data.
            message->mPayload.clear();
        }
    }

exit:
    if (error != ErrorCode::kNone)
    {
        message = nullptr;
    }
    aError = error;
    return message;
}

bool Message::IsValidOption(OptionType aNumber, const OptionValue &aValue)
{
    switch (aNumber)
    {
    case OptionType::kIfMatch:
        return aValue.GetLength() <= 8;
    case OptionType::kUriHost:
        return aValue.GetLength() >= 1 && aValue.GetLength() <= 255;
    case OptionType::kETag:
        return aValue.GetLength() >= 1 && aValue.GetLength() <= 8;
    case OptionType::kIfNonMatch:
        return aValue.GetLength() == 0;
    case OptionType::kUriPort:
        return aValue.GetLength() <= 2;
    case OptionType::kLocationPath:
        return aValue.GetLength() <= 255;
    case OptionType::kUriPath:
        return aValue.GetLength() <= 255;
    case OptionType::kContentFormat:
        return aValue.GetLength() <= 2;
    case OptionType::kMaxAge:
        return aValue.GetLength() <= 4;
    case OptionType::kUriQuery:
        return aValue.GetLength() <= 255;
    case OptionType::kAccept:
        return aValue.GetLength() <= 2;
    case OptionType::kLocationQuery:
        return aValue.GetLength() <= 255;
    case OptionType::kProxyUri:
        return aValue.GetLength() >= 1 && aValue.GetLength() <= 1034;
    case OptionType::kProxyScheme:
        return aValue.GetLength() >= 1 && aValue.GetLength() <= 255;
    case OptionType::kSize1:
        return aValue.GetLength() <= 4;
    default:
        return false;
    }
}

bool Message::IsCriticalOption(OptionType aNumber)
{
    switch (aNumber)
    {
    case OptionType::kIfMatch:
    case OptionType::kUriHost:
    case OptionType::kIfNonMatch:
    case OptionType::kUriPort:
    case OptionType::kUriPath:
    case OptionType::kUriQuery:
    case OptionType::kAccept:
    case OptionType::kProxyUri:
    case OptionType::kProxyScheme:
        return true;
    default:
        return false;
    }
}

void Message::SetToken(const ByteArray &aToken)
{
    auto len             = std::min(sizeof(mHeader.mToken), aToken.size());
    mHeader.mTokenLength = len;
    memcpy(mHeader.mToken, &aToken[0], len);
}

void Message::SetToken(uint8_t aTokenLength)
{
    VerifyOrDie(aTokenLength <= sizeof(mHeader.mToken));

    mHeader.mTokenLength = aTokenLength;
    random::non_crypto::FillBuffer(mHeader.mToken, aTokenLength);
}

} // namespace coap

} // namespace commissioner

} // namespace ot
