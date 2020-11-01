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
 *   This file includes definitions for CoAP.
 */

#ifndef OT_COMM_LIBRARY_COAP_HPP_
#define OT_COMM_LIBRARY_COAP_HPP_

#include <algorithm>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <queue>
#include <set>

#include <commissioner/defines.hpp>
#include <commissioner/error.hpp>

#include "common/address.hpp"
#include "common/utils.hpp"
#include "library/endpoint.hpp"
#include "library/message.hpp"
#include "library/timer.hpp"

namespace ot {

namespace commissioner {

namespace coap {

/**
 * CoAP Type values.
 *
 */
enum class Type : uint8_t
{
    kConfirmable    = 0x00,
    kNonConfirmable = 0x01,
    kAcknowledgment = 0x02,
    kReset          = 0x03,
};

/**
 * Helper macro to define CoAP Code values.
 *
 */
#define OT_COAP_CODE(c, d) ((((c)&0x7u) << 5u) | ((d)&0x1fu))

/**
 * CoAP Code values.
 *
 */
enum class Code : uint8_t
{
    /*
     * Methods (0.XX).
     */
    kEmpty  = OT_COAP_CODE(0, 0),
    kGet    = OT_COAP_CODE(0, 1),
    kPost   = OT_COAP_CODE(0, 2),
    kPut    = OT_COAP_CODE(0, 3),
    kDelete = OT_COAP_CODE(0, 4),

    /*
     * Success (2.XX).
     */
    kResponseMin = OT_COAP_CODE(2, 0),
    kCreated     = OT_COAP_CODE(2, 1),
    kDeleted     = OT_COAP_CODE(2, 2),
    kValid       = OT_COAP_CODE(2, 3),
    kChanged     = OT_COAP_CODE(2, 4),
    kContent     = OT_COAP_CODE(2, 5),

    /*
     * Client side errors (4.XX).
     */
    kBadRequest         = OT_COAP_CODE(4, 0),
    kUnauthorized       = OT_COAP_CODE(4, 1),
    kBadOption          = OT_COAP_CODE(4, 2),
    kForBidden          = OT_COAP_CODE(4, 3),
    kNotFound           = OT_COAP_CODE(4, 4),
    kMethodNotAllowed   = OT_COAP_CODE(4, 5),
    kNotAcceptable      = OT_COAP_CODE(4, 6),
    kPreconditionFailed = OT_COAP_CODE(4, 12),
    kRequestTooLarge    = OT_COAP_CODE(4, 13),
    kUnsupportedFormat  = OT_COAP_CODE(4, 15),

    /*
     * Server side errors (5.XX).
     */
    kInternalError      = OT_COAP_CODE(5, 0),
    kNotImplemented     = OT_COAP_CODE(5, 1),
    kBadGateway         = OT_COAP_CODE(5, 2),
    kServiceUnavailable = OT_COAP_CODE(5, 3),
    kGatewayTimeout     = OT_COAP_CODE(5, 4),
    kProxyNotSupported  = OT_COAP_CODE(5, 5),
};

#undef OT_COAP_CODE

/**
 * CoAP Option Numbers
 */
enum class OptionType : uint16_t
{
    kIfMatch       = 1,
    kUriHost       = 3,
    kETag          = 4,
    kIfNonMatch    = 5,
    kObserve       = 6,
    kUriPort       = 7,
    kLocationPath  = 8,
    kUriPath       = 11,
    kContentFormat = 12,
    kMaxAge        = 14,
    kUriQuery      = 15,
    kAccept        = 17,
    kLocationQuery = 20,
    kProxyUri      = 35,
    kProxyScheme   = 39,
    kSize1         = 60,
};

/**
 * CoAP Content Format codes.  The full list is documented at
 * https://www.iana.org/assignments/core-parameters/core-parameters.xhtml#content-formats
 */
enum class ContentFormat : uint32_t
{
    /**
     * text/plain; charset=utf-8: [RFC2046][RFC3676][RFC5147]
     */
    kTextPlain = 0,

    /**
     * application/cose; cose-type="cose-encrypt0": [RFC8152]
     */
    kCoseEncrypt0 = 16,

    /**
     * application/cose; cose-type="cose-mac0": [RFC8152]
     */
    kCoseMac0 = 17,

    /**
     * application/cose; cose-type="cose-sign1": [RFC8152]
     */
    kCoseSign1 = 18,

    /**
     * application/link-format: [RFC6690]
     */
    kLinkFormat = 40,

    /**
     * application/xml: [RFC3023]
     */
    kXML = 41,

    /**
     * application/octet-stream: [RFC2045][RFC2046]
     */
    kOctetStream = 42,

    /**
     * application/exi:
     * ["Efficient XML Interchange (EXI) Format 1.0 (Second Edition)", February 2014]
     */
    kEXI = 47,

    /**
     * application/json: [RFC7159]
     */
    kJSON = 50,

    /**
     * application/json-patch+json: [RFC6902]
     */
    kJSONPatchJSON = 51,

    /**
     * application/merge-patch+json: [RFC7396]
     */
    kMergePatchJSON = 52,

    /**
     * application/cbor: [RFC7049]
     */
    kCBOR = 60,

    /**
     * application/cwt: [RFC8392]
     */
    kCWT = 61,

    /**
     * application/cose; cose-type="cose-encrypt": [RFC8152]
     */
    kCoseEncrypt = 96,

    /**
     * application/cose; cose-type="cose-mac": [RFC8152]
     */
    kCoseMac = 97,

    /**
     * application/cose; cose-type="cose-sign": [RFC8152]
     */
    kCoseSign = 98,

    /**
     * application/cose-key: [RFC8152]
     */
    kCoseKey = 101,

    /**
     * application/cose-key-set: [RFC8152]
     */
    kCoseKeySign = 102,

    /**
     * application/senml+json: [RFC8428]
     */
    kSenmlJSON = 110,

    /**
     * application/sensml+json: [RFC8428]
     */
    kSensmlJSON = 111,

    /**
     * application/senml+cbor: [RFC8428]
     */
    kSenmlCBOR = 112,

    /**
     * application/sensml+cbor: [RFC8428]
     */
    kSensmlCBOR = 113,

    /**
     * application/senml-exi: [RFC8428]
     */
    kSenmlEXI = 114,

    /**
     * application/sensml-exi: [RFC8428]
     */
    kSensmlEXI = 115,

    /**
     * application/coap-group+json: [RFC7390]
     */
    kCOAPGroupJSON = 256,

    /**
     * application/csrattrs: [RFC7030]
     */
    kCSRAttrs = 285,

    /**
     * application/pkcs10: [RFC5967]
     */
    kPKCS10 = 286,

    /**
     * application/senml+xml: [RFC8428]
     */
    kSenmlXML = 310,

    /**
     * application/sensml+xml: [RFC8428]
     */
    kSensmlXML = 311
};

class OptionValue
{
public:
    OptionValue() = default;
    OptionValue(const ByteArray &aOpaque)
        : mValue(aOpaque)
    {
    }
    OptionValue(const std::string &aString)
        : mValue(aString.begin(), aString.end())
    {
    }
    OptionValue(uint32_t aUint32)
    {
        // Encoding an unsigned integer without preceding zeros.
        while (aUint32 != 0)
        {
            mValue.push_back(aUint32 & 0xff);
            aUint32 >>= 8;
        }
        std::reverse(mValue.begin(), mValue.end());
    }

    const ByteArray &GetOpaqueValue() const { return mValue; }

    std::string GetStringValue() const { return std::string(mValue.begin(), mValue.end()); }

    uint32_t GetUint32Value() const
    {
        VerifyOrDie(mValue.size() <= sizeof(uint32_t));
        uint32_t ret = 0;
        for (auto byte : mValue)
        {
            ret = (ret << 8) | byte;
        }
        return ret;
    }

    size_t GetLength() const { return mValue.size(); }

private:
    ByteArray mValue;
};

/**
 * Protocol Constants (RFC 7252).
 *
 */
static constexpr int kAckTimeout                 = 2; ///< In seconds
static constexpr int kAckRandomFactorNumerator   = 3;
static constexpr int kAckRandomFactorDenominator = 2;
static constexpr int kMaxRetransmit              = 4;
static constexpr int kNStart                     = 1;
static constexpr int kDefaultLeisure             = 5;
static constexpr int kProbingRate                = 1; ///< bytes/second

// Note that 2 << (kMaxRetransmit - 1) is equal to kMaxRetransmit power of 2
static constexpr int kMaxTransmitSpan =
    kAckTimeout * ((2 << (kMaxRetransmit - 1)) - 1) * kAckRandomFactorNumerator / kAckRandomFactorDenominator;
static constexpr int      kMaxLatency       = 100;
static constexpr int      kProcessingDelay  = kAckTimeout;
static constexpr int      kMaxRtt           = 2 * kMaxLatency + kProcessingDelay;
static constexpr uint32_t kExchangeLifetime = kMaxTransmitSpan + 2 * (kMaxLatency) + kProcessingDelay;

struct MessageInfo
{
    Address  mSockAddr;
    Address  mPeerAddr;
    uint16_t mSockPort;
    uint16_t mPeerPort;

    bool EqualPeer(const MessageInfo &aOther) const
    {
        return mPeerPort == aOther.mPeerPort && mPeerAddr == aOther.mPeerAddr;
    }
};

/**
 * Protocol Constants (RFC 7252).
 *
 */
static constexpr size_t  kOptionDeltaOffset = 4;
static constexpr uint8_t kOptionDeltaMask   = 0x0F << kOptionDeltaOffset;

static constexpr size_t kMaxOptionHeaderSize = 5;

static constexpr uint8_t kOption1ByteExtension = 13; ///< RFC 7252.
static constexpr uint8_t kOption2ByteExtension = 14; ///< RFC 7252.

static constexpr uint16_t kOption1ByteExtensionOffset = 13;  ///< RFC 7252.
static constexpr uint16_t kOption2ByteExtensionOffset = 269; ///< RFC 7252.
static constexpr uint8_t  kPayloadMarker              = 0xFF;

static constexpr uint8_t kVersion1           = 1;
static constexpr uint8_t kMaxTokenLength     = 8;
static constexpr uint8_t kDefaultTokenLength = kMaxTokenLength;

/**
 * This class implements CoAP message generation and parsing.
 *
 */
class Message
{
    friend class Coap;

public:
    struct Header
    {
        uint8_t  mVersion : 2;
        uint8_t  mType : 2;
        uint8_t  mTokenLength : 4;
        uint8_t  mCode;
        uint16_t mMessageId;
        uint8_t  mToken[kMaxTokenLength];

        Header()
            : mVersion(kVersion1)
            , mType(0)
            , mTokenLength(0)
            , mCode(0)
            , mMessageId(0)
        {
        }
        bool IsValid() const;
    };

    // Read and deserialize a message from the buffer.
    static std::shared_ptr<Message> Deserialize(Error &aError, const ByteArray &aBuf);
    Error                           Serialize(ByteArray &aBuf) const;

    Message(Type aType, Code aCode);
    Message();

    const Header &GetHeader() const { return mHeader; }

    size_t GetHeaderLength() const;

    uint8_t GetVersion(void) const { return mHeader.mVersion; }

    Type GetType(void) const { return utils::from_underlying<Type>(mHeader.mType); }

    void SetType(Type aType) { mHeader.mType = utils::to_underlying(aType); }

    Code GetCode(void) const { return utils::from_underlying<Code>(mHeader.mCode); }

    void SetCode(Code aCode) { mHeader.mCode = utils::to_underlying(aCode); }

    uint16_t GetMessageId(void) const { return mHeader.mMessageId; }

    const ByteArray GetToken() const
    {
        auto len = std::min(sizeof(mHeader.mToken), static_cast<size_t>(mHeader.mTokenLength));
        return ByteArray(mHeader.mToken, mHeader.mToken + len);
    }

    size_t GetOptionNum() const { return mOptions.size(); }

    Error SetUriPath(const std::string &aUriPath);
    Error GetUriPath(std::string &aUriPath) const { return GetOption(aUriPath, OptionType::kUriPath); }

    Error SetAccept(ContentFormat aAcceptFormat)
    {
        return AppendOption(OptionType::kAccept, utils::to_underlying(aAcceptFormat));
    }
    Error GetAccept(ContentFormat &aAcceptFormat) const;

    Error SetContentFormat(ContentFormat aContentFormat)
    {
        return AppendOption(OptionType::kContentFormat, utils::to_underlying(aContentFormat));
    }
    Error GetContentFormat(ContentFormat &aContentFormat) const;

    bool IsEmpty(void) const { return (GetCode() == Code::kEmpty); };

    bool IsRequest(void) const
    {
        return GetCode() == Code::kGet || GetCode() == Code::kPost || GetCode() == Code::kPut ||
               GetCode() == Code::kDelete;
    }

    bool IsResponse(void) const { return !IsEmpty() && !IsRequest(); }

    bool IsConfirmable(void) const { return (GetType() == Type::kConfirmable); };

    bool IsNonConfirmable(void) const { return (GetType() == Type::kNonConfirmable); };

    bool IsAck(void) const { return (GetType() == Type::kAcknowledgment); };

    bool IsReset(void) const { return (GetType() == Type::kReset); };

    void Append(const ByteArray &aData) { mPayload.insert(mPayload.end(), aData.begin(), aData.end()); }

    void Append(const std::string &aData) { mPayload.insert(mPayload.end(), aData.begin(), aData.end()); }

    const ByteArray &GetPayload() const { return mPayload; }

    std::string GetPayloadAsString() const { return std::string{mPayload.begin(), mPayload.end()}; }

    Endpoint *GetEndpoint() const { return mEndpoint; }

    MessageSubType GetSubType() const { return mSubType; }
    void           SetSubType(MessageSubType aSubType) { mSubType = aSubType; }

    static Error NormalizeUriPath(std::string &uriPath);

protected:
    Error        Serialize(const Header &aHeader, ByteArray &aBuf) const;
    static Error Deserialize(Header &aHeader, const ByteArray &aBuf, size_t &aOffset);
    Error        Serialize(OptionType         aOptionNumber,
                           const OptionValue &aOptionValue,
                           uint16_t           aLastOptionNumber,
                           ByteArray &        aBuf) const;
    static Error Deserialize(OptionType &     aOptionNumber,
                             OptionValue &    aOptionValue,
                             uint16_t         aLastOptionNumber,
                             const ByteArray &aBuf,
                             size_t &         aOffset);

    // Split the URI path by slash ('/').
    static Error SplitUriPath(std::list<std::string> &aUriPathList, const std::string &aUriPath);

    Error AppendOption(OptionType aType, const OptionValue &aValue);

    Error              GetOption(std::string &aValue, OptionType aNumber) const;
    Error              GetOption(std::uint32_t &aValue, OptionType aNumber) const;
    Error              GetOption(ByteArray &aValue, OptionType aNumber) const;
    const OptionValue *GetOption(OptionType aNumber) const;
    static bool        IsValidOption(OptionType aNumber, const OptionValue &aValue);
    static bool        IsCriticalOption(OptionType aNumber);

    void SetVersion(uint8_t aVersion) { mHeader.mVersion = aVersion; }
    void SetMessageId(uint16_t aMessageId) { mHeader.mMessageId = aMessageId; }
    void SetToken(const ByteArray &aToken);
    void SetToken(uint8_t aTokenLength);
    bool IsTokenEqual(const Message &aMessage) const;

    void SetEndpoint(Endpoint *aEndpoint) const { mEndpoint = aEndpoint; }

    Header                            mHeader;
    std::map<OptionType, OptionValue> mOptions;
    ByteArray                         mPayload;

    MessageSubType mSubType;

    mutable Endpoint *mEndpoint = nullptr;
};

using Request         = Message;
using Response        = Message;
using RequestPtr      = std::shared_ptr<Request>;
using ResponsePtr     = std::shared_ptr<Response>;
using RequestHandler  = std::function<void(const Request &)>;
using ResponseHandler = std::function<void(const Response *, Error)>;

/**
 * This class implements CoAP resource handling.
 *
 */
class Resource
{
    friend class Coap;

public:
    // `aHandler` is nullable.
    Resource(const std::string &aUriPath, RequestHandler aHandler)
        : mUriPath(aUriPath)
        , mHandler(aHandler)
    {
    }

    const std::string &GetUriPath(void) const { return mUriPath; }

private:
    void HandleRequest(const Message &aMessage) const
    {
        if (mHandler != nullptr)
        {
            mHandler(aMessage);
        }
    }

    std::string    mUriPath;
    RequestHandler mHandler;
};

/**
 * This class implements the CoAP client and server.
 *
 */
class Coap
{
public:
    Coap(struct event_base *aEventBase, Endpoint &aEndpoint);
    virtual ~Coap() = default;

    // Cancel all outstanding requests
    void CancelRequests();

    // Clear/Cancel requests and clear response caches.
    void ClearRequestsAndResponses(void);

    size_t GetPendingRequestsNum() const { return mRequestsCache.Count(); }
    size_t GetCachedResponsesNum() const { return mResponsesCache.Count(); }

    Error AddResource(const Resource &aResource);

    void RemoveResource(const Resource &aResource);
    void SetDefaultHandler(RequestHandler aHandler);

    // If `aRequest` is confirmable, `aHandler` is guaranteed to be called;
    // Otherwise, `aHandler` will be called only when failed to send the request.
    void SendRequest(const Request &aRequest, ResponseHandler aHandler);

    Error SendReset(const Request &aRequest) { return SendEmptyMessage(Type::kReset, aRequest); };

    Error SendHeaderResponse(Code aCode, const Request &aRequest);

    // Send the response corresponding to specified request.
    Error SendResponse(const Request &aRequest, Response &aResponse);

    Error SendEmptyChanged(const Request &aRequest);

    Error SendAck(const Request &aRequest) { return SendEmptyMessage(Type::kAcknowledgment, aRequest); }

    Error SendNotFound(const Request &aRequest) { return SendHeaderResponse(Code::kNotFound, aRequest); }

    void Receive(const ByteArray &aBuf) { Receive(mEndpoint, aBuf); }

private:
    /**
     * The holder of a CoAP request along with retransmission metadata
     * and response handler.
     */
    struct RequestHolder
    {
        RequestHolder(const RequestPtr aRequest, ResponseHandler aHandler);

        RequestPtr              mRequest;
        mutable ResponseHandler mHandler;
        uint32_t                mRetransmissionCount;
        Duration                mRetransmissionDelay;
        TimePoint               mNextTimerShot;
        mutable bool            mAcknowledged;

        bool operator<(const RequestHolder &aOther) const { return mNextTimerShot < aOther.mNextTimerShot; }
    };

    /**
     * The request cache of all oustanding requests.
     */
    class RequestsCache
    {
    public:
        RequestsCache(struct event_base *aEventBase, Timer::Action aRetransmitter)
            : mRetransmissionTimer(aEventBase, aRetransmitter)
        {
        }
        ~RequestsCache() = default;

        void Put(const RequestPtr aRequest, ResponseHandler aHandler);
        void Put(const RequestHolder &aRequestHolder);

        // Find corresponding request with the response.
        const RequestHolder *Match(const Response &aResponse) const;

        // Remove and return the earliest request.
        RequestHolder Eliminate();

        // Find and remove the specified request.
        void Eliminate(const RequestHolder &aRequestHolder);

        TimePoint Earliest() const
        {
            VerifyOrDie(!IsEmpty());
            return mContainer.begin()->mNextTimerShot;
        }

        size_t Count() const { return mContainer.size(); }
        bool   IsEmpty() const { return mContainer.empty(); }

        const RequestHolder &Front() const
        {
            VerifyOrDie(!IsEmpty());
            return *mContainer.begin();
        }

        // Try starting the retransmit timer if it is not running
        // and there is pending requests. Try stopping the retransmit
        // timer if there is no more pending requests.
        void UpdateTimer();

    private:
        Timer                        mRetransmissionTimer;
        std::multiset<RequestHolder> mContainer;
    };

    /**
     * The cache of all sent out response to avoid reprocessing of
     * the same requests.
     */
    class ResponsesCache
    {
    public:
        explicit ResponsesCache(struct event_base *aEventBase, const Duration &aLifetime)
            : mLifetime(aLifetime)
            , mTimer(aEventBase, [this](Timer &) { Eliminate(); })
        {
        }
        ~ResponsesCache() = default;

        void Put(const Response &aResponse) { mContainer.emplace(Clock::now() + mLifetime, aResponse); }

        // Find the response of a request in the response cache.
        const Response *Match(const Request &aRequest) const;

        size_t Count() const { return mContainer.size(); }
        bool   IsEmpty() const { return mContainer.empty(); }

        void Clear();

    private:
        // Remove all response caches that have expired.
        void Eliminate();

        Duration mLifetime;

        // The timer to remove a response.
        Timer                              mTimer;
        std::multimap<TimePoint, Response> mContainer;
    };

    uint16_t AllocMessageId() { return ++mMessageId; }

    void Receive(Endpoint &aEndpoint, const ByteArray &aBuf);
    void ReceiveMessage(Endpoint &aEndpoint, std::shared_ptr<Message> aMessage, Error error);

    void Retransmit(Timer &aTimer);

    void FinalizeTransaction(const RequestHolder &aRequestHolder, const Response *aResponse, Error aResult);

    void HandleRequest(const Request &aRequest);

    // Handle empty and response message
    void HandleResponse(const Response &aResponse);

    Error SendEmptyMessage(Type aType, const Request &aRequest);

    Error Send(const Message &aMessage);

private:
    uint16_t mMessageId;

    std::map<std::string, Resource> mResources;

    RequestsCache  mRequestsCache;
    ResponsesCache mResponsesCache;

    // The default request handler when there is matching resource.
    RequestHandler mDefaultHandler;

    Endpoint &mEndpoint;
};

} // namespace coap

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_LIBRARY_COAP_HPP_
