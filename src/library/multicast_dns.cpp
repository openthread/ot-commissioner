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

#include "multicast_dns.hpp"

#include <memory>

#include <address.hpp>
#include <utils.hpp>

#include "logging.hpp"
#include "timer.hpp"

namespace ot {

namespace commissioner {

BorderAgentQuerier::BorderAgentQuerier(struct event_base *aEventBase)
    : mEventBase(aEventBase)
    , mSocket(-1)
    , mResponseHandler(nullptr)
{
}

BorderAgentQuerier::~BorderAgentQuerier()
{
    if (mSocket >= 0)
    {
        event_del(&mResponseEvent);
        mdns_socket_close(mSocket);
    }
}

Error BorderAgentQuerier::Setup()
{
    Error error = Error::kFailed;

    struct timeval tv;
    evutil_timerclear(&tv);
    tv.tv_sec = kQueryTimeout;

    int socket = mdns_socket_open_ipv4();

    VerifyOrExit(mSocket < 0, error = Error::kAlready);

    VerifyOrExit(socket >= 0, error = Error::kFailed);

    if (event_assign(&mResponseEvent, mEventBase, socket, EV_PERSIST | EV_READ, ReceiveResponse, this) != 0)
    {
        ExitNow(error = Error::kFailed);
    }
    VerifyOrExit(event_add(&mResponseEvent, &tv) == 0, error = Error::kFailed);

    error   = Error::kNone;
    mSocket = socket;

exit:
    if (error != Error::kNone)
    {
        if (socket >= 0)
        {
            mdns_socket_close(socket);
        }
    }
    return error;
}

void BorderAgentQuerier::SendQuery(ResponseHandler aHandler)
{
    static const mdns_record_type_t kMdnsQueryType = MDNS_RECORDTYPE_PTR;
    static const char *             kServiceName   = "_meshcop._udp.local";

    Error   error = Error::kNone;
    uint8_t buf[kDefaultBufferSize];

    VerifyOrExit(mSocket < 0, error = Error::kBusy);
    SuccessOrExit(error = Setup());

    if (mdns_query_send(mSocket, kMdnsQueryType, kServiceName, strlen(kServiceName), buf, sizeof(buf)) != 0)
    {
        ExitNow(error = Error::kFailed);
    }

    mResponseHandler = aHandler;

exit:
    if (error != Error::kNone)
    {
        aHandler(nullptr, error);
    }
}

void BorderAgentQuerier::ReceiveResponse(evutil_socket_t aSocket, short aFlags, void *aContext)
{
    auto querier = reinterpret_cast<BorderAgentQuerier *>(aContext);
    querier->ReceiveResponse(aSocket, aFlags);
}

void BorderAgentQuerier::ReceiveResponse(evutil_socket_t aSocket, short aFlags)
{
    if (aFlags & EV_TIMEOUT)
    {
        LOG_INFO("found {} Border Agents", mBorderAgents.size());

        if (mResponseHandler != nullptr)
        {
            mResponseHandler(&mBorderAgents, Error::kNone);
            mResponseHandler = nullptr;
        }

        // The BorderAgentQuerier caches the discovered Border Agents, but doesn't store.
        mBorderAgents.clear();

        if (mSocket >= 0)
        {
            event_del(&mResponseEvent);
            mdns_socket_close(mSocket);
            mSocket = -1;
        }
    }
    else
    {
        uint8_t buf[kDefaultBufferSize];

        mCurBorderAgent.mPresentFlags = 0;
        mdns_query_recv(aSocket, buf, sizeof(buf), HandleRecord, this, 1);
        if (ValidateBorderAgent(mCurBorderAgent))
        {
            mBorderAgents.push_back(mCurBorderAgent);
        }
    }
}

static inline std::string ToString(const mdns_string_t &aString)
{
    return std::string(aString.str, aString.length);
}

int BorderAgentQuerier::HandleRecord(const struct sockaddr *from,
                                     mdns_entry_type_t      entry,
                                     uint16_t               type,
                                     uint16_t               rclass,
                                     uint32_t               ttl,
                                     const void *           data,
                                     size_t                 size,
                                     size_t                 offset,
                                     size_t                 length,
                                     void *                 user_data)
{
    return reinterpret_cast<BorderAgentQuerier *>(user_data)->HandleRecord(from, entry, type, rclass, ttl, data, size,
                                                                           offset, length);
}

int BorderAgentQuerier::HandleRecord(const struct sockaddr *from,
                                     mdns_entry_type_t      entry,
                                     uint16_t               type,
                                     uint16_t               rclass,
                                     uint32_t               ttl,
                                     const void *           data,
                                     size_t                 size,
                                     size_t                 offset,
                                     size_t                 length)
{
    Error                   error = Error::kNone;
    struct sockaddr_storage fromAddrStorage;
    Address                 fromAddr;
    std::string             fromAddrStr;
    std::string             entryType;
    char                    nameBuffer[256];

    *reinterpret_cast<struct sockaddr *>(&fromAddrStorage) = *from;
    SuccessOrExit(error = fromAddr.Set(fromAddrStorage));
    SuccessOrExit(error = fromAddr.ToString(fromAddrStr));

    entryType = (entry == MDNS_ENTRYTYPE_ANSWER) ? "answer"
                                                 : ((entry == MDNS_ENTRYTYPE_AUTHORITY) ? "authority" : "additional");
    if (type == MDNS_RECORDTYPE_PTR)
    {
        mdns_string_t nameStr = mdns_record_parse_ptr(data, size, offset, length, nameBuffer, sizeof(nameBuffer));
        LOG_INFO("[mDNS] received from {}: {} PTR={}, type={}, rclass={}, ttl={}, length={}", fromAddrStr, entryType,
                 ToString(nameStr), type, rclass, ttl, length);
    }
    else if (type == MDNS_RECORDTYPE_SRV)
    {
        mdns_record_srv_t server = mdns_record_parse_srv(data, size, offset, length, nameBuffer, sizeof(nameBuffer));
        LOG_INFO("[mDNS] received from {}: {} SRV={}, priority={}, weight={}, port={}", fromAddrStr, entryType,
                 ToString(server.name), server.priority, server.weight, server.port);

        mCurBorderAgent.mPort = server.port;
        mCurBorderAgent.mPresentFlags |= BorderAgent::kPortBit;
    }
    else if (type == MDNS_RECORDTYPE_A)
    {
        struct sockaddr_storage addrStorage;
        struct sockaddr_in      addr;
        std::string             addrStr;

        mdns_record_parse_a(data, size, offset, length, &addr);

        *reinterpret_cast<struct sockaddr_in *>(&addrStorage) = addr;
        SuccessOrExit(error = fromAddr.Set(addrStorage));
        SuccessOrExit(error = fromAddr.ToString(addrStr));

        LOG_INFO("[mDNS] received from {}: {} A={}", fromAddrStr, entryType, addrStr);

        // We prefer AAAA (IPv6) address than A (IPv4) address.
        if (!(mCurBorderAgent.mPresentFlags & BorderAgent::kAddrBit))
        {
            mCurBorderAgent.mAddr = addrStr;
            mCurBorderAgent.mPresentFlags |= BorderAgent::kAddrBit;
        }
    }
    else if (type == MDNS_RECORDTYPE_AAAA)
    {
        struct sockaddr_storage addrStorage;
        struct sockaddr_in6     addr;
        std::string             addrStr;

        mdns_record_parse_aaaa(data, size, offset, length, &addr);

        *reinterpret_cast<struct sockaddr_in6 *>(&addrStorage) = addr;
        SuccessOrExit(error = fromAddr.Set(addrStorage));
        SuccessOrExit(error = fromAddr.ToString(addrStr));

        LOG_INFO("[mDNS] received from {}: {} AAAA={}", fromAddrStr, entryType, addrStr);

        mCurBorderAgent.mAddr = addrStr;
        mCurBorderAgent.mPresentFlags |= BorderAgent::kAddrBit;
    }
    else if (type == MDNS_RECORDTYPE_TXT)
    {
        mdns_record_txt_t txtBuffer[128];
        size_t            parsed =
            mdns_record_parse_txt(data, size, offset, length, txtBuffer, sizeof(txtBuffer) / sizeof(mdns_record_txt_t));
        for (size_t i = 0; i < parsed; ++i)
        {
            auto key   = ToString(txtBuffer[i].key);
            auto value = ToString(txtBuffer[i].value);

            LOG_INFO("[mDNS] received from {}: {} TXT.{}={}", fromAddrStr, entryType, key, value);

            if (key == "rv")
            {
                if (value != "1")
                {
                    LOG_ERROR("[mDNS] value of TXT Key 'rv' is not '1'");
                    ExitNow(error = Error::kBadFormat);
                }
            }
            else if (key == "tv")
            {
                mCurBorderAgent.mThreadVersion = value;
                mCurBorderAgent.mPresentFlags |= BorderAgent::kThreadVersionBit;
            }
            else if (key == "sb")
            {
                ByteArray bitmap;
                if (utils::Hex(bitmap, value) != Error::kNone || bitmap.size() != 4)
                {
                    LOG_ERROR("[mDNS] value of TXT Key 'sb' is invalid: {}", value);
                    ExitNow(error = Error::kBadFormat);
                }

                mCurBorderAgent.mState.mConnectionMode = (bitmap[0] >> 5);
                mCurBorderAgent.mState.mThreadIfStatus = (bitmap[0] << 3) >> 6;
                mCurBorderAgent.mState.mAvailability   = (bitmap[0] << 5) >> 6;
                mCurBorderAgent.mState.mBbrIsActive    = (bitmap[0] << 7) >> 7;
                mCurBorderAgent.mState.mBbrIsPrimary   = (bitmap[1] >> 7);
                mCurBorderAgent.mPresentFlags |= BorderAgent::kStateBit;
            }
            else if (key == "nn")
            {
                mCurBorderAgent.mNetworkName = value;
                mCurBorderAgent.mPresentFlags |= BorderAgent::kNetworkNameBit;
            }
            else if (key == "xp")
            {
                ByteArray extendPanId;
                if (utils::Hex(extendPanId, value) != Error::kNone || extendPanId.size() != 8)
                {
                    LOG_WARN("[mDNS] value of TXT Key 'xp' is invalid: {}", value);
                }
                else
                {
                    mCurBorderAgent.mExtendedPanId = utils::Decode<uint64_t>(extendPanId);
                    mCurBorderAgent.mPresentFlags |= BorderAgent::kExtendedPanIdBit;
                }
            }
            else if (key == "vn")
            {
                mCurBorderAgent.mVendorName = value;
                mCurBorderAgent.mPresentFlags |= BorderAgent::kVendorNameBit;
            }
            else if (key == "mn")
            {
                mCurBorderAgent.mModelName = value;
                mCurBorderAgent.mPresentFlags |= BorderAgent::kModelNameBit;
            }
            else if (key == "at")
            {
                ByteArray activeTimestamp;
                if (utils::Hex(activeTimestamp, value) != Error::kNone || activeTimestamp.size() != 8)
                {
                    LOG_WARN("[mDNS] value of TXT Key 'at' is invalid: {}", value);
                }
                else
                {
                    mCurBorderAgent.mActiveTimestamp = Timestamp::Decode(utils::Decode<uint64_t>(activeTimestamp));
                    mCurBorderAgent.mPresentFlags |= BorderAgent::kActiveTimestampBit;
                }
            }
            else if (key == "pt")
            {
                ByteArray partitionId;
                if (utils::Hex(partitionId, value) != Error::kNone || partitionId.size() != 4)
                {
                    LOG_WARN("[mDNS] value of TXT Key 'pt' is invalid: {}", value);
                }
                else
                {
                    mCurBorderAgent.mPartitionId = utils::Decode<uint32_t>(partitionId);
                    mCurBorderAgent.mPresentFlags |= BorderAgent::kPartitionIdBit;
                }
            }
            else if (key == "vd")
            {
                mCurBorderAgent.mVendorData = value;
                mCurBorderAgent.mPresentFlags |= BorderAgent::kVendorDataBit;
            }
            else if (key == "vo")
            {
                ByteArray oui;
                if (utils::Hex(oui, value) != Error::kNone || oui.size() != 3)
                {
                    LOG_WARN("[mDNS] value of TXT Key 'vo' is invalid: {}", value);
                }
                else
                {
                    mCurBorderAgent.mVendorOui = oui;
                    mCurBorderAgent.mPresentFlags |= BorderAgent::kVendorOuiBit;
                }
            }
            else if (key == "dn")
            {
                mCurBorderAgent.mDomainName = value;
                mCurBorderAgent.mPresentFlags |= BorderAgent::kDomainNameBit;
            }
            else if (key == "sq")
            {
                ByteArray bbrSeqNum;
                if (utils::Hex(bbrSeqNum, value) != Error::kNone || bbrSeqNum.size() != 1)
                {
                    LOG_WARN("[mDNS] value of TXT Key 'vo' is invalid: {}", value);
                }
                else
                {
                    mCurBorderAgent.mBbrSeqNumber = utils::Decode<uint8_t>(bbrSeqNum);
                    mCurBorderAgent.mPresentFlags |= BorderAgent::kBbrSeqNumberBit;
                }
            }
            else if (key == "bb")
            {
                ByteArray bbrPort;
                if (utils::Hex(bbrPort, value) != Error::kNone || bbrPort.size() != 2)
                {
                    LOG_WARN("[mDNS] value of TXT Key 'vo' is invalid: {}", value);
                }
                else
                {
                    mCurBorderAgent.mBbrPort = utils::Decode<uint16_t>(bbrPort);
                    mCurBorderAgent.mPresentFlags |= BorderAgent::kBbrPortBit;
                }
            }
            else
            {
                LOG_WARN("[mDNS] unknown TXT key: {}", key);
            }
        }
    }
    else
    {
        LOG_INFO("[mDNS] received from {}: {} type={}, rclass={}, ttl={}, length={}", fromAddrStr, entryType, type,
                 rclass, ttl, length);
    }

exit:
    return error == Error::kNone ? 0 : -1;
}

bool BorderAgentQuerier::ValidateBorderAgent(const BorderAgent &aBorderAgent)
{
    bool ret = true;
    if (!(aBorderAgent.mPresentFlags & BorderAgent::kAddrBit))
    {
        LOG_ERROR("'addr' of a border agent is mandatory");
        ret = false;
    }
    if (!(aBorderAgent.mPresentFlags & BorderAgent::kPortBit))
    {
        LOG_ERROR("'port' of a border agent is mandatory");
        ret = false;
    }
    if (!(aBorderAgent.mPresentFlags & BorderAgent::kThreadVersionBit))
    {
        LOG_ERROR("'thread version' of a border agent is mandatory");
        ret = false;
    }
    if (!(aBorderAgent.mPresentFlags & BorderAgent::kStateBit))
    {
        LOG_ERROR("'state bitmap' of a border agent is mandatory");
        ret = false;
    }
    if (!(aBorderAgent.mPresentFlags & BorderAgent::kNetworkNameBit) &&
        (aBorderAgent.mPresentFlags & BorderAgent::kStateBit) && aBorderAgent.mState.mConnectionMode != 0)
    {
        LOG_ERROR("'network name' of a border agent is mandatory when connection mode is not '0'");
        ret = false;
    }
    if (!(aBorderAgent.mPresentFlags & BorderAgent::kExtendedPanIdBit) &&
        (aBorderAgent.mPresentFlags & BorderAgent::kStateBit) && aBorderAgent.mState.mConnectionMode != 0)
    {
        LOG_ERROR("'extended PAN ID' of a border agent is mandatory when connection mode is not '0'");
        ret = false;
    }
    if (!(aBorderAgent.mPresentFlags & BorderAgent::kVendorOuiBit) &&
        (aBorderAgent.mPresentFlags & BorderAgent::kVendorDataBit))
    {
        LOG_ERROR("'vendor OUI' of a border agent is mandatory when 'vendor data' is present");
        ret = false;
    }
    return ret;
}

} // namespace commissioner

} // namespace ot
