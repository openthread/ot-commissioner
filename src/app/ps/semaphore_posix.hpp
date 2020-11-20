/***************************************************************************
 * TODO: Copyright
 ***************************************************************************
 * @brief POSIX semaphore
 */

#ifndef _OS_SEMAPHORE_POSIX_HPP_
#define _OS_SEMAPHORE_POSIX_HPP_

#if !defined(_WIN32) && !defined(WIN32)

#include <sys/stat.h>
#include <semaphore.h>

namespace tg_os {
    namespace sem {

        /**
         * OS dependent semaphore impl
         */
        struct semaphore {
            sem_t * platform; /**< posix semaphore type */
        };
    }
}

#endif

#endif // _OS_SEMAPHORE_POSIX_HPP_
