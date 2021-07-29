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
 *   The file defines global system wide semaphore ops.
 */

#ifndef _OS_SEMAPHORE_HPP_
#define _OS_SEMAPHORE_HPP_

#include <string>

#if defined(_WIN32) || defined(WIN32)
#include "semaphore_win.hpp"
#else
#include "semaphore_posix.hpp"
#endif

namespace ot {

namespace os {

namespace semaphore {

/**
 * Semaphore operation status
 */
enum SemaphoreStatus
{
    kSuccess, /**< operation succeeded */
    kError,   /**< operation failed */
};

/**
 * Opens/creates semaphore.
 *
 * Name of the semaphore should not contain any suffixes or prefixes
 * and will be converted to system specific format.
 *
 * @param[in] aName semaphore name
 * @param[out] aSem created semaphore
 * @see Semaphore
 * @return SemaphoreStatus
 * @see SemaphoreStatus
 */
SemaphoreStatus SemaphoreOpen(std::string const &aName, Semaphore &aSem);

/**
 * Close semaphore previously created by open
 * @see SemaphoreOpen
 *
 * @param[in] aSem semaphore
 * @see Semaphore
 * @return SemaphoreStatus
 * @see SemaphoreStatus
 */
SemaphoreStatus SemaphoreClose(Semaphore &aSem);

/**
 * Posts semaphore
 *
 * @param[in] aSem semaphore
 * @see Semaphore
 * @return SemaphoreStatus
 * @see SemaphoreStatus
 */
SemaphoreStatus SemaphorePost(Semaphore &aSem);

/**
 * Blocks until semaphore can be obtained
 *
 * @param[in] aSem semaphore
 * @See Semaphore
 * @return SemaphoreStatus
 * @see SemaphoreStatus
 */
SemaphoreStatus SemaphoreWait(Semaphore &aSem);

} // namespace semaphore

} // namespace os

} // namespace ot

#endif // _OS_SEMAPHORE_HPP_
