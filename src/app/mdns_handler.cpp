/*
 *    Copyright (c) 2021, The OpenThread Commissioner Authors.
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

#include "mdns_handler.hpp"

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <string>

#include <netinet/in.h>
#include <sys/socket.h>

#include "border_agent.hpp"
#include "commissioner/defines.hpp"
#include "commissioner/error.hpp"
#include "commissioner/network_data.hpp"
#include "common/address.hpp"
#include "common/error_macros.hpp"
#include "common/utils.hpp"
#include "mdns/mdns.h"

namespace ot {

namespace commissioner {

static inline std::string ToString(const mdns_string_t &aString) { return std::string(aString.str, aString.length); }

static inline ByteArray ToByteArray(const mdns_string_t &aString)
{
    return ByteArray{aString.str, aString.str + aString.length};
}

int HandleRecord(const struct sockaddr *aFrom,
                 mdns_entry_type_t      aEntry,
                 uint16_t               aType,
                 uint16_t               aRclass,
                 uint32_t               aTtl,
                 const void            *aData,
                 size_t                 aSize,
                 size_t                 aOffset,
                 size_t                 aLength,
                 void                  *aBorderAgent)
{
    struct sockaddr_storage fromAddrStorage;
    Address                 fromAddr;
    std::string             fromAddrStr;
    std::string             entryType;
    char                    nameBuffer[256];

    BorderAgentOrErrorMsg &borderAgentOrErrorMsg = *reinterpret_cast<BorderAgentOrErrorMsg *>(aBorderAgent);
    BorderAgent           &borderAgent           = borderAgentOrErrorMsg.mBorderAgent;
    Error                 &error                 = borderAgentOrErrorMsg.mError;

    (void)aRclass;
    (void)aTtl;

    *reinterpret_cast<struct sockaddr *>(&fromAddrStorage) = *aFrom;
    if (fromAddr.Set(fromAddrStorage) != ErrorCode::kNone)
    {
        ExitNow(error = ERROR_BAD_FORMAT("invalid source address of mDNS response"));
    }

    fromAddrStr = fromAddr.ToString();

    entryType = (aEntry == MDNS_ENTRYTYPE_ANSWER) ? "answer"
                                                  : ((aEntry == MDNS_ENTRYTYPE_AUTHORITY) ? "authority" : "additional");
    if (aType == MDNS_RECORDTYPE_PTR)
    {
        mdns_string_t nameStr = mdns_record_parse_ptr(aData, aSize, aOffset, aLength, nameBuffer, sizeof(nameBuffer));
        borderAgent.mServiceName  = std::string(nameStr.str, nameStr.length);
        borderAgent.mPresentFlags = BorderAgent::kServiceNameBit;
        // TODO(wgtdkp): add debug logging.
    }
    else if (aType == MDNS_RECORDTYPE_SRV)
    {
        mdns_record_srv_t server =
            mdns_record_parse_srv(aData, aSize, aOffset, aLength, nameBuffer, sizeof(nameBuffer));

        borderAgent.mPort = server.port;
        borderAgent.mPresentFlags |= BorderAgent::kPortBit;
    }
    else if (aType == MDNS_RECORDTYPE_A)
    {
        struct sockaddr_storage addrStorage;
        struct sockaddr_in      addr;
        std::string             addrStr;

        mdns_record_parse_a(aData, aSize, aOffset, aLength, &addr);

        *reinterpret_cast<struct sockaddr_in *>(&addrStorage) = addr;
        if (fromAddr.Set(addrStorage) != ErrorCode::kNone)
        {
            ExitNow(error = ERROR_BAD_FORMAT("invalid IPv4 address in A record from {}", fromAddrStr));
        }

        addrStr = fromAddr.ToString();

        // We prefer AAAA (IPv6) address than A (IPv4) address.
        if (!(borderAgent.mPresentFlags & BorderAgent::kAddrBit))
        {
            borderAgent.mAddr = addrStr;
            borderAgent.mPresentFlags |= BorderAgent::kAddrBit;
        }
    }
    else if (aType == MDNS_RECORDTYPE_AAAA)
    {
        struct sockaddr_storage addrStorage;
        struct sockaddr_in6     addr;
        std::string             addrStr;

        mdns_record_parse_aaaa(aData, aSize, aOffset, aLength, &addr);

        *reinterpret_cast<struct sockaddr_in6 *>(&addrStorage) = addr;
        if (fromAddr.Set(addrStorage) != ErrorCode::kNone)
        {
            ExitNow(error = ERROR_BAD_FORMAT("invalid IPv6 address in AAAA record from {}", fromAddrStr));
        }

        addrStr = fromAddr.ToString();

        borderAgent.mAddr = addrStr;
        borderAgent.mPresentFlags |= BorderAgent::kAddrBit;
    }
    else if (aType == MDNS_RECORDTYPE_TXT)
    {
        mdns_record_txt_t txtBuffer[128];
        size_t            parsed;

        parsed = mdns_record_parse_txt(aData, aSize, aOffset, aLength, txtBuffer,
                                       sizeof(txtBuffer) / sizeof(mdns_record_txt_t));

        for (size_t i = 0; i < parsed; ++i)
        {
            auto key         = ToString(txtBuffer[i].key);
            auto value       = ToString(txtBuffer[i].value);
            auto binaryValue = ToByteArray(txtBuffer[i].value);

            if (key == "rv")
            {
                VerifyOrExit(value == "1", error = ERROR_BAD_FORMAT("value of TXT Key 'rv' is {} but not 1 from {}",
                                                                    value, fromAddrStr));
            }
            else if (key == "dd")
            {
                auto &discriminator = binaryValue;

                if (discriminator.size() != kExtendedAddrLength)
                {
                    ExitNow(error = ERROR_BAD_FORMAT("value of TXT Key 'dd' is invalid: value={}",
                                                     utils::Hex(discriminator)));
                }
                else
                {
                    borderAgent.mDiscriminator = discriminator;
                    borderAgent.mPresentFlags |= BorderAgent::kDiscriminatorBit;
                }
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
                    ExitNow(error = ERROR_BAD_FORMAT("value of TXT Key 'sb' is invalid: value={} from {}",
                                                     utils::Hex(bitmap), fromAddrStr));
                }

                borderAgent.mState.mConnectionMode = (bitmap[3] & 0x07);
                borderAgent.mState.mThreadIfStatus = (bitmap[3] & 0x18) >> 3;
                borderAgent.mState.mAvailability   = (bitmap[3] & 0x60) >> 5;
                borderAgent.mState.mBbrIsActive    = (bitmap[3] & 0x80) >> 7;
                borderAgent.mState.mBbrIsPrimary   = (bitmap[2] & 0x01);
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
                    ExitNow(error = ERROR_BAD_FORMAT("value of TXT Key 'xp' is invalid: value={} from {}",
                                                     utils::Hex(extendPanId), fromAddrStr));
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
                    ExitNow(error = ERROR_BAD_FORMAT("value of TXT Key 'at' is invalid: value={} from {}",
                                                     utils::Hex(activeTimestamp), fromAddrStr));
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
                    ExitNow(error = ERROR_BAD_FORMAT("value of TXT Key 'pt' is invalid: value={} from {}",
                                                     utils::Hex(partitionId), fromAddrStr));
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
                    ExitNow(error = ERROR_BAD_FORMAT("value of TXT Key 'vo' is invalid: value={} from {}",
                                                     utils::Hex(oui), fromAddrStr));
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
                    ExitNow(error = ERROR_BAD_FORMAT("[mDNS] value of TXT Key 'sq' is invalid: {} from {}",
                                                     utils::Hex(bbrSeqNum), fromAddrStr));
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
                    ExitNow(error = ERROR_BAD_FORMAT("[mDNS] value of TXT Key 'bb' is invalid: {} from {}",
                                                     utils::Hex(bbrPort), fromAddrStr));
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

    if (borderAgent.mPresentFlags != 0)
    {
        // Actualize Timestamp
        borderAgent.mUpdateTimestamp.mTime = time(nullptr);
        borderAgent.mPresentFlags |= BorderAgent::kUpdateTimestampBit;
    }
exit:
    return 0;
}

} // namespace commissioner

} // namespace ot
