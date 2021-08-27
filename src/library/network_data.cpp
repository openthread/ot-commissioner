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
 *   The file implements Thread Network Data.
 */

#include <commissioner/network_data.hpp>

#include <iomanip>
#include <sstream>

#include "common/address.hpp"
#include "common/error_macros.hpp"
#include "common/time.hpp"
#include "common/utils.hpp"

namespace ot {

namespace commissioner {

Timestamp Timestamp::Cur()
{
    Timestamp cur;
    auto      now = NowSinceEpoch<std::chrono::microseconds>().count();
    cur.mSeconds  = now / 1000000;
    cur.mTicks    = (now % 1000000) * (1 << 15) / 1000000;

    // Not an authoritive source
    cur.mU = 0;

    return cur;
}

Timestamp Timestamp::Decode(uint64_t aValue)
{
    Timestamp ret;
    ret.mSeconds = aValue >> 16;
    ret.mTicks   = (aValue & 0xFFFF) >> 1;
    ret.mU       = aValue & 0x01;
    return ret;
}

uint64_t Timestamp::Encode() const
{
    return (mSeconds << 16) | (mTicks << 1) | mU;
}

Error Ipv6PrefixFromString(ByteArray &aPrefix, const std::string &aStr)
{
    Error   error;
    size_t  slash;
    size_t  prefixLength;
    Address addr;

    slash = aStr.find('/');
    VerifyOrExit(slash != std::string::npos, error = ERROR_INVALID_ARGS("{} is not a valid IPv6 prefix", aStr));

    {
        char *endPtr = nullptr;
        prefixLength = strtoull(&aStr[slash + 1], &endPtr, 0);
        VerifyOrExit(endPtr != nullptr && endPtr > &aStr[slash + 1],
                     error = ERROR_INVALID_ARGS("{} is not a valid IPv6 prefix", aStr));
        VerifyOrExit(prefixLength <= 128, error = ERROR_INVALID_ARGS("{} is not a valid IPv6 prefix", aStr));
        prefixLength /= 8;
    }

    SuccessOrExit(error = addr.Set(aStr.substr(0, slash)));
    VerifyOrExit(addr.IsIpv6(), error = ERROR_INVALID_ARGS("{} is not a valid IPv6 prefix", aStr));

    aPrefix = addr.GetRaw();
    aPrefix.resize(prefixLength);

exit:
    return error;
}

std::string Ipv6PrefixToString(ByteArray aPrefix)
{
    VerifyOrDie(aPrefix.size() <= 16);

    auto prefixLength = aPrefix.size() * 8;
    aPrefix.resize(16);

    Address addr;
    SuccessOrDie(addr.Set(aPrefix));

    return addr.ToString() + "/" + std::to_string(prefixLength);
}

XpanId::XpanId(uint64_t val)
    : mValue(val)
{
}

XpanId::XpanId()
    : XpanId(kEmptyXpanId)
{
}

std::string XpanId::str() const
{
    return *this;
}

bool XpanId::operator==(const XpanId &aOther) const
{
    return mValue == aOther.mValue;
}

bool XpanId::operator!=(const uint64_t aOther) const
{
    return !operator==(aOther);
}

bool XpanId::operator<(const XpanId aOther) const
{
    return mValue < aOther.mValue;
}

XpanId::operator std::string() const
{
    std::ostringstream value;
    value << std::uppercase << std::hex << std::setw(sizeof(mValue) * 2) << std::setfill('0') << mValue;
    return value.str();
}

/**
 * Converts hex string to the corresponding integer type.
 * @attention Makes no validity checks.
 */
Error XpanId::FromHex(const std::string &aInput)
{
    mValue = 0;

    std::string input = aInput;
    if (utils::ToLower(input.substr(0, 2)) == "0x")
    {
        input = input.substr(2);
    }
    if (input.empty() || input.length() > 16)
        return ERROR_BAD_FORMAT("{}: wrong XPAN ID string length", input.length());
    for (auto c : input)
    {
        if (!std::isxdigit(c))
        {
            return ERROR_BAD_FORMAT("{}: not a hex string", input);
        }
    }

    std::istringstream is(input);
    is >> std::hex >> mValue;
    return ERROR_NONE;
}

PanId::PanId(uint16_t aValue)
    : mValue(aValue)
{
}

PanId::PanId()
    : PanId(kEmptyPanId)
{
}

PanId &PanId::operator=(uint16_t aValue)
{
    mValue = aValue;
    return *this;
}

PanId::operator uint16_t() const
{
    return mValue;
}

PanId::operator std::string() const
{
    std::ostringstream value;
    value << "0x" << std::uppercase << std::hex << std::setw(sizeof(mValue) * 2) << std::setfill('0') << mValue;
    return value.str();
}

Error PanId::FromHex(const std::string &aInput)
{
    mValue = 0;

    std::string input = aInput;
    if (utils::ToLower(input.substr(0, 2)) == "0x")
    {
        input = input.substr(2);
    }
    if (input.empty() || input.length() > 4)
        return ERROR_BAD_FORMAT("{}: wrong PAN ID string length", input.length());
    for (auto c : input)
    {
        if (!std::isxdigit(c))
        {
            return ERROR_BAD_FORMAT("{}: not a hex string", input);
        }
    }

    std::istringstream is(input);
    is >> std::hex >> mValue;
    return ERROR_NONE;
}

ActiveOperationalDataset::ActiveOperationalDataset()
    : mActiveTimestamp(Timestamp::Cur())
    , mPresentFlags(kActiveTimestampBit)
{
}

PendingOperationalDataset::PendingOperationalDataset()
    : mPendingTimestamp(mActiveTimestamp)
{
    mPresentFlags |= kPendingTimestampBit;
}

} // namespace commissioner

} // namespace ot
