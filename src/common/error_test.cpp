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
    SECTION("default Error object should return 'NONE' as string representation")
    {
        Error error;

        REQUIRE(error.ToString() == "NONE");
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
