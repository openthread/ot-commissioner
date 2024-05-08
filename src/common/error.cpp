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
 *   The file implements errors.
 */

#include "commissioner/error.hpp"

#include <string>

#include "common/utils.hpp"

namespace ot {

namespace commissioner {

// Returns the std::string representation of the status code.
std::string ErrorCodeToString(ErrorCode code)
{
    switch (code)
    {
    case ErrorCode::kNone:
        return "OK";
    case ErrorCode::kCancelled:
        return "CANCELLED";
    case ErrorCode::kInvalidArgs:
        return "INVALID_ARGS";
    case ErrorCode::kInvalidCommand:
        return "INVALID_COMMAND";
    case ErrorCode::kTimeout:
        return "TIMEOUT";
    case ErrorCode::kNotFound:
        return "NOT_FOUND";
    case ErrorCode::kSecurity:
        return "SECURITY";
    case ErrorCode::kUnimplemented:
        return "UNIMPLEMENTED";
    case ErrorCode::kBadFormat:
        return "BAD_FORMAT";
    case ErrorCode::kBusy:
        return "BUSY";
    case ErrorCode::kOutOfMemory:
        return "OUT_OF_MEMORY";
    case ErrorCode::kIOError:
        return "IO_ERROR";
    case ErrorCode::kIOBusy:
        return "IO_BUSY";
    case ErrorCode::kAlreadyExists:
        return "ALREADY_EXISTS";
    case ErrorCode::kAborted:
        return "ABORTED";
    case ErrorCode::kInvalidState:
        return "INVALID_STATE";
    case ErrorCode::kRejected:
        return "REJECTED";
    case ErrorCode::kCoapError:
        return "COAP_ERROR";
    case ErrorCode::kRegistryError:
        return "REGISTRY_ERROR";
    case ErrorCode::kUnknown:
        return "UNKNOWN";

    default:
        VerifyOrDie(false);
        return "UNKNOWN";
    }
}

std::string Error::ToString() const
{
    std::string ret;
    if (GetCode() == ErrorCode::kNone)
    {
        ret = ErrorCodeToString(ErrorCode::kNone);
    }
    else
    {
        ret = ErrorCodeToString(GetCode()) + ": " + mMessage;
    }
    return ret;
}

} // namespace commissioner

} // namespace ot
