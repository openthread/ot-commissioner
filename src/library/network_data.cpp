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
 *   The file implements Thread Network Data.
 */

#include <commissioner/network_data.hpp>

#include "time.hpp"
#include <address.hpp>
#include <utils.hpp>

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
    Error   error = Error::kNone;
    size_t  slash;
    size_t  prefixLength;
    Address addr;

    slash = aStr.find('/');
    VerifyOrExit(slash != std::string::npos, error = Error::kInvalidArgs);

    {
        char *endPtr = nullptr;
        prefixLength = strtoull(&aStr[slash + 1], &endPtr, 0);
        VerifyOrExit(endPtr != nullptr && endPtr > &aStr[slash + 1], error = Error::kInvalidArgs);
        VerifyOrExit(prefixLength <= 128, error = Error::kInvalidArgs);
        prefixLength /= 8;
    }

    SuccessOrExit(error = addr.Set(aStr.substr(0, slash)));
    VerifyOrExit(addr.IsIpv6(), error = Error::kInvalidArgs);

    error   = Error::kNone;
    aPrefix = addr.GetRaw();
    aPrefix.resize(prefixLength);

exit:
    return error;
}

std::string Ipv6PrefixToString(ByteArray aPrefix)
{
    ASSERT(aPrefix.size() <= 16);

    auto prefixLength = aPrefix.size() * 8;
    aPrefix.resize(16);

    Address addr;
    ASSERT(addr.Set(aPrefix) == Error::kNone);

    std::string ret;
    ASSERT(addr.ToString(ret) == Error::kNone);
    return ret + "/" + std::to_string(prefixLength);
}

} // namespace commissioner

} // namespace ot
