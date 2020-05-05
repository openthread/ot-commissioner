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
 *   The file implements errors.
 */

#include <commissioner/error.hpp>

#include "common/utils.hpp"

namespace ot {

namespace commissioner {

const std::string ErrorToString(const Error &aError)
{
    switch (aError)
    {
    case Error::kNone:
        return "None";
    case Error::kAbort:
        return "Abort";
    case Error::kAlready:
        return "Already";
    case Error::kBadFormat:
        return "BadFormat";
    case Error::kBadOption:
        return "BadOption";
    case Error::kBusy:
        return "Busy";
    case Error::kFailed:
        return "Failed";
    case Error::kInvalidArgs:
        return "InvalidArgs";
    case Error::kInvalidAddr:
        return "InvalidAddr";
    case Error::kInvalidState:
        return "InvalidState";
    case Error::kNotFound:
        return "NotFound";
    case Error::kNotImplemented:
        return "NotImplemented";
    case Error::kOutOfMemory:
        return "OutOfMemory";
    case Error::kReject:
        return "Reject";
    case Error::kSecurity:
        return "Security";
    case Error::kTimeout:
        return "Timeout";
    case Error::kTransportBusy:
        return "TransportBusy";
    case Error::kTransportFailed:
        return "TransportFailed";
    default:
        VerifyOrDie(false);
        return "";
    }
}

} // namespace commissioner

} // namespace ot
