/*
 *  Copyright (c) 2019, The OpenThread Authors.
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

#include "coap.hpp"

#include <ctype.h>
#include <memory.h>

#include <algorithm>

#include "logging.hpp"
#include "openthread/random.hpp"

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
{
    SetVersion(kVersion1);
}

size_t Message::GetHeaderLength() const
{
    ByteArray buf;

    ASSERT(Serialize(mHeader, buf) == Error::kNone);
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
    Error error = Error::kNone;

    VerifyOrExit(IsValidOption(aNumber, aValue), error = Error::kInvalidArgs);

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
    auto option = GetOption(aNumber);
    if (option == nullptr)
    {
        return Error::kNotFound;
    }
    aValue = option->GetStringValue();
    return Error::kNone;
}

Error Message::GetOption(uint32_t &aValue, OptionType aNumber) const
{
    auto option = GetOption(aNumber);
    if (option == nullptr)
    {
        return Error::kNotFound;
    }
    aValue = option->GetUint32Value();
    return Error::kNone;
}

Error Message::GetOption(ByteArray &aValue, OptionType aNumber) const
{
    auto option = GetOption(aNumber);
    if (option == nullptr)
    {
        return Error::kNotFound;
    }
    aValue = option->GetOpaqueValue();
    return Error::kNone;
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
    auto option = GetOption(OptionType::kAccept);
    if (option == nullptr)
    {
        return Error::kNotFound;
    }
    aAcceptFormat = utils::from_underlying<ContentFormat>(option->GetUint32Value());
    return Error::kNone;
}

Error Message::Serialize(const Header &aHeader, ByteArray &aBuf) const
{
    Error error = Error::kNone;

    VerifyOrExit(aHeader.IsValid(), error = Error::kBadFormat);

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
    Error error = Error::kNone;

    VerifyOrExit(aOffset < aBuf.size(), error = Error::kBadFormat);
    aHeader.mVersion     = aBuf[aOffset] >> 6;
    aHeader.mType        = aBuf[aOffset] >> 4;
    aHeader.mTokenLength = aBuf[aOffset++];

    VerifyOrExit(aOffset < aBuf.size(), error = Error::kBadFormat);
    aHeader.mCode = aBuf[aOffset++];

    VerifyOrExit(aOffset + 1 < aBuf.size(), error = Error::kBadFormat);
    aHeader.mMessageId = aBuf[aOffset++];
    aHeader.mMessageId = (aHeader.mMessageId << 8) | aBuf[aOffset++];

    VerifyOrExit(aOffset + aHeader.mTokenLength <= aBuf.size(), error = Error::kBadFormat);
    memcpy(aHeader.mToken, &aBuf[aOffset], std::min(aHeader.mTokenLength, kMaxTokenLength));
    aOffset += aHeader.mTokenLength;

exit:
    return error;
}

bool Message::Header::IsValid() const
{
    return mVersion == kVersion1 && mTokenLength >= 0 && mTokenLength <= kMaxTokenLength;
}

Error Message::Serialize(OptionType         aOptionNumber,
                         const OptionValue &aOptionValue,
                         uint16_t           aLastOptionNumber,
                         ByteArray &        aBuf) const
{
    Error    error = Error::kNone;
    uint16_t delta = utils::to_underlying(aOptionNumber) - aLastOptionNumber;
    uint16_t length;
    uint16_t valueLength = aOptionValue.GetLength();
    size_t   firstByte   = aBuf.size();
    size_t   extend      = aBuf.size() + 1;

    ASSERT(utils::to_underlying(aOptionNumber) >= aLastOptionNumber);

    VerifyOrExit(IsValidOption(aOptionNumber, aOptionValue), error = Error::kBadFormat);

    length = 1;
    length += delta < kOption1ByteExtensionOffset ? 0 : (delta < kOption2ByteExtensionOffset ? 1 : 2);
    length += valueLength < kOption1ByteExtensionOffset ? 0 : (valueLength < kOption2ByteExtensionOffset ? 1 : 2);

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

    ASSERT(length + firstByte == extend);

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
    Error    error = Error::kNone;
    uint16_t delta;
    uint16_t length;
    uint16_t valueLength;
    size_t   firstByte = aOffset;
    size_t   extend    = aOffset + 1;

    VerifyOrExit(firstByte < aBuf.size(), error = Error::kBadFormat);

    delta       = aBuf[firstByte] >> 4;
    valueLength = aBuf[firstByte] & 0x0f;

    length = 1;
    length += delta < kOption1ByteExtensionOffset ? 0 : (delta < kOption2ByteExtensionOffset ? 1 : 2);
    length += valueLength < kOption1ByteExtensionOffset ? 0 : (valueLength < kOption2ByteExtensionOffset ? 1 : 2);

    VerifyOrExit(firstByte + length <= aBuf.size(), error = Error::kBadFormat);

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
        VerifyOrExit(valueLength == 0x0f, error = Error::kBadFormat);
        ExitNow(error = Error::kNotFound);
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
        ExitNow(error = Error::kBadFormat);
    }

    ASSERT(firstByte + length == extend);

    VerifyOrExit(valueLength + extend <= aBuf.size(), error = Error::kBadFormat);

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
    AbortRequests();
    mResponsesCache.Clear();
}

void Coap::AbortRequests()
{
    while (!mRequestsCache.IsEmpty())
    {
        FinalizeTransaction(mRequestsCache.Front(), nullptr, Error::kAbort);
    }
}

Error Coap::AddResource(const Resource &aResource)
{
    Error error = Error::kNone;
    auto  res   = mResources.find(aResource.GetUriPath());
    VerifyOrExit(res == mResources.end(), error = Error::kAlready);
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
    Error error   = Error::kNone;
    auto  request = std::make_shared<Request>(aRequest);

    VerifyOrExit(request->IsConfirmable() || request->IsNonConfirmable(), error = Error::kInvalidArgs);

    ASSERT(request->GetMessageId() == 0);
    request->SetMessageId(AllocMessageId());
    request->SetToken(kDefaultTokenLength);

    SuccessOrExit(error = Send(*request));

    if (request->IsConfirmable())
    {
        mRequestsCache.Put(request, aHandler);
    }

exit:
    if (error != Error::kNone && aHandler != nullptr)
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

void Coap::Receive(Endpoint &aEndpoint, const ByteArray &aBuf)
{
    Error error;

    auto message = Message::Deserialize(error, aBuf);
    ReceiveMessage(aEndpoint, message, error);
}

void Coap::ReceiveMessage(Endpoint &aEndpoint, std::shared_ptr<Message> aMessage, Error error)
{
    if (error != Error::kNone)
    {
        // Silently drop a bad formatted message
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
    Error                                error    = Error::kNone;
    const Response *                     response = nullptr;
    std::string                          uriPath;
    decltype(mResources)::const_iterator resource;

    response = mResponsesCache.Match(aRequest);
    if (response != nullptr)
    {
        ExitNow(error = Send(*response));
    }

    SuccessOrExit(error = aRequest.GetUriPath(uriPath));

    resource = mResources.find(uriPath);
    if (resource != mResources.end())
    {
        resource->second.HandleRequest(aRequest);
        ExitNow(error = Error::kNone);
    }
    else if (mDefaultHandler != nullptr)
    {
        mDefaultHandler(aRequest);
        ExitNow(error = Error::kNone);
    }
    else
    {
        IgnoreError(SendNotFound(aRequest));
        ExitNow(error = Error::kNotFound);
    }

exit:
    if (error != Error::kNone)
    {
        LOG_INFO("CoAP: handle request failed: %s", ErrorToString(error));
    }
    return;
}

void Coap::HandleResponse(const Response &aResponse)
{
    const RequestHolder *requestHolder = nullptr;

    requestHolder = mRequestsCache.Match(aResponse);
    if (requestHolder == nullptr)
    {
        if (aResponse.IsConfirmable() || aResponse.IsNonConfirmable())
        {
            IgnoreError(SendReset(aResponse));
        }
        ExitNow();
    }

    switch (aResponse.GetType())
    {
    case Type::kReset:
        if (aResponse.IsEmpty())
        {
            FinalizeTransaction(*requestHolder, nullptr, Error::kAbort);
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
            FinalizeTransaction(*requestHolder, &aResponse, Error::kNone);
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
        FinalizeTransaction(*requestHolder, &aResponse, Error::kNone);
        break;
    }

exit:
    return;
}

void Coap::Retransmit(Timer &)
{
    auto now = Clock::now();

    LOG_DEBUG("CoAP retransmit timer triggered");

    while (!mRequestsCache.IsEmpty() && mRequestsCache.Earliest() < now)
    {
        auto requestHolder = mRequestsCache.Eliminate();

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
                LOG_INFO("retransmit of request {}, retransmit count = {}", uri, requestHolder.mRetransmissionCount);

                auto error = Send(*requestHolder.mRequest);
                if (error != Error::kNone)
                {
                    LOG_WARN("retransmit of request {} failed: {}", uri, ErrorToString(error));
                    FinalizeTransaction(requestHolder, nullptr, error);
                }
            }
            else
            {
                LOG_DEBUG("request to {} has been acknowledged, won't retransmit", uri);
            }
        }
        else
        {
            // No expected response or acknowledgment.
            FinalizeTransaction(requestHolder, nullptr, Error::kTimeout);
        }
    }
}

Error Coap::SendHeaderResponse(Code aCode, const Request &aRequest)
{
    Error    error = Error::kNone;
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
        ExitNow(error = Error::kInvalidArgs);
        break;
    }

    SuccessOrExit(error = SendResponse(aRequest, response));

exit:
    return error;
}

Error Coap::SendEmptyMessage(Type aType, const Request &aRequest)
{
    Error    error = Error::kNone;
    Response response{aType, Code::kEmpty};

    VerifyOrExit(aRequest.IsConfirmable(), error = Error::kInvalidArgs);

    response.SetMessageId(aRequest.GetMessageId());
    response.SetEndpoint(aRequest.GetEndpoint());

    SuccessOrExit(error = Send(response));

exit:
    return error;
}

Error Coap::Send(const Message &aMessage)
{
    Error     error = Error::kNone;
    ByteArray data;
    SuccessOrExit(error = aMessage.Serialize(data));

    if (aMessage.GetEndpoint() == nullptr)
    {
        aMessage.SetEndpoint(&mEndpoint);
    }

    error = aMessage.GetEndpoint()->Send(data);
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
        LOG_INFO("removed response cache: token={}, messageId={}", utils::Hex(response.GetToken()),
                 response.GetMessageId());
    }
}

void Coap::RequestsCache::Put(const RequestPtr aRequest, ResponseHandler aHandler)
{
    Put({aRequest, aHandler});
}

void Coap::RequestsCache::Put(const RequestHolder &aRequestHolder)
{
    mContainer.emplace(aRequestHolder);
    if (mRetransmissionTimer.IsRunning())
    {
        if (Earliest() < mRetransmissionTimer.GetFireTime())
        {
            mRetransmissionTimer.Start(Earliest());
        }
    }
    else
    {
        mRetransmissionTimer.Start(Earliest());
    }
}

Coap::RequestHolder Coap::RequestsCache::Eliminate()
{
    ASSERT(!IsEmpty());
    auto ret = *mContainer.begin();

    mContainer.erase(mContainer.begin());

    if (IsEmpty())
    {
        // No more requests pending, stop the timer.
        mRetransmissionTimer.Stop();
    }

    // No need to worry that the earliest pending message was removed -
    // the timer would just shoot earlier and then it'd be setup again.
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
}

void Coap::RequestsCache::Clear()
{
    mRetransmissionTimer.Stop();

    mContainer.clear();
}

void Coap::RequestsCache::TryRetartTimer()
{
    if (!mRetransmissionTimer.IsRunning() && !IsEmpty())
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
    Error    error = Error::kNone;
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

    return Error::kNone;
}

// Normalize the uri path by:
//   stripping spaces at the two ends;
//   adding '/' to the beginning if there is none;
//   applying percent-decode;
Error Message::NormalizeUriPath(std::string &aUriPath)
{
    size_t begin = 0, end = aUriPath.size();

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

    auto xdigit = [](char c) { return 'A' <= c && c <= 'F' ? c - 'A' : 'a' <= c && c <= 'f' ? c - 'a' : c - '0'; };
    while (true)
    {
        auto pos = aUriPath.find_first_of('%', begin);
        if (pos == std::string::npos)
        {
            break;
        }
        if (pos + 2 >= aUriPath.size())
        {
            return Error::kInvalidArgs;
        }
        if (!isxdigit(aUriPath[pos + 1]) || !isxdigit(aUriPath[pos + 2]))
        {
            return Error::kInvalidArgs;
        }

        char c = (xdigit(aUriPath[pos + 1]) << 4) | xdigit(aUriPath[pos + 2]);
        aUriPath.replace(pos, 3, 1, c);
        begin = pos + 1;
    }

    return Error::kNone;
}

std::shared_ptr<Message> Message::Deserialize(Error &aError, const ByteArray &aBuf)
{
    Error    error  = Error::kNone;
    size_t   offset = 0;
    uint16_t lastOptionNumber;
    auto     message = std::make_shared<Message>();

    SuccessOrExit(error = Deserialize(message->mHeader, aBuf, offset));
    VerifyOrExit(message->mHeader.IsValid(), error = Error::kBadFormat);

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
            error = Error::kBadFormat;
        }

        if (error != Error::kNone)
        {
            if (IsCriticalOption(number))
            {
                // Stop if any unrecognized option is critical.
                ExitNow(error = Error::kBadOption);
            }
            else
            {
                // Ignore non-critical option error.
                error = Error::kNone;
            }
        }

        lastOptionNumber = utils::to_underlying(number);
    }

    if (offset < aBuf.size())
    {
        if (aBuf[offset++] == kPayloadMarker)
        {
            VerifyOrExit(offset < aBuf.size(), error = Error::kBadFormat);
            message->mPayload.assign(aBuf.begin() + offset, aBuf.end());
        }
        else
        {
            // Ignore extra data.
            message->mPayload.clear();
        }
    }

exit:
    if (error != Error::kNone)
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
    ASSERT(aTokenLength <= sizeof(mHeader.mToken));

    mHeader.mTokenLength = aTokenLength;
    random::non_crypto::FillBuffer(mHeader.mToken, aTokenLength);
}

} // namespace coap

} // namespace commissioner

} // namespace ot
