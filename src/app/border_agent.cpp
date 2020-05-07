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

#include "border_agent.hpp"

#include <chrono>
#include <thread>

#include <fmt/format.h>
#include <mdns/mdns.h>

#include "common/address.hpp"
#include "common/utils.hpp"

namespace ot {

namespace commissioner {

struct BorderAgentOrErrorMsg
{
    BorderAgent mBorderAgent;
    std::string mErrorMsg;
};

static int HandleRecord(const struct sockaddr *from,
                        mdns_entry_type_t      entry,
                        uint16_t               type,
                        uint16_t               rclass,
                        uint32_t               ttl,
                        const void *           data,
                        size_t                 size,
                        size_t                 offset,
                        size_t                 length,
                        void *                 user_data);

Error DiscoverBorderAgent(BorderAgentHandler aBorderAgentHandler, size_t aTimeout)
{
    static constexpr size_t             kDefaultBufferSize = 1024 * 16;
    static constexpr mdns_record_type_t kMdnsQueryType     = MDNS_RECORDTYPE_PTR;
    static const char *                 kServiceName       = "_meshcop._udp.local";

    Error   error = Error::kNone;
    uint8_t buf[kDefaultBufferSize];

    auto begin = std::chrono::system_clock::now();

    int socket = mdns_socket_open_ipv4();
    VerifyOrExit(socket >= 0, error = Error::kTransportFailed);

    if (mdns_query_send(socket, kMdnsQueryType, kServiceName, strlen(kServiceName), buf, sizeof(buf)) != 0)
    {
        ExitNow(error = Error::kTransportFailed);
    }

    while (begin + std::chrono::milliseconds(aTimeout) >= std::chrono::system_clock::now())
    {
        BorderAgentOrErrorMsg curBorderAgentOrErrorMsg;

        mdns_query_recv(socket, buf, sizeof(buf), HandleRecord, &curBorderAgentOrErrorMsg, 1);

        if (!curBorderAgentOrErrorMsg.mErrorMsg.empty())
        {
            aBorderAgentHandler(nullptr, &curBorderAgentOrErrorMsg.mErrorMsg);
        }
        else if (curBorderAgentOrErrorMsg.mBorderAgent.mPresentFlags != 0)
        {
            aBorderAgentHandler(&curBorderAgentOrErrorMsg.mBorderAgent, nullptr);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

exit:
    if (socket >= 0)
    {
        mdns_socket_close(socket);
    }

    return error;
}

static inline std::string ToString(const mdns_string_t &aString)
{
    return std::string(aString.str, aString.length);
}

static inline ByteArray ToByteArray(const mdns_string_t &aString)
{
    return ByteArray{aString.str, aString.str + aString.length};
}

/**
 * This is the callback per mDNS record.
 *
 * It may be called multiple times for a single mDNS response.
 *
 */
static int HandleRecord(const struct sockaddr *from,
                        mdns_entry_type_t      entry,
                        uint16_t               type,
                        uint16_t               rclass,
                        uint32_t               ttl,
                        const void *           data,
                        size_t                 size,
                        size_t                 offset,
                        size_t                 length,
                        void *                 border_agent)
{
    Error                   error = Error::kNone;
    struct sockaddr_storage fromAddrStorage;
    Address                 fromAddr;
    std::string             fromAddrStr;
    std::string             entryType;
    char                    nameBuffer[256];

    BorderAgentOrErrorMsg &borderAgentOrErrorMsg = *reinterpret_cast<BorderAgentOrErrorMsg *>(border_agent);
    BorderAgent &          borderAgent           = borderAgentOrErrorMsg.mBorderAgent;
    std::string &          errorMsg              = borderAgentOrErrorMsg.mErrorMsg;

    (void)rclass;
    (void)ttl;

    *reinterpret_cast<struct sockaddr *>(&fromAddrStorage) = *from;
    if (fromAddr.Set(fromAddrStorage) != Error::kNone || fromAddr.ToString(fromAddrStr) != Error::kNone)
    {
        ExitNow(errorMsg = "invalid source address of mDNS response");
    }

    entryType = (entry == MDNS_ENTRYTYPE_ANSWER) ? "answer"
                                                 : ((entry == MDNS_ENTRYTYPE_AUTHORITY) ? "authority" : "additional");
    if (type == MDNS_RECORDTYPE_PTR)
    {
        mdns_string_t nameStr = mdns_record_parse_ptr(data, size, offset, length, nameBuffer, sizeof(nameBuffer));
        (void)nameStr;
        // TODO(wgtdkp): add debug logging.
    }
    else if (type == MDNS_RECORDTYPE_SRV)
    {
        mdns_record_srv_t server = mdns_record_parse_srv(data, size, offset, length, nameBuffer, sizeof(nameBuffer));

        borderAgent.mPort = server.port;
        borderAgent.mPresentFlags |= BorderAgent::kPortBit;
    }
    else if (type == MDNS_RECORDTYPE_A)
    {
        struct sockaddr_storage addrStorage;
        struct sockaddr_in      addr;
        std::string             addrStr;

        mdns_record_parse_a(data, size, offset, length, &addr);

        *reinterpret_cast<struct sockaddr_in *>(&addrStorage) = addr;
        if (fromAddr.Set(addrStorage) != Error::kNone || fromAddr.ToString(addrStr) != Error::kNone)
        {
            ExitNow(errorMsg = "invalid IPv4 address in A record");
        }

        // We prefer AAAA (IPv6) address than A (IPv4) address.
        if (!(borderAgent.mPresentFlags & BorderAgent::kAddrBit))
        {
            borderAgent.mAddr = addrStr;
            borderAgent.mPresentFlags |= BorderAgent::kAddrBit;
        }
    }
    else if (type == MDNS_RECORDTYPE_AAAA)
    {
        struct sockaddr_storage addrStorage;
        struct sockaddr_in6     addr;
        std::string             addrStr;

        mdns_record_parse_aaaa(data, size, offset, length, &addr);

        *reinterpret_cast<struct sockaddr_in6 *>(&addrStorage) = addr;
        if (fromAddr.Set(addrStorage) != Error::kNone || fromAddr.ToString(addrStr) != Error::kNone)
        {
            ExitNow(errorMsg = "invalid IPv6 address in AAAA record");
        }

        borderAgent.mAddr = addrStr;
        borderAgent.mPresentFlags |= BorderAgent::kAddrBit;
    }
    else if (type == MDNS_RECORDTYPE_TXT)
    {
        mdns_record_txt_t txtBuffer[128];
        size_t            parsed;

        parsed =
            mdns_record_parse_txt(data, size, offset, length, txtBuffer, sizeof(txtBuffer) / sizeof(mdns_record_txt_t));

        for (size_t i = 0; i < parsed; ++i)
        {
            auto key         = ToString(txtBuffer[i].key);
            auto value       = ToString(txtBuffer[i].value);
            auto binaryValue = ToByteArray(txtBuffer[i].value);

            if (key == "rv")
            {
                VerifyOrExit(value == "1", errorMsg = fmt::format("value of TXT Key 'rv' is {} but not 1", value));
            }
            else if (key == "tv")
            {
                borderAgent.mThreadVersion = value;
                borderAgent.mPresentFlags |= BorderAgent::kThreadVersionBit;
            }
            else if (key == "sb")
            {
                auto &bitmap = binaryValue;
                if (bitmap.size() != 4)
                {
                    ExitNow(errorMsg = fmt::format("value of TXT Key 'sb' is invalid: value={}", utils::Hex(bitmap)));
                }

                borderAgent.mState.mConnectionMode = (bitmap[0] >> 5);
                borderAgent.mState.mThreadIfStatus = (bitmap[0] << 3) >> 6;
                borderAgent.mState.mAvailability   = (bitmap[0] << 5) >> 6;
                borderAgent.mState.mBbrIsActive    = (bitmap[0] << 7) >> 7;
                borderAgent.mState.mBbrIsPrimary   = (bitmap[1] >> 7);
                borderAgent.mPresentFlags |= BorderAgent::kStateBit;
            }
            else if (key == "nn")
            {
                borderAgent.mNetworkName = value;
                borderAgent.mPresentFlags |= BorderAgent::kNetworkNameBit;
            }
            else if (key == "xp")
            {
                auto &extendPanId = binaryValue;
                if (extendPanId.size() != 8)
                {
                    ExitNow(errorMsg =
                                fmt::format("value of TXT Key 'xp' is invalid: value={}", utils::Hex(extendPanId)));
                }
                else
                {
                    borderAgent.mExtendedPanId = utils::Decode<uint64_t>(extendPanId);
                    borderAgent.mPresentFlags |= BorderAgent::kExtendedPanIdBit;
                }
            }
            else if (key == "vn")
            {
                borderAgent.mVendorName = value;
                borderAgent.mPresentFlags |= BorderAgent::kVendorNameBit;
            }
            else if (key == "mn")
            {
                borderAgent.mModelName = value;
                borderAgent.mPresentFlags |= BorderAgent::kModelNameBit;
            }
            else if (key == "at")
            {
                auto &activeTimestamp = binaryValue;
                if (activeTimestamp.size() != 8)
                {
                    ExitNow(errorMsg =
                                fmt::format("value of TXT Key 'at' is invalid: value={}", utils::Hex(activeTimestamp)));
                }
                else
                {
                    borderAgent.mActiveTimestamp = Timestamp::Decode(utils::Decode<uint64_t>(activeTimestamp));
                    borderAgent.mPresentFlags |= BorderAgent::kActiveTimestampBit;
                }
            }
            else if (key == "pt")
            {
                auto &partitionId = binaryValue;
                if (partitionId.size() != 4)
                {
                    ExitNow(errorMsg =
                                fmt::format("value of TXT Key 'pt' is invalid: value={}", utils::Hex(partitionId)));
                }
                else
                {
                    borderAgent.mPartitionId = utils::Decode<uint32_t>(partitionId);
                    borderAgent.mPresentFlags |= BorderAgent::kPartitionIdBit;
                }
            }
            else if (key == "vd")
            {
                borderAgent.mVendorData = value;
                borderAgent.mPresentFlags |= BorderAgent::kVendorDataBit;
            }
            else if (key == "vo")
            {
                auto &oui = binaryValue;
                if (oui.size() != 3)
                {
                    ExitNow(errorMsg = fmt::format("value of TXT Key 'vo' is invalid: value={}", utils::Hex(oui)));
                }
                else
                {
                    borderAgent.mVendorOui = oui;
                    borderAgent.mPresentFlags |= BorderAgent::kVendorOuiBit;
                }
            }
            else if (key == "dn")
            {
                borderAgent.mDomainName = value;
                borderAgent.mPresentFlags |= BorderAgent::kDomainNameBit;
            }
            else if (key == "sq")
            {
                auto &bbrSeqNum = binaryValue;
                if (bbrSeqNum.size() != 1)
                {
                    ExitNow(errorMsg =
                                fmt::format("[mDNS] value of TXT Key 'sq' is invalid: {}", utils::Hex(bbrSeqNum)));
                }
                else
                {
                    borderAgent.mBbrSeqNumber = utils::Decode<uint8_t>(bbrSeqNum);
                    borderAgent.mPresentFlags |= BorderAgent::kBbrSeqNumberBit;
                }
            }
            else if (key == "bb")
            {
                auto &bbrPort = binaryValue;
                if (bbrPort.size() != 2)
                {
                    ExitNow(errorMsg = fmt::format("[mDNS] value of TXT Key 'bb' is invalid: {}", utils::Hex(bbrPort)));
                }
                else
                {
                    borderAgent.mBbrPort = utils::Decode<uint16_t>(bbrPort);
                    borderAgent.mPresentFlags |= BorderAgent::kBbrPortBit;
                }
            }
            else
            {
                // TODO(wgtdkp): add debug logging.
            }
        }
    }
    else
    {
        // TODO(wgtdkp): add debug logging.
    }

exit:
    return error == Error::kNone ? 0 : -1;
}

} // namespace commissioner

} // namespace ot
