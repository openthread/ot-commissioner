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
 *   This file defines test cases for Error class.
 */

#include "commissioner/error.hpp"

#include "common/error_macros.hpp"

#include <gtest/gtest.h>

namespace ot {

namespace commissioner {

TEST(ErrorTest, ErrorMessage_DefaultErrorHasNoErrorMessage)
{
    Error error;

    EXPECT_EQ(error.GetCode(), ErrorCode::kNone);
    EXPECT_TRUE(error.GetMessage().empty());
}

TEST(ErrorTest, ErrorToString_DefaultErrorShouldReturnOkAsString)
{
    Error error;

    EXPECT_EQ(error.ToString(), "OK");
}

TEST(ErrorTest, ErrorToString_NonOkErrorToString)
{
// Check if the error string starts with the error name.
#define CHECK_ERROR_STR_STARTS_WITH_ERROR_NAME(aErrorName) \
    EXPECT_EQ((ERROR_##aErrorName("").ToString().find(#aErrorName)), 0U)

    CHECK_ERROR_STR_STARTS_WITH_ERROR_NAME(CANCELLED);
    CHECK_ERROR_STR_STARTS_WITH_ERROR_NAME(INVALID_ARGS);
    CHECK_ERROR_STR_STARTS_WITH_ERROR_NAME(INVALID_COMMAND);
    CHECK_ERROR_STR_STARTS_WITH_ERROR_NAME(TIMEOUT);
    CHECK_ERROR_STR_STARTS_WITH_ERROR_NAME(NOT_FOUND);
    CHECK_ERROR_STR_STARTS_WITH_ERROR_NAME(SECURITY);
    CHECK_ERROR_STR_STARTS_WITH_ERROR_NAME(UNIMPLEMENTED);
    CHECK_ERROR_STR_STARTS_WITH_ERROR_NAME(BAD_FORMAT);
    CHECK_ERROR_STR_STARTS_WITH_ERROR_NAME(BUSY);
    CHECK_ERROR_STR_STARTS_WITH_ERROR_NAME(OUT_OF_MEMORY);
    CHECK_ERROR_STR_STARTS_WITH_ERROR_NAME(IO_ERROR);
    CHECK_ERROR_STR_STARTS_WITH_ERROR_NAME(IO_BUSY);
    CHECK_ERROR_STR_STARTS_WITH_ERROR_NAME(ALREADY_EXISTS);
    CHECK_ERROR_STR_STARTS_WITH_ERROR_NAME(ABORTED);
    CHECK_ERROR_STR_STARTS_WITH_ERROR_NAME(INVALID_STATE);
    CHECK_ERROR_STR_STARTS_WITH_ERROR_NAME(REJECTED);
    CHECK_ERROR_STR_STARTS_WITH_ERROR_NAME(COAP_ERROR);
    CHECK_ERROR_STR_STARTS_WITH_ERROR_NAME(UNKNOWN);

#undef CHECK_ERROR_STR_STARTS_WITH_ERROR_NAME
}

TEST(ErrorTest, ErrorComparison_DefaultErrorsEqualToEachOther)
{
    Error lhs;
    Error rhs;

    EXPECT_EQ(lhs, rhs);
}

TEST(ErrorTest, ErrorComparison_DefaultErrorEqualToErrorNone)
{
    Error error;

    EXPECT_EQ(error, ErrorCode::kNone);
}

} // namespace commissioner

} // namespace ot
