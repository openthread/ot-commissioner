/*
 *    Copyright (c) 2020, The OpenThread Authors.
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
 *  This file defines file utilities.
 *
 */

#ifndef OT_COMM_APP_FILE_UTIL_HPP_
#define OT_COMM_APP_FILE_UTIL_HPP_

#include <string>

#include "commissioner/defines.hpp"
#include "commissioner/error.hpp"

namespace ot {

namespace commissioner {

/**
 * This function writes a string to the target file.
 *
 * Create the target file if it is not found.
 * Clear the target file if it is not empty and write from the beginning.
 *
 * @param[in] aData      The data to be written.
 * @param[in] aFileanme  The name of the target file.
 *
 * @retval Error::kNone  Successfully written the whole string.
 * @retval ...           Failed to write the string.
 *
 * @note This function is not atomic, which means the target file
 *       could be corrupted if this function failed.
 *
 */
Error WriteFile(const std::string &aData, const std::string &aFilename);

/**
 * This function reads a file into a std::string.
 *
 * @param[out] aData     The data to read to.
 * @param[in] aFileanme  The name of the file.
 *
 * @retval Error::kNone      Successfully read the whole file.
 * @retval Error::kNotFound  Cannot find the given file.
 *
 */
Error ReadFile(std::string &aData, const std::string &aFilename);

/**
 * This function reads a PEM file into a ByteArray.
 *
 * @param[out] aData     The data to read to.
 * @param[in] aFileanme  The name of the file.
 *
 * @retval Error::kNone      Successfully read the whole file.
 * @retval Error::kNotFound  Cannot find the given file.
 *
 * @note A '\0' will be append to the end of the data buffer
 *       (this is required by mbedtls to distinguish DER and PEM).
 *
 */
Error ReadPemFile(ByteArray &aData, const std::string &aFilename);

/**
 * This function reads a HEX string file into a ByteArray.
 *
 * Any spaces in the file are accepted and ingored to produce a
 * continuous byte array.
 *
 * @param[out] aData     The data to read to.
 * @param[in] aFileanme  The name of the file.
 *
 * @retval Error::kNone       Successfully read the whole file.
 * @retval Error::kNotFound   Cannot find the given file.
 * @retval Error::kBadFormat  There are invalid characters in the file.
 *
 */
Error ReadHexStringFile(ByteArray &aData, const std::string &aFilename);

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_APP_FILE_UTIL_HPP_
