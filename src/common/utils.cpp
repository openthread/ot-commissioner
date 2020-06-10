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
 *   This file implements commissioner utilities.
 */

#include "common/utils.hpp"

#include <ctype.h>

#include "error_macros.hpp"

namespace ot {

namespace commissioner {

namespace utils {

template <> void Encode<uint8_t>(ByteArray &aBuf, uint8_t aInteger)
{
    aBuf.emplace_back(aInteger);
}

template <> void Encode<int8_t>(ByteArray &aBuf, int8_t aInteger)
{
    aBuf.emplace_back(aInteger);
}

template <> ByteArray Encode<uint8_t>(uint8_t aInteger)
{
    return {aInteger};
}

template <> ByteArray Encode<int8_t>(int8_t aInteger)
{
    return {static_cast<uint8_t>(aInteger)};
}

std::string Hex(const ByteArray &aBytes)
{
    std::string str;
    for (auto byte : aBytes)
    {
        char c0 = (byte & 0xF0) >> 4;
        char c1 = byte & 0x0F;
        str.push_back(c0 < 10 ? (c0 + '0') : (c0 + 'a' - 10));
        str.push_back(c1 < 10 ? (c1 + '0') : (c1 + 'a' - 10));
    }
    return str;
}

static inline uint8_t Hex(char c)
{
    VerifyOrDie(isxdigit(c));
    return c >= 'A' && c <= 'F' ? (c - 'A' + 10) : c >= 'a' && c <= 'f' ? (c - 'a' + 10) : (c - '0');
}

Error Hex(ByteArray &aBuf, const std::string &aHexStr)
{
    if (aHexStr.size() % 2 != 0)
    {
        return ERROR_INVALID_ARGS("{} is not a valid HEX string; must have even length", aHexStr);
    }

    for (size_t i = 0; i < aHexStr.size(); i += 2)
    {
        if (!isxdigit(aHexStr[i]) || !isxdigit(aHexStr[i + 1]))
        {
            return ERROR_INVALID_ARGS("{} is not a valid HEX string; there is non-HEX char", aHexStr);
        }
        aBuf.push_back((Hex(aHexStr[i]) << 4) | Hex(aHexStr[i + 1]));
    }
    return ERROR_NONE;
}

} // namespace utils

} // namespace commissioner

} // namespace ot
