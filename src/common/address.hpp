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

    bool IsIpv4() const { return mBytes.size() == kIpv4Size; }

    bool IsIpv6() const { return mBytes.size() == kIpv6Size; }

    bool IsRloc16() const { return mBytes.size() == kRloc16Size;  }

    bool IsMulticast() const;

    Error Set(const ByteArray &aRawAddr);

    Error Set(const std::string &aIp);

    Error Set(const sockaddr_storage &aSockAddr);

    Error Set(uint16_t aRloc16);

    const ByteArray &GetRaw() const;

    uint16_t GetRloc16() const;

    /**
     * Returns the string representation of the IP address.
     *
     * @return "INVALID_ADDR" if the address is not valid.
     *
     */
    std::string ToString() const;

    // Invalid address string is not acceptable.
    // For only unittests.
    static Address FromString(const std::string &aAddr);

private:
    static constexpr size_t  kIpv4Size        = 4;
    static constexpr size_t  kIpv6Size        = 16;
    static constexpr size_t  kRloc16Size      = 2;
    static constexpr uint8_t kMulticastPrefix = 0xFF;

    ByteArray mBytes;
};

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_COMMON_ADDRESS_HPP_
