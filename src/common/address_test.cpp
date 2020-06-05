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
 *   This file defines test cases for Address class.
 */

#include "common/address.hpp"

#include <memory.h>
#include <netinet/in.h>

#include <catch2/catch.hpp>

namespace ot {

namespace commissioner {

TEST_CASE("address-from-string", "[address]")
{
    Address addr;

    SECTION("IPv4 loopback address")
    {
        REQUIRE(addr.Set("127.0.0.1") == ErrorCode::kNone);
        REQUIRE(addr.IsValid());
        REQUIRE(addr.IsIpv4());

        REQUIRE(addr.ToString() == "127.0.0.1");
    }

    SECTION("IPv6 loopback address")
    {
        REQUIRE(addr.Set("::1") == ErrorCode::kNone);
        REQUIRE(addr.IsValid());
        REQUIRE(addr.IsIpv6());

        REQUIRE(addr.ToString() == "::1");
    }

    SECTION("IPv6 prefix")
    {
        const static std::string kPrefix = "2001:db8:3c4d:15::";
        REQUIRE(addr.Set(kPrefix) == ErrorCode::kNone);
        REQUIRE(addr.IsValid());
        REQUIRE(addr.IsIpv6());

        REQUIRE(addr.ToString() == kPrefix);
    }

    SECTION("IPv4 FromString")
    {
        addr = Address::FromString("127.0.0.1");
        REQUIRE(addr.IsValid());
        REQUIRE(addr.IsIpv4());

        REQUIRE(addr.ToString() == "127.0.0.1");
    }

    SECTION("IPv6 FromString")
    {
        addr = Address::FromString("::1");
        REQUIRE(addr.IsValid());
        REQUIRE(addr.IsIpv6());

        REQUIRE(addr.ToString() == "::1");
    }
}

TEST_CASE("address-from-sockaddr", "[address]")
{
    Address addr;

    SECTION("ipv4 socket address")
    {
        sockaddr_in sockaddr;
        memset(&sockaddr, 0, sizeof(sockaddr));
        sockaddr.sin_family      = AF_INET;
        sockaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        sockaddr.sin_port        = htons(5684);

        REQUIRE(addr.Set(*reinterpret_cast<sockaddr_storage *>(&sockaddr)) == ErrorCode::kNone);
        REQUIRE(addr.IsValid());
        REQUIRE(addr.IsIpv4());

        REQUIRE(addr.ToString() == "127.0.0.1");
    }

    SECTION("ipv6 socket address")
    {
        sockaddr_in6 sockaddr;
        memset(&sockaddr, 0, sizeof(sockaddr));
        sockaddr.sin6_family = AF_INET6;
        sockaddr.sin6_addr   = in6addr_loopback;
        sockaddr.sin6_port   = htons(5684);

        REQUIRE(addr.Set(*reinterpret_cast<sockaddr_storage *>(&sockaddr)) == ErrorCode::kNone);
        REQUIRE(addr.IsValid());
        REQUIRE(addr.IsIpv6());

        REQUIRE(addr.ToString() == "::1");
    }
}

TEST_CASE("address-negative-tests", "[address]")
{
    Address addr;

    SECTION("invalid ipv4 address should fail")
    {
        REQUIRE(addr.Set("127.0.0.1.2") == ErrorCode::kInvalidArgs);
        REQUIRE(!addr.IsValid());
        REQUIRE(!addr.IsIpv4());
        REQUIRE(!addr.IsIpv6());
    }

    SECTION("invalid ipv6 address should fail")
    {
        REQUIRE(addr.Set("::1::2") == ErrorCode::kInvalidArgs);
        REQUIRE(!addr.IsValid());
        REQUIRE(!addr.IsIpv4());
        REQUIRE(!addr.IsIpv6());
    }
}

} // namespace commissioner

} // namespace ot
