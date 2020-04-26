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
 *  This file defines file logger.
 *
 */

#include "app/file_logger.hpp"

#include "common/time.hpp"
#include "common/utils.hpp"

namespace ot {

namespace commissioner {

static std::string ToString(LogLevel aLevel)
{
    std::string ret;

    switch (aLevel)
    {
    case LogLevel::kOff:
        ret = "off";
        break;
    case LogLevel::kCritical:
        ret = "critical";
        break;
    case LogLevel::kError:
        ret = "error";
        break;
    case LogLevel::kWarn:
        ret = "warn";
        break;
    case LogLevel::kInfo:
        ret = "info";
        break;
    case LogLevel::kDebug:
        ret = "debug";
        break;
    default:
        ASSERT(false);
        break;
    }

    return ret;
}

FileLogger::FileLogger(const std::string &aFilename, ot::commissioner::LogLevel aLogLevel)
    : mFileStream(aFilename)
    , mLogLevel(aLogLevel)
{
}

void FileLogger::Log(ot::commissioner::LogLevel aLevel, const std::string &aMsg)
{
    VerifyOrExit(aLevel <= mLogLevel);
    VerifyOrExit(mFileStream.is_open());

    mFileStream << "[ " << TimePointToString(Clock::now()) << " ] [ " << ToString(aLevel) << " ] " << aMsg << std::endl;

exit:
    return;
}

} // namespace commissioner

} // namespace ot
