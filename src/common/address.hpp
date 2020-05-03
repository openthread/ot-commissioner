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

#ifndef OT_COMM_COMMON_ADDRESS_HPP_
#define OT_COMM_COMMON_ADDRESS_HPP_

#include <string>

#include <sys/socket.h>

#include <commissioner/defines.hpp>
#include <commissioner/error.hpp>

namespace ot {

namespace commissioner {

class Address
{
public:
    Address() = default;

    bool operator==(const Address &aOther) const { return mBytes == aOther.mBytes; }

    bool operator!=(const Address &aOther) const { return !(*this == aOther); }

    bool operator<(const Address &aOther) const { return mBytes < aOther.mBytes; }

    bool IsValid() const { return !mBytes.empty(); }

    bool IsIpv4() const { return mBytes.size() == 4; }

    bool IsIpv6() const { return mBytes.size() == 16; }

    bool IsMulticast() const { return IsValid() && mBytes[0] == 0xFF; }

    Error Set(const ByteArray &aRawAddr)
    {
        if (aRawAddr.size() != 4 && aRawAddr.size() != 16)
        {
            return Error::kInvalidArgs;
        }
        mBytes = aRawAddr;
        return Error::kNone;
    }

    Error Set(const std::string &aIp);

    Error Set(const sockaddr_storage &aSockAddr);

    const ByteArray &GetRaw() const { return mBytes; }

    Error ToString(std::string &aAddr) const;

    // Invalid address string is not acceptable.
    static Address FromString(const std::string &aAddr);

private:
    ByteArray mBytes;
};

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_COMMON_ADDRESS_HPP_
