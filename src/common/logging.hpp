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
 *   This file includes definitions of logging module.
 */

#ifndef OT_COMM_LIBRARY_LOGGING_HPP_
#define OT_COMM_LIBRARY_LOGGING_HPP_

#include <fmt/format.h>

#include <commissioner/commissioner.hpp>

#define LOG_REGION_COAP "coap"
#define LOG_REGION_CONFIG "config"
#define LOG_REGION_DTLS "dtls"
#define LOG_REGION_JOINER_SESSION "joiner-session"
#define LOG_REGION_MBEDTLS "mbedtls"
#define LOG_REGION_MESHCOP "meshcop"
#define LOG_REGION_MGMT "mgmt"
#define LOG_REGION_SOCKET "socket"
#define LOG_REGION_THCI "thci"
#define LOG_REGION_TOKEN_MANAGER "token-manager"
#define LOG_REGION_JOB_MANAGER "job-manager"
#define LOG_REGION_JOB "job"
#define LOG_REGION_SECURITY_MATERIALS "security-materials"
#define LOG_REGION_DIAG "meshdiag"

#define LOG(aLevel, aRegion, aFmt, ...)                                     \
    do                                                                      \
    {                                                                       \
        Log(aLevel, aRegion, fmt::format(FMT_STRING(aFmt), ##__VA_ARGS__)); \
    } while (false)

#define LOG_DEBUG(aRegion, aFmt, ...) LOG(LogLevel::kDebug, aRegion, aFmt, ##__VA_ARGS__)
#define LOG_INFO(aRegion, aFmt, ...) LOG(LogLevel::kInfo, aRegion, aFmt, ##__VA_ARGS__)
#define LOG_WARN(aRegion, aFmt, ...) LOG(LogLevel::kWarn, aRegion, aFmt, ##__VA_ARGS__)
#define LOG_ERROR(aRegion, aFmt, ...) LOG(LogLevel::kError, aRegion, aFmt, ##__VA_ARGS__)
#define LOG_CRIT(aRegion, aFmt, ...) LOG(LogLevel::kCritical, aRegion, aFmt, ##__VA_ARGS__)

/**
 * Log macro for pure string objects implementing all the required
 * formatting by themselves.
 *
 * Log levels must be {DEBUG, INFO, WARN, ERROR, CRIT}, i.e. the ones
 * that turn the macro into one of the above. E.g. DEBUG => LOG_DEBUG
 */
#define LOG_STR(aLogLevel, aRegion, aStr) LOG_##aLogLevel(aRegion, "{}", aStr)

namespace ot {

namespace commissioner {

// TODO(wgtdkp): add json format. This is useful for certification.

void                    InitLogger(std::shared_ptr<Logger> aLogger);
std::shared_ptr<Logger> GetLogger(void);

void Log(LogLevel aLevel, const std::string &aRegion, const std::string &aMessage);

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_LIBRARY_LOGGING_HPP_
