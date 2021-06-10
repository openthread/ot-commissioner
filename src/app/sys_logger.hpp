/*
 *    Copyright (c) 2021, The OpenThread Commissioner Authors.
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
 *  This file defines sys logger.
 *
 */

#ifndef OT_COMM_APP_SYS_LOGGER_HPP_
#define OT_COMM_APP_SYS_LOGGER_HPP_

#include <string>

#include <commissioner/commissioner.hpp>

namespace ot {

namespace commissioner {

/**
 * @brief An implementation of the Logger interface that write log to syslog.
 *
 */
class SysLogger : public Logger
{
public:
    ~SysLogger() override;

    /**
     * This function returns a sys logger with given minimum log level.
     *
     * @param[in] aLogLevel  The minimum log level. Log messages with a lower
     *                       log level than this will be dropped silently.
     *
     * @retval An instance of SysLogger.
     *
     */
    static std::shared_ptr<SysLogger> Create(LogLevel aLogLevel);

    void Log(LogLevel aLevel, const std::string &aRegion, const std::string &aMsg) override;

private:
    SysLogger() = default;
    void Init(LogLevel aLogLevel);

    LogLevel mLogLevel;
};

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_APP_SYS_LOGGER_HPP_
