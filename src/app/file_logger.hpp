/*
 *    Copyright (c) 2020, The OpenThread Commissioner Authors.
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

#ifndef OT_COMM_APP_FILE_LOGGER_HPP_
#define OT_COMM_APP_FILE_LOGGER_HPP_

#include <mutex>
#include <string>

#include <stdio.h>

#include <commissioner/commissioner.hpp>

namespace ot {

namespace commissioner {

/**
 * @brief An implementation of the Logger interface that write log to a text file.
 *
 */
class FileLogger : public Logger
{
public:
    ~FileLogger() override;

    /**
     * This function returns a file logger with given filename and minimum log level.
     *
     * @param[in] aFilename  The log file name.
     * @param[in] aLogLevel  The minimum log level. Log messages with a lower
     *                       log level than this will be dropped silently.
     *
     * @retval Error::kNone  Successfully created the file logger.
     * @retval ...           Failed to create the file logger.
     *
     */
    static Error Create(std::shared_ptr<FileLogger> &aFileLogger, const std::string &aFilename, LogLevel aLogLevel);

    void Log(LogLevel aLevel, const std::string &aRegion, const std::string &aMsg) override;

private:
    FileLogger() = default;
    Error Init(const std::string &aFilename, LogLevel aLogLevel);

    FILE      *mLogFile;
    LogLevel   mLogLevel;
    std::mutex mLogMutex;
};

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_APP_FILE_LOGGER_HPP_
