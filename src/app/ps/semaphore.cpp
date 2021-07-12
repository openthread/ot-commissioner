#include "semaphore.hpp"

#include <errno.h>
#include <fcntl.h>

namespace ot {

namespace os {

namespace semaphore {

#if defined(_WIN32) || defined(WIN32)
SemaphoreStatus SemaphoreOpen(std::string const &aName, Semaphore &aSem)
{
    std::string semName = "Global\\" + aName;
    aSem.platform       = NULL;

    aSem.platform = CreateSemaphoreA(NULL, 1, 1, semName.c_str());
    if (aSem.platform == NULL)
    {
        return kError;
    }

    return kSuccess;
}

SemaphoreStatus SemaphoreClose(Semaphore &aSem)
{
    CloseHandle(aSem.platform);
    aSem.platform = NULL;
    return kSuccess;
}

SemaphoreStatus SemaphorePost(Semaphore &aSem)
{
    if (ReleaseSemaphore(aSem.platform, 1, NULL) == 0)
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
    aSem.mPlatform      = NULL;

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
    aSem.mPlatform = NULL;
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
