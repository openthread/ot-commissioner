/*
 *    Copyright (c) 2021, The OpenThread Commissioner Authors.
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
 *   This file defines test cases for file utils.
 */

#include "app/file_util.hpp"

#include <catch2/catch.hpp>

namespace ot {

namespace commissioner {

TEST_CASE("SplitPath", "[file_util]")
{
    std::string path;
    std::string dirName  = "123";
    std::string baseName = "456";

    SECTION("directory and file")
    {
        path = "dir/ect/ory/file";
        SplitPath(path, dirName, baseName);
        REQUIRE(dirName == std::string("dir/ect/ory/"));
        REQUIRE(baseName == "file");
    }

    SECTION("file only")
    {
        path = "file.name";
        SplitPath(path, dirName, baseName);
        REQUIRE(dirName.empty());
        REQUIRE(baseName == path);
    }

    SECTION("pure directory")
    {
        path = "dir/ect/ory/";
        SplitPath(path, dirName, baseName);
        REQUIRE(dirName == path);
        REQUIRE(baseName.empty());
    }
}

TEST_CASE("PathExists", "[file_util]")
{
    REQUIRE(PathExists("./").GetCode() == ErrorCode::kNone);
}

} // namespace commissioner

} // namespace ot
