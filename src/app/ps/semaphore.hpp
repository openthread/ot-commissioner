/***************************************************************************
 * TODO: Copyright
 ***************************************************************************
 * @brief OS independent global system wide semaphore ops
 */

#ifndef _OS_SEMAPHORE_HPP_
#define _OS_SEMAPHORE_HPP_

#include <string>

#if defined(_WIN32) || defined(WIN32)
#include "semaphore_win.hpp"
#else
#include "semaphore_posix.hpp"
#endif

namespace tg_os {
namespace sem {

/**
 * Semaphore operation status
 */
enum sem_status
{
    SEM_SUCCESS, /**< operation succeeded */
    SEM_ERROR    /**< operation failed */
};

/**
 * Opens/creates semaphore.
 *
 * Name of the semaphore should not contain any suffixes or prefixes
 * and will be converted to system specific format.
 *
 * @param[in] name semaphore name
 * @param[out] sem created semaphore
 * @see semaphore
 * @return sem_status
 * @see sem_status
 */
sem_status semaphore_open(std::string const &name, semaphore &sem);

/**
 * Close semaphore previously created by open
 * @see semaphore_open
 *
 * @param[in] sem semaphore
 * @see semaphore
 * @return sem_status
 * @see sem_status
 */
sem_status semaphore_close(semaphore &sem);

/**
 * Posts semaphore
 *
 * @param[in] sem semaphore
 * @see semaphore
 * @return sem_status
 * @see sem_status
 */
sem_status semaphore_post(semaphore &sem);

/**
 * Blocks until semaphore can be obtained
 *
 * @param[in] sem semaphore
 * @see semaphore
 * @return sem_status
 * @see sem_status
 */
sem_status semaphore_wait(semaphore &sem);
} // namespace sem
} // namespace tg_os

#endif // _OS_SEMAPHORE_HPP_
