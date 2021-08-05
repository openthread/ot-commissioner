/*
 *    Copyright (c) 2019, The OpenThread Commissioner Authors.
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
 *   The file includes definition of commissioner errors.
 */

#ifndef OT_COMM_ERROR_HPP_
#define OT_COMM_ERROR_HPP_

#include <memory>
#include <string>

#include <commissioner/defines.hpp>

namespace ot {

namespace commissioner {

/**
 * @brief The canonical error codes for OT Commissioner APIs.
 *
 */
enum class ErrorCode : int
{
    /**
     * Not an error; returned on success.
     */
    kNone = 0,

    /**
     * The operation was cancelled (typically by the caller).
     */
    kCancelled = 1,

    /**
     * Client specified invalid arguments that are problematic
     * regardless of the state of the system (e.g., a malformed file name).
     */
    kInvalidArgs = 2,

    /**
     * Invalid CLI command.
     */
    kInvalidCommand = 3,

    /**
     * Timeout before operation could complete.  For operations
     * that change the state of the system, this error may be returned
     * even if the operation has completed successfully. For example, a
     * successful response from a server could have been delayed long
     * enough for the deadline to expire.
     */
    kTimeout = 4,

    /**
     * Some requested entity (e.g., TLV) was not found.
     * For privacy reasons, this code *may* be returned when the client
     * does not have the access right to the entity.
     */
    kNotFound = 5,

    /**
     * Security failures, such as signature validation, message signing
     * and (D)TLS handshake failure.
     */
    kSecurity = 6,

    /**
     * Operation is not implemented or not supported/enabled in this service.
     */
    kUnimplemented = 7,

    /**
     * Message, TLV or encoded data is in bad format.
     */
    kBadFormat = 8,

    /**
     * The commissioner is busy that current request cannot be processed.
     * It is mostly that the commissioner is receiving duplicate requests
     * before finishing the previous one.
     */
    kBusy = 9,

    /**
     * Running out of memory.
     */
    kOutOfMemory = 10,

    /**
     * Read/write to file/socket failed.
     */
    kIOError = 11,

    /**
     * The file/socket is busy and read/write to file/socket will be blocked.
     */
    kIOBusy = 12,

    /**
     * Some entity that we attempted to create (e.g., CoAP resource)
     * already exists.
     */
    kAlreadyExists = 13,

    /**
     * The operation, transaction or message exchange was aborted.
     */
    kAborted = 14,

    /**
     * The commissioner is not in a valid state that the operation can be processed.
     */
    kInvalidState = 15,

    /**
     * The operation was rejected. For example, petition could be rejected
     * because of existing active commissioner.
     */
    kRejected = 16,

    /**
     * The operation was failed because with a CoAP error.
     */
    kCoapError = 17,

    /**
     * The Registry operation failed.
     */
    kRegistryError = 18,

    /**
     * The error is out of the address space of OT Commissioner.
     */
    kUnknown = 19,
};

/**
 * @brief The error of a call in OT Commissioner.
 *
 */
class OT_COMM_MUST_USE_RESULT Error
{
public:
    /**
     * The default error is none error.
     *
     */
    Error()
        : mCode(ErrorCode::kNone)
    {
    }

    /**
     * Creates a error with the specified error code and message as a
     * human-readable string containing more detailed information.
     *
     */
    Error(ErrorCode aErrorCode, const std::string &aErrorMessage)
        : mCode(aErrorCode)
        , mMessage(aErrorMessage)
    {
    }

    // Copy the specified error.
    Error(const Error &aError);
    Error &operator=(const Error &aError);
    Error(Error &&aError) noexcept;
    Error &operator=(Error &&aError) noexcept;

    /**
     * Two errors are considered equal when their error codes are equal.
     *
     */
    bool operator==(const Error &aOther) const { return GetCode() == aOther.GetCode(); }
    bool operator!=(const Error &aOther) const { return !(*this == aOther); }

    ErrorCode GetCode() const { return mCode; }

    const std::string &GetMessage() const { return mMessage; }

    /**
     * Returns a string representation of this error suitable for
     * printing. Returns the string `"OK"` for success.
     *
     */
    std::string ToString() const;

    /**
     * Ignores any errors. This method does nothing except potentially suppress
     * complaints from any tools that are checking that errors are not dropped on
     * the floor.
     *
     */
    void IgnoreError() const {}

private:
    ErrorCode   mCode;
    std::string mMessage;
};

inline Error::Error(const Error &aError)
    : mCode(aError.mCode)
    , mMessage(aError.mMessage)
{
}

inline Error &Error::operator=(const Error &aError)
{
    mCode    = aError.mCode;
    mMessage = aError.mMessage;
    return *this;
}

inline Error::Error(Error &&aError) noexcept
    : mCode(std::move(aError.mCode))
    , mMessage(std::move(aError.mMessage))
{
}

inline Error &Error::operator=(Error &&aError) noexcept
{
    mCode    = std::move(aError.mCode);
    mMessage = std::move(aError.mMessage);
    return *this;
}

inline bool operator==(const Error &aError, const ErrorCode &aErrorCode)
{
    return aError.GetCode() == aErrorCode;
}

inline bool operator!=(const Error &aError, const ErrorCode &aErrorCode)
{
    return !(aError == aErrorCode);
}

inline bool operator==(const ErrorCode &aErrorCode, const Error &aError)
{
    return aError == aErrorCode;
}

inline bool operator!=(const ErrorCode &aErrorCode, const Error &aError)
{
    return !(aErrorCode == aError);
}

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_ERROR_HPP_
