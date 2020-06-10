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

#include "common/address.hpp"

#include <arpa/inet.h>

#include "common/error_macros.hpp"
#include "common/utils.hpp"

namespace ot {

namespace commissioner {

Error Address::Set(const ByteArray &aRawAddr)
{
    Error error;

    if (aRawAddr.size() != kIpv4Size && aRawAddr.size() != kIpv6Size)
    {
        ExitNow(error = ERROR_INVALID_ARGS("IP address must has length of 4(IPv4) or 16(IPv6)"));
    }

    mBytes = aRawAddr;

exit:
    return error;
}

Error Address::Set(const std::string &aIp)
{
    Error     error;
    ByteArray bytes;

    bytes.resize(kIpv4Size);
    if (inet_pton(AF_INET, aIp.c_str(), bytes.data()) != 1)
    {
        bytes.resize(kIpv6Size);
        if (inet_pton(AF_INET6, aIp.c_str(), bytes.data()) != 1)
        {
            ExitNow(error = ERROR_INVALID_ARGS("{} is not a valid IP address", aIp));
        }
    }

    mBytes = bytes;

exit:
    return error;
}

Error Address::Set(const sockaddr_storage &aSockAddr)
{
    Error error;

    VerifyOrExit(aSockAddr.ss_family == AF_INET || aSockAddr.ss_family == AF_INET6,
                 error = ERROR_INVALID_ARGS("only AF_INET and AF_INET6 address families are supported"));

    if (aSockAddr.ss_family == AF_INET)
    {
        auto &addr4 = *reinterpret_cast<const sockaddr_in *>(&aSockAddr);
        auto  raw   = reinterpret_cast<const uint8_t *>(&addr4.sin_addr);
        mBytes.assign(raw, raw + sizeof(addr4.sin_addr));
    }
    else
    {
        auto &addr6 = *reinterpret_cast<const sockaddr_in6 *>(&aSockAddr);
        auto  raw   = reinterpret_cast<const uint8_t *>(&addr6.sin6_addr);
        mBytes.assign(raw, raw + sizeof(addr6.sin6_addr));
    }

exit:
    return error;
}

std::string Address::ToString() const
{
    static const char *kInvalidAddr = "INVALID_ADDR";
    std::string        result;

    if (mBytes.size() == kIpv4Size)
    {
        char ipv4[INET_ADDRSTRLEN];
        auto ret = inet_ntop(AF_INET, mBytes.data(), ipv4, sizeof(ipv4));
        VerifyOrExit(ret != nullptr, result = kInvalidAddr);
        result = ret;
    }
    else if (mBytes.size() == kIpv6Size)
    {
        char ipv6[INET6_ADDRSTRLEN];
        auto ret = inet_ntop(AF_INET6, mBytes.data(), ipv6, sizeof(ipv6));
        VerifyOrExit(ret != nullptr, result = kInvalidAddr);
        result = ret;
    }
    else
    {
        ExitNow(result = kInvalidAddr);
    }

exit:
    return result;
}

Address Address::FromString(const std::string &aAddr)
{
    Address ret;

    SuccessOrDie(ret.Set(aAddr));
    return ret;
}

} // namespace commissioner

} // namespace ot
