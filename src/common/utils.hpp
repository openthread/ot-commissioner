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
 *   This file includes definitions of utilities.
 */

#ifndef OT_COMM_COMMON_UTILS_HPP_
#define OT_COMM_COMMON_UTILS_HPP_

#include <string>

#include <assert.h>

#include <commissioner/defines.hpp>
#include <commissioner/error.hpp>

#define ASSERT(aCondition)             \
    do                                 \
    {                                  \
        bool condition = (aCondition); \
        assert(condition);             \
        if (condition)                 \
        {                              \
        }                              \
    } while (false)

#define SuccessOrDie(aError)            \
    do                                  \
    {                                   \
        VerifyOrDie((aError).IsNone()); \
    } while (false)

#define VerifyOrDie(aCondition)             \
    do                                      \
    {                                       \
        if (!(aCondition))                  \
        {                                   \
            /* TODO(wgtdkp): print trace */ \
            abort();                        \
        }                                   \
    } while (false)

#define SuccessOrExit(aError)   \
    do                          \
    {                           \
        if (!(aError).IsNone()) \
        {                       \
            goto exit;          \
        }                       \
    } while (false)

#define VerifyOrExit(aCondition, ...) \
    do                                \
    {                                 \
        if (!(aCondition))            \
        {                             \
            __VA_ARGS__;              \
            goto exit;                \
        }                             \
    } while (false)

#define ExitNow(...) \
    do               \
    {                \
        __VA_ARGS__; \
        goto exit;   \
    } while (false)

static inline void IgnoreError(ot::commissioner::Error aError)
{
    (void)aError;
}

namespace ot {

namespace commissioner {

namespace utils {

template <typename E> constexpr typename std::underlying_type<E>::type to_underlying(E e) noexcept
{
    return static_cast<typename std::underlying_type<E>::type>(e);
}

template <typename E> constexpr E from_underlying(typename std::underlying_type<E>::type e) noexcept
{
    return static_cast<E>(e);
}

template <typename T> void Encode(ByteArray &aBuf, T aInteger)
{
    auto length = aBuf.size();
    aBuf.resize(length + sizeof(T));
    for (size_t i = aBuf.size(); i > length; --i)
    {
        aBuf[i - 1] = aInteger & 0xFF;
        aInteger >>= 8;
    }
}

template <typename T> ByteArray Encode(T aInteger)
{
    ByteArray buf;
    Encode(buf, aInteger);
    return buf;
}

// The @paBuf must have a length of at least sizeof(T).
template <typename T> T Decode(const uint8_t *aBuf)
{
    T ret = 0;
    for (size_t i = 0; i < sizeof(T); ++i)
    {
        ret = (ret << 8) | aBuf[i];
    }
    return ret;
}

template <typename T> T Decode(const ByteArray &aBuf)
{
    VerifyOrDie(aBuf.size() >= sizeof(T));
    return Decode<T>(&aBuf[0]);
}

template <> void Encode<uint8_t>(ByteArray &aBuf, uint8_t aInteger);

template <> void Encode<int8_t>(ByteArray &aBuf, int8_t aInteger);

template <> ByteArray Encode<uint8_t>(uint8_t aInteger);

template <> ByteArray Encode<int8_t>(int8_t aInteger);

std::string Hex(const ByteArray &aBytes);

Error Hex(ByteArray &aBuf, const std::string &aHexStr);

} // namespace utils

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_COMMON_UTILS_HPP_
