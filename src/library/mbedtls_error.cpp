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
 *   This file implements mbedtls error facilities.
 */

#include "mbedtls_error.hpp"

#include <assert.h>

#include <mbedtls/error.h>
#include <mbedtls/ssl.h>

#include "common/error_macros.hpp"
#include "common/utils.hpp"

namespace ot {

namespace commissioner {

/**
 * This function convert mbedtls error to OT Commissioner error.
 *
 * For the implementation details, please reference to <mbedtls/error.h>.
 *
 */
Error ErrorFromMbedtlsError(int aMbedtlsError)
{
    // See <mbedtls/error.h> for the constants.
    static constexpr int kMbedtlsErrorLowLevelNetBegin        = -0x0052;
    static constexpr int kMbedtlsErrorLowLevelNetEnd          = -0x0042;
    static constexpr int kMbedtlsErrorHighLevelModuleIdMask   = 0x7000;
    static constexpr int kMbedtlsErrorHighLevelModuleIdOffset = 12;
    static constexpr int kMbedtlsErrorHighLevelModuleIdCipher = 6;
    static constexpr int kMbedtlsErrorHighLevelModuleIdSsl    = 7;
    static constexpr int kMbedtlsErrorMsgMaxLength            = 256;

    assert(aMbedtlsError <= 0);

    ErrorCode errorCode;

    uint16_t highLevelModuleId = (static_cast<uint16_t>(-aMbedtlsError) & kMbedtlsErrorHighLevelModuleIdMask) >>
                                 kMbedtlsErrorHighLevelModuleIdOffset;

    if (aMbedtlsError == 0)
    {
        errorCode = ErrorCode::kNone;
    }
    else if (aMbedtlsError == MBEDTLS_ERR_SSL_WANT_READ || aMbedtlsError == MBEDTLS_ERR_SSL_WANT_WRITE ||
             aMbedtlsError == MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS || aMbedtlsError == MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS)
    {
        errorCode = ErrorCode::kIOBusy;
        ;
    }
    else if (aMbedtlsError >= kMbedtlsErrorLowLevelNetBegin && aMbedtlsError <= kMbedtlsErrorLowLevelNetEnd)
    {
        // Low-level NET error.
        errorCode = ErrorCode::kIOError;
    }
    else if (highLevelModuleId == kMbedtlsErrorHighLevelModuleIdCipher ||
             highLevelModuleId == kMbedtlsErrorHighLevelModuleIdSsl)
    {
        // High-level SSL or CIPHER error.
        errorCode = ErrorCode::kSecurity;
    }
    else
    {
        errorCode = ErrorCode::kUnknown;
    }

    char mbedtlsErrorMsg[kMbedtlsErrorMsgMaxLength + 1];
    mbedtls_strerror(aMbedtlsError, mbedtlsErrorMsg, sizeof(mbedtlsErrorMsg));

    return errorCode == ErrorCode::kNone ? ERROR_NONE : Error{errorCode, mbedtlsErrorMsg};
}

} // namespace commissioner

} // namespace ot
