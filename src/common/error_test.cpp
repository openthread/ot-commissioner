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

#include <catch2/catch.hpp>

namespace ot {

namespace commissioner {

TEST_CASE("error-message", "[error]")
{
    SECTION("default Error object has no error message")
    {
        Error error;

        REQUIRE(error.GetCode() == ErrorCode::kNone);
        REQUIRE(error.GetMessage().empty());
    }
}

TEST_CASE("error-to-string", "[error]")
{
    SECTION("default Error object should return 'OK' as string representation")
    {
        Error error;

        REQUIRE(error.ToString() == "OK");
    }

    SECTION("non-OK errors return its string representation (not required by the class Error API)")
    {
// Check if the error string starts with the error name.
#define CHECK_ERROR_STR(aErrorName) REQUIRE(ERROR_##aErrorName("").ToString().find(#aErrorName) == 0)

        CHECK_ERROR_STR(CANCELLED);
        CHECK_ERROR_STR(INVALID_ARGS);
        CHECK_ERROR_STR(INVALID_COMMAND);
        CHECK_ERROR_STR(TIMEOUT);
        CHECK_ERROR_STR(NOT_FOUND);
        CHECK_ERROR_STR(SECURITY);
        CHECK_ERROR_STR(UNIMPLEMENTED);
        CHECK_ERROR_STR(BAD_FORMAT);
        CHECK_ERROR_STR(BUSY);
        CHECK_ERROR_STR(OUT_OF_MEMORY);
        CHECK_ERROR_STR(IO_ERROR);
        CHECK_ERROR_STR(IO_BUSY);
        CHECK_ERROR_STR(ALREADY_EXISTS);
        CHECK_ERROR_STR(ABORTED);
        CHECK_ERROR_STR(INVALID_STATE);
        CHECK_ERROR_STR(REJECTED);
        CHECK_ERROR_STR(UNKNOWN);

#undef CHECK_ERROR_STR
    }
}

TEST_CASE("error-comparison", "[error]")
{
    SECTION("default Error objects should be equal to each other")
    {
        Error lhs;
        Error rhs;

        REQUIRE(lhs == rhs);
    }

    SECTION("default Error object should be equal to ErrorCode::kNone")
    {
        Error error;

        REQUIRE(error == ErrorCode::kNone);
    }
}

} // namespace commissioner

} // namespace ot
