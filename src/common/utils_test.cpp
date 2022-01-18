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

#include <gtest/gtest.h>

namespace ot {

namespace commissioner {

TEST(UtilsTest, IntegerEncodingDecoding_8BitsInteger)
{
    ByteArray buf;

    buf = utils::Encode<uint8_t>(0xFC);
    EXPECT_EQ(buf, (ByteArray{0xFC}));

    utils::Encode<uint8_t>(buf, 0xFB);
    EXPECT_EQ(buf, (ByteArray{0xFC, 0xFB}));

    EXPECT_EQ(utils::Decode<int8_t>(utils::Encode<int8_t>(0xFD)), static_cast<int8_t>(0xFD));
}

TEST(UtilsTest, IntegerEncodingDecoding_16BitsInteger)
{
    ByteArray buf;
    buf = utils::Encode<uint16_t>(0x010F);
    EXPECT_EQ(buf, (ByteArray{0x01, 0x0F}));
    EXPECT_EQ(utils::Decode<uint16_t>(buf), 0x010F);

    EXPECT_EQ(utils::Decode<int16_t>(utils::Encode<int16_t>(0xFDFC)), static_cast<int16_t>(0xFDFC));
}

TEST(UtilsTest, IntegerEncodingDecoding_32BitsInteger)
{
    ByteArray buf;
    buf = utils::Encode<uint32_t>(0x00010E0F);
    EXPECT_EQ(buf, (ByteArray{0x00, 0x01, 0x0E, 0x0F}));
    EXPECT_EQ(utils::Decode<uint32_t>(buf), 0x00010E0FU);

    EXPECT_EQ(utils::Decode<int32_t>(utils::Encode<int32_t>(0xFDFCFBFA)), static_cast<int32_t>(0xFDFCFBFA));
}

TEST(UtilsTest, IntegerEncodingDecoding_64BitsInteger)
{
    ByteArray buf;
    buf = utils::Encode<uint64_t>(0x00010E0F00010E0F);
    EXPECT_EQ(buf, (ByteArray{0x00, 0x01, 0x0E, 0x0F, 0x00, 0x01, 0x0E, 0x0F}));
    EXPECT_EQ(utils::Decode<uint64_t>(buf), 0x00010E0F00010E0FU);

    EXPECT_EQ(utils::Decode<int64_t>(utils::Encode<int64_t>(0xFDFCFBFAF9F8F7F6)),
              static_cast<int64_t>(0xFDFCFBFAF9F8F7F6));
}

TEST(UtilsTest, HexEncodingDecoding_ByteArraysEqualAfterEncodingDecoding)
{
    ByteArray   buf;
    std::string hexStr;

    hexStr = utils::Hex(ByteArray{0x00, 0x01, 0x02, 0x03});

    EXPECT_EQ(hexStr, "00010203");
    EXPECT_EQ(utils::Hex(buf, hexStr), ErrorCode::kNone);
    EXPECT_EQ(buf, (ByteArray{0x00, 0x01, 0x02, 0x03}));
}

TEST(UtilsTest, HexEncodingDecoding_EmptyStringDecodedIntoEmptyByteArray)
{
    ByteArray   buf;
    std::string hexStr;

    EXPECT_EQ(utils::Hex(buf, hexStr), ErrorCode::kNone);
    EXPECT_TRUE(buf.empty());
}

TEST(UtilsTest, HexEncodingDecoding_DecodingHexStringWithOddLengthShouldFail)
{
    ByteArray buf;

    EXPECT_EQ(utils::Hex(buf, "00010"), ErrorCode::kInvalidArgs);
}

TEST(UtilsTest, HexEncodingDecoding_DecodingHexStringWithInvalidCharactersShouldFail)
{
    ByteArray buf;

    EXPECT_EQ(utils::Hex(buf, "00010g"), ErrorCode::kInvalidArgs);
}

} // namespace commissioner

} // namespace ot
