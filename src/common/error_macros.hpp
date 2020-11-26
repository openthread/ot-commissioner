/*
 *    Copyright (c) 2020, The OpenThread Authors.
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
 *   This file includes definition of error macros.
 */

#ifndef ERROR_MACROS_HPP_
#define ERROR_MACROS_HPP_

#include <fmt/format.h>

#include <commissioner/error.hpp>

#define ERROR_NONE \
    Error {}
#define ERROR_CANCELLED(aFormat, ...) \
    Error { ErrorCode::kCancelled, fmt::format(FMT_STRING((aFormat)), ##__VA_ARGS__) }
#define ERROR_INVALID_ARGS(aFormat, ...) \
    Error { ErrorCode::kInvalidArgs, fmt::format(FMT_STRING((aFormat)), ##__VA_ARGS__) }
#define ERROR_INVALID_COMMAND(aFormat, ...) \
    Error { ErrorCode::kInvalidCommand, fmt::format(FMT_STRING((aFormat)), ##__VA_ARGS__) }
#define ERROR_TIMEOUT(aFormat, ...) \
    Error { ErrorCode::kTimeout, fmt::format(FMT_STRING((aFormat)), ##__VA_ARGS__) }
#define ERROR_NOT_FOUND(aFormat, ...) \
    Error { ErrorCode::kNotFound, fmt::format(FMT_STRING((aFormat)), ##__VA_ARGS__) }
#define ERROR_SECURITY(aFormat, ...) \
    Error { ErrorCode::kSecurity, fmt::format(FMT_STRING((aFormat)), ##__VA_ARGS__) }
#define ERROR_UNIMPLEMENTED(aFormat, ...) \
    Error { ErrorCode::kUnimplemented, fmt::format(FMT_STRING((aFormat)), ##__VA_ARGS__) }
#define ERROR_BAD_FORMAT(aFormat, ...) \
    Error { ErrorCode::kBadFormat, fmt::format(FMT_STRING((aFormat)), ##__VA_ARGS__) }
#define ERROR_BUSY(aFormat, ...) \
    Error { ErrorCode::kBusy, fmt::format(FMT_STRING((aFormat)), ##__VA_ARGS__) }
#define ERROR_OUT_OF_MEMORY(aFormat, ...) \
    Error { ErrorCode::kOutOfMemory, fmt::format(FMT_STRING((aFormat)), ##__VA_ARGS__) }
#define ERROR_IO_ERROR(aFormat, ...) \
    Error { ErrorCode::kIOError, fmt::format(FMT_STRING((aFormat)), ##__VA_ARGS__) }
#define ERROR_IO_BUSY(aFormat, ...) \
    Error { ErrorCode::kIOBusy, fmt::format(FMT_STRING((aFormat)), ##__VA_ARGS__) }
#define ERROR_ALREADY_EXISTS(aFormat, ...) \
    Error { ErrorCode::kAlreadyExists, fmt::format(FMT_STRING((aFormat)), ##__VA_ARGS__) }
#define ERROR_ABORTED(aFormat, ...) \
    Error { ErrorCode::kAborted, fmt::format(FMT_STRING((aFormat)), ##__VA_ARGS__) }
#define ERROR_INVALID_STATE(aFormat, ...) \
    Error { ErrorCode::kInvalidState, fmt::format(FMT_STRING((aFormat)), ##__VA_ARGS__) }
#define ERROR_REJECTED(aFormat, ...) \
    Error { ErrorCode::kRejected, fmt::format(FMT_STRING((aFormat)), ##__VA_ARGS__) }
#define ERROR_COAP_ERROR(aFormat, ...) \
    Error { ErrorCode::kCoapError, fmt::format(FMT_STRING((aFormat)), ##__VA_ARGS__) }
#define ERROR_INVALID_SYNTAX(aFormat, ...) \
    Error { ErrorCode::kSyntaxViolation, fmt::format(FMT_STRING((aFormat)), ##__VA_ARGS__) }
#define ERROR_UNKNOWN(aFormat, ...) \
    Error { ErrorCode::kUnknown, fmt::format(FMT_STRING((aFormat)), ##__VA_ARGS__) }

#endif // ERROR_MACROS_HPP_
