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
 *   The file defines errors.
 */

#ifndef COMMISSIONER_INCLUDE_ERROR_HPP_
#define COMMISSIONER_INCLUDE_ERROR_HPP_

#include <string>

namespace ot {

namespace commissioner {

enum class Error : int
{
    kNone = 0,        ///< No error.
    kAbort,           ///< A request is aborted.
    kAlready,         ///< Operation has already be executed.
    kBadFormat,       ///< Request/Response is in bad format.
    kBadOption,       ///< Request/Response has bad options.
    kBusy,            ///< The commissioner is now busy.
    kFailed,          ///< Generic failure.
    kInvalidArgs,     ///< Invalid arguments.
    kInvalidAddr,     ///< Invalid address.
    kInvalidState,    ///< The commissioner is currently in invalid state.
    kNotFound,        ///< Things not found.
    kNotImplemented,  ///< Feature not implemented.
    kOutOfMemory,     ///< Running out of memory.
    kReject,          ///< Request is rejected.
    kSecurity,        ///< Security related errors.
    kTimeout,         ///< Request timeouted.
    kTransportBusy,   ///< Transport level is now busy.
    kTransportFailed, ///< Transport level failure.
};

/**
 * @brief Convert Error to printable string.
 *
 * @param aError  A error code.
 *
 * @return The printable string of the error.
 */
const std::string ErrorToString(const Error &aError);

} // namespace commissioner

} // namespace ot

#endif // COMMISSIONER_INCLUDE_ERROR_HPP_
