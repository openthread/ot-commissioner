/*
 *    Copyright (c) 2020, The OpenThread Commissioner Authors.
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
 *   This file defines test cases for utils.
 */

#include "common/utils.hpp"

#include <catch2/catch.hpp>

namespace ot {

namespace commissioner {

TEST_CASE("integer-encoding-decoding", "[utils]")
{
    ByteArray buf;

    buf = utils::Encode<uint8_t>(0xFC);
    REQUIRE(buf == ByteArray{0xFC});
    utils::Encode<uint8_t>(buf, 0xFB);
    REQUIRE(buf == ByteArray{0xFC, 0xFB});

    REQUIRE(utils::Decode<int8_t>(utils::Encode<int8_t>(0xFD)) == static_cast<int8_t>(0xFD));

    buf = utils::Encode<uint32_t>(0x00010203);
    REQUIRE(buf == ByteArray{0x00, 0x01, 0x02, 0x03});
    REQUIRE(utils::Decode<uint32_t>(buf) == 0x00010203);
}

TEST_CASE("hex-encoding-decoing", "[utils]")
{
    ByteArray   buf;
    std::string hexStr;

    hexStr = utils::Hex(ByteArray{0x00, 0x01, 0x02, 0x03});
    REQUIRE(hexStr == "00010203");
    REQUIRE(utils::Hex(buf, hexStr) == ErrorCode::kNone);
    REQUIRE(buf == ByteArray{0x00, 0x01, 0x02, 0x03});
}

TEST_CASE("hex-negative-decoing", "[utils]")
{
    ByteArray buf;

    SECTION("decoding HEX string with odd length should fail")
    {
        REQUIRE(utils::Hex(buf, "00010") == ErrorCode::kInvalidArgs);
    }

    SECTION("decoding HEX string with invalid characters should fail")
    {
        REQUIRE(utils::Hex(buf, "00010g") == ErrorCode::kInvalidArgs);
    }
}

} // namespace commissioner

} // namespace ot
