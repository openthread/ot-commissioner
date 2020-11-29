/***************************************************************************
 * TODO: Copyright
 ***************************************************************************
 * @brief OS independent file ops
 */

#ifndef _OS_FILE_HPP_
#define _OS_FILE_HPP_

#include <string>

namespace tg_os {
namespace file {

/**
 * File operation status
 */
enum file_status
{
    FS_SUCCESS,          /**< operation succeeded */
    FS_NOT_EXISTS,       /**< requested file not found */
    FS_PERMISSION_ERROR, /**< file cannot be opened due to access err */
    FS_ERROR             /**< other io error */
};

/**
 * Reads file content into string
 *
 * Files are opend in exclusive mode
 *
 * @param[in] name  file name to be read
 * @param[out] data file content
 * @return file_status
 * @see file_status
 */
file_status file_read(std::string const &name, std::string &data);

/**
 * Writes data to file.
 *
 * Files are opend in exclusive mode
 *
 * If not exists file will be created.
 * Previous content will be lost.
 *
 * @param[in] name file name to be written/created
 * @param[in] data data to be written
 * @return file_status
 * @see file_status
 */
file_status file_write(std::string const &name, std::string const &data);
} // namespace file
} // namespace tg_os

#endif // _OS_FILE_HPP_
