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

#include <gtest/gtest.h>

namespace ot {

namespace commissioner {

TEST(AddressTest, AddressFromString_Ipv4LoopbackAddress)
{
    Address addr;

    EXPECT_EQ(addr.Set("127.0.0.1"), ErrorCode::kNone);
    EXPECT_TRUE(addr.IsValid());
    EXPECT_TRUE(addr.IsIpv4());
    EXPECT_FALSE(addr.IsRloc16());

    EXPECT_EQ(addr.ToString(), "127.0.0.1");
}

TEST(AddressTest, AddressFromString_Ipv6LoopbackAddress)
{
    Address addr;

    EXPECT_EQ(addr.Set("::1"), ErrorCode::kNone);
    EXPECT_TRUE(addr.IsValid());
    EXPECT_TRUE(addr.IsIpv6());
    EXPECT_FALSE(addr.IsRloc16());

    EXPECT_EQ(addr.ToString(), "::1");
}

TEST(AddressTest, AddressFromString_Ipv6Prefix)
{
    Address addr;

    const static std::string kPrefix = "2001:db8:3c4d:15::";
    EXPECT_EQ(addr.Set(kPrefix), ErrorCode::kNone);
    EXPECT_TRUE(addr.IsValid());
    EXPECT_TRUE(addr.IsIpv6());
    EXPECT_FALSE(addr.IsRloc16());

    EXPECT_EQ(addr.ToString(), kPrefix);
}

TEST(AddressTest, AddressFromSockaddr_Ipv4SocketAddress)
{
    Address     addr;
    sockaddr_in sockaddr;
    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin_family      = AF_INET;
    sockaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sockaddr.sin_port        = htons(5684);

    EXPECT_EQ(addr.Set(*reinterpret_cast<sockaddr_storage *>(&sockaddr)), ErrorCode::kNone);
    EXPECT_TRUE(addr.IsValid());
    EXPECT_TRUE(addr.IsIpv4());
    EXPECT_FALSE(addr.IsRloc16());

    EXPECT_EQ(addr.ToString(), "127.0.0.1");
}

TEST(AddressTest, AddressFromSockaddr_Ipv6SocketAddress)
{
    Address      addr;
    sockaddr_in6 sockaddr;
    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin6_family = AF_INET6;
    sockaddr.sin6_addr   = in6addr_loopback;
    sockaddr.sin6_port   = htons(5684);

    EXPECT_EQ(addr.Set(*reinterpret_cast<sockaddr_storage *>(&sockaddr)), ErrorCode::kNone);
    EXPECT_TRUE(addr.IsValid());
    EXPECT_TRUE(addr.IsIpv6());
    EXPECT_FALSE(addr.IsRloc16());

    EXPECT_EQ(addr.ToString(), "::1");
}

TEST(AddressTest, AddressNegativeTests_InvalidAddressToString)
{
    Address addr;

    EXPECT_EQ(addr.Set(addr.ToString()), ErrorCode::kInvalidArgs);
    EXPECT_FALSE(addr.IsValid());
    EXPECT_FALSE(addr.IsIpv4());
    EXPECT_FALSE(addr.IsIpv6());
}

TEST(AddressTest, AddressNegativeTests_SetInvalidRawAddress)
{
    Address addr;

    EXPECT_EQ(addr.Set(ByteArray{0, 0, 0}), ErrorCode::kInvalidArgs);
    EXPECT_FALSE(addr.IsValid());
    EXPECT_FALSE(addr.IsIpv4());
    EXPECT_FALSE(addr.IsIpv6());
}

TEST(AddressTest, AddressNegativeTests_InvalidIpv4Address)
{
    Address addr;

    EXPECT_EQ(addr.Set("127.0.0.1.2"), ErrorCode::kInvalidArgs);
    EXPECT_FALSE(addr.IsValid());
    EXPECT_FALSE(addr.IsIpv4());
    EXPECT_FALSE(addr.IsIpv6());
}

TEST(AddressTest, AddressNegativeTests_InvalidIpv6Address)
{
    Address addr;

    EXPECT_EQ(addr.Set("::1::2"), ErrorCode::kInvalidArgs);
    EXPECT_FALSE(addr.IsValid());
    EXPECT_FALSE(addr.IsIpv4());
    EXPECT_FALSE(addr.IsIpv6());
}

TEST(AddressTest, AddressFromString_Rloc16Address)
{
    Address addr;

    EXPECT_EQ(addr.Set("3039"), ErrorCode::kNone);
    EXPECT_TRUE(addr.IsValid());
    EXPECT_TRUE(addr.IsRloc16());

    EXPECT_EQ(addr.ToString(), "3039");
}

TEST(AddressTest, AddressNegativeTests_InvalidRloc16Address)
{
    Address addr;

    EXPECT_EQ(addr.Set("not-a-valid-hex-string"), ErrorCode::kInvalidArgs);
    EXPECT_FALSE(addr.IsValid());
}

TEST(AddressTest, AddressNegativeTests_Rloc16AddressOutOfRange)
{
    Address addr;

    // A 3-byte hex string is not a valid rloc16.
    EXPECT_EQ(addr.Set("10000"), ErrorCode::kInvalidArgs);
    EXPECT_FALSE(addr.IsValid());
}

TEST(AddressTest, AddressFromString_Rloc16AddressWithPrefix)
{
    Address addr;

    EXPECT_EQ(addr.Set("0x3039"), ErrorCode::kNone);
    EXPECT_TRUE(addr.IsValid());
    EXPECT_TRUE(addr.IsRloc16());
    EXPECT_EQ(addr.ToString(), "3039");
}

TEST(AddressTest, AddressFromString_Rloc16AddressWithCapitalXPrefix)
{
    Address addr;

    EXPECT_EQ(addr.Set("0XFC00"), ErrorCode::kNone);
    EXPECT_TRUE(addr.IsValid());
    EXPECT_TRUE(addr.IsRloc16());
    EXPECT_EQ(addr.ToString(), "fc00");
}

TEST(AddressTest, AddressFromUint16_Rloc16Address)
{
    Address  addr;
    uint16_t rloc16 = 0x3039;

    EXPECT_EQ(addr.Set(rloc16), ErrorCode::kNone);
    EXPECT_TRUE(addr.IsValid());
    EXPECT_TRUE(addr.IsRloc16());
    EXPECT_EQ(addr.GetRloc16(), rloc16);
    EXPECT_EQ(addr.ToString(), "3039");
}

TEST(AddressTest, AddressNegativeTests_IsMulticast)
{
    Address addr;

    EXPECT_EQ(addr.Set("3039"), ErrorCode::kNone);
    EXPECT_FALSE(addr.IsMulticast());
}

} // namespace commissioner

} // namespace ot
