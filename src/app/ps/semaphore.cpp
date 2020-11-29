#include "semaphore.hpp"

#include <errno.h>
#include <fcntl.h>

namespace tg_os {
namespace sem {

#if defined(_WIN32) || defined(WIN32)
sem_status semaphore_open(std::string const &name, semaphore &sem)
{
    std::string sem_name = "Global\\" + name;
    sem.platform         = NULL;

    sem.platform = CreateSemaphoreA(NULL, 1, 1, sem_name.c_str());
    if (sem.platform == NULL)
    {
        return SEM_ERROR;
    }

    return SEM_SUCCESS;
}

sem_status semaphore_close(semaphore &sem)
{
    CloseHandle(sem.platform);
    sem.platform = NULL;
    return SEM_SUCCESS;
}

sem_status semaphore_post(semaphore &sem)
{
    if (ReleaseSemaphore(sem.platform, 1, NULL) == 0)
    {
        return SEM_ERROR;
    }
    return SEM_SUCCESS;
}

sem_status semaphore_wait(semaphore &sem)
{
    if (WaitForSingleObject(sem.platform, INFINITE) != WAIT_OBJECT_0)
    {
        return SEM_ERROR;
    }
    return SEM_SUCCESS;
}

#else

sem_status semaphore_open(std::string const &name, semaphore &sem)
{
    std::string sem_name = "/" + name;
    sem.platform         = NULL;

    sem.platform = sem_open(sem_name.c_str(), O_CREAT, S_IWUSR | S_IRUSR, 1);
    if (sem.platform == SEM_FAILED)
    {
        return SEM_ERROR;
    }

    return SEM_SUCCESS;
}

sem_status semaphore_close(semaphore &sem)
{
    sem_close(sem.platform);
    sem.platform = NULL;
    return SEM_SUCCESS;
}

sem_status semaphore_post(semaphore &sem)
{
    if (sem_post(sem.platform) == -1)
    {
        return SEM_ERROR;
    }
    return SEM_SUCCESS;
}

sem_status semaphore_wait(semaphore &sem)
{
    if (sem_wait(sem.platform) == -1)
    {
        return SEM_ERROR;
    }
    return SEM_SUCCESS;
}

#endif

} // namespace sem
} // namespace tg_os
