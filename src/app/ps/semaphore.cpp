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
 *   The file implements global system wide semaphore ops.
 */

#include "semaphore.hpp"

#include <fcntl.h>
#include <semaphore.h>
#include <string>

#include "app/ps/semaphore_posix.hpp"

namespace ot {

namespace os {

namespace semaphore {

#if defined(_WIN32) || defined(WIN32)
SemaphoreStatus SemaphoreOpen(std::string const &aName, Semaphore &aSem)
{
    std::string semName = "Global\\" + aName;
    aSem.platform       = nullptr;

    aSem.platform = CreateSemaphoreA(nullptr, 1, 1, semName.c_str());
    if (aSem.platform == nullptr)
    {
        return kError;
    }

    return kSuccess;
}

SemaphoreStatus SemaphoreClose(Semaphore &aSem)
{
    CloseHandle(aSem.platform);
    aSem.platform = nullptr;
    return kSuccess;
}

SemaphoreStatus SemaphorePost(Semaphore &aSem)
{
    if (ReleaseSemaphore(aSem.platform, 1, nullptr) == 0)
    {
        return kError;
    }
    return kSuccess;
}

SemaphoreStatus SemaphoreWait(Semaphore &aSem)
{
    if (WaitForSingleObject(aSem.platform, INFINITE) != WAIT_OBJECT_0)
    {
        return kError;
    }
    return kSuccess;
}

#else

SemaphoreStatus SemaphoreOpen(std::string const &aName, Semaphore &aSem)
{
    std::string semName = "/" + aName;
    aSem.mPlatform      = nullptr;

    aSem.mPlatform = sem_open(semName.c_str(), O_CREAT, S_IWUSR | S_IRUSR, 1);
    if (aSem.mPlatform == SEM_FAILED)
    {
        return kError;
    }

    return kSuccess;
}

SemaphoreStatus SemaphoreClose(Semaphore &aSem)
{
    sem_close(aSem.mPlatform);
    aSem.mPlatform = nullptr;
    return kSuccess;
}

SemaphoreStatus SemaphorePost(Semaphore &aSem)
{
    if (sem_post(aSem.mPlatform) == -1)
    {
        return kError;
    }
    return kSuccess;
}

SemaphoreStatus SemaphoreWait(Semaphore &aSem)
{
    if (sem_wait(aSem.mPlatform) == -1)
    {
        return kError;
    }
    return kSuccess;
}

#endif

} // namespace semaphore

} // namespace os

} // namespace ot
