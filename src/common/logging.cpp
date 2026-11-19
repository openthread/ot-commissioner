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
 *   This file includes wrapper of mbedtls.
 */

#include <memory>
#include <mutex>
#include <string>

#include "commissioner/commissioner.hpp"
#include "common/logging.hpp"

namespace ot {

namespace commissioner {

// The logger is shared by all commissioner instances and may be replaced
// by one instance while the event loop threads of others are logging.
static std::mutex              sLoggerMutex;
static std::shared_ptr<Logger> sLogger;

void InitLogger(std::shared_ptr<Logger> aLogger)
{
    {
        std::lock_guard<std::mutex> lock(sLoggerMutex);

        sLogger.swap(aLogger);
    }

    // Release the previous logger (now in `aLogger`) outside of the lock.
}

std::shared_ptr<Logger> GetLogger(void)
{
    std::lock_guard<std::mutex> lock(sLoggerMutex);

    return sLogger;
}

void Log(LogLevel aLevel, const std::string &aRegion, const std::string &aMessage)
{
    std::shared_ptr<Logger> logger = GetLogger();

    if (logger != nullptr)
    {
        logger->Log(aLevel, aRegion, aMessage);
    }
}

} // namespace commissioner

} // namespace ot
