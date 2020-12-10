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
 *   The file defines OS independent file ops
 */

#ifndef _OS_FILE_HPP_
#define _OS_FILE_HPP_

#include <string>

namespace ot {

namespace os {

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

} // namespace os

} // namespace ot

#endif // _OS_FILE_HPP_
