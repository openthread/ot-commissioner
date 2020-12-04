/***************************************************************************
 * TODO: Copyright
 ***************************************************************************
 * @brief POSIX semaphore
 */

#ifndef _OS_SEMAPHORE_POSIX_HPP_
#define _OS_SEMAPHORE_POSIX_HPP_

#if !defined(_WIN32) && !defined(WIN32)

#include <semaphore.h>
#include <sys/stat.h>

namespace ot {

namespace os {

namespace sem {

/**
 * OS dependent semaphore impl
 */
struct semaphore
{
    sem_t *platform; /**< posix semaphore type */
};

} // namespace sem

} // namespace os

} // namespace ot

#endif

#endif // _OS_SEMAPHORE_POSIX_HPP_
