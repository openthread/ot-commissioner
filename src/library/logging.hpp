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
 *   This file includes definitions of logging module.
 */

#ifndef LOGGING_HPP_
#define LOGGING_HPP_

#include <fmt/format.h>

#include <commissioner/commissioner.hpp>

#define LOG(level, fmt, ...)                          \
    do                                                \
    {                                                 \
        if (level <= GetLogLevel() && GetLogWriter()) \
        {                                             \
            Log(level, fmt, ##__VA_ARGS__);           \
        };                                            \
    } while (0)

#define LOG_DEBUG(fmt, ...) LOG(LogLevel::kDebug, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) LOG(LogLevel::kInfo, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) LOG(LogLevel::kWarn, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG(LogLevel::kError, fmt, ##__VA_ARGS__)
#define LOG_CRIT(fmt, ...) LOG(LogLevel::kCritical, fmt, ##__VA_ARGS__)

namespace ot {

namespace commissioner {

// TODO(wgtdkp): add json format. This is useful for certification.

void      InitLogger(LogLevel aLevel, LogWriter aWriter);
LogLevel  GetLogLevel();
LogWriter GetLogWriter();

template <typename... Args> void Log(LogLevel aLevel, const std::string &aFmt, const Args &... aArgs)
{
    GetLogWriter()(aLevel, fmt::format(aFmt, aArgs...));
}

} // namespace commissioner

} // namespace ot

#endif // LOGGING_HPP_
