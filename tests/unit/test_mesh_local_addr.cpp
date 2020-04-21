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
 *   This file defines test cases of building Mesh local IPv6 address.
 */

#include <catch2/catch.hpp>

#include <commissioner/commissioner.hpp>

#include "common/utils.hpp"

namespace ot {

namespace commissioner {

TEST_CASE("mesh-local-address-basic", "[mesh-local-addr]")
{
    std::string meshLocalAddr;
    REQUIRE(Commissioner::GetMeshLocalAddr(meshLocalAddr, "fd00::/64", 0xBBCC) == Error::kNone);
    REQUIRE(meshLocalAddr == "fd00::ff:fe00:bbcc");
}

TEST_CASE("mesh-local-address-invalid-args", "[mesh-local-addr]")
{
    std::string meshLocalAddr;

    SECTION("invalid prefix length")
    {
        REQUIRE(Commissioner::GetMeshLocalAddr(meshLocalAddr, "fd00::/63", 0xBBCC) == Error::kInvalidArgs);
    }

    SECTION("prefix length is not 8 bytes")
    {
        REQUIRE(Commissioner::GetMeshLocalAddr(meshLocalAddr, "fd00::/48", 0xBBCC) == Error::kInvalidArgs);
    }

    SECTION("invalid prefix format")
    {
        REQUIRE(Commissioner::GetMeshLocalAddr(meshLocalAddr, "fd00::48", 0xBBCC) == Error::kInvalidArgs);
    }
}

} // namespace commissioner

} // namespace ot
