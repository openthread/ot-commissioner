/*
 *    Copyright (c) 2019, The OpenThread Authors.
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
 *   This file defines test cases of PSKc generation.
 */

#include <catch2/catch.hpp>

#include "commissioner/commissioner.hpp"

#include "common/utils.hpp"

namespace ot {

namespace commissioner {

// This teat case is from section 8.4.1.2.2 of the Thread 1.2.0 specification.
TEST_CASE("pskc-test-vector-from-thread-1.2.0-spec", "[pskc]")
{
    const std::string passphrase    = "12SECRETPASSWORD34";
    const std::string networkName   = "Test Network";
    const ByteArray   extendedPanId = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    ByteArray         pskc;

    REQUIRE(Commissioner::GeneratePSKc(pskc, passphrase, networkName, extendedPanId) == Error::kNone);
    REQUIRE(pskc.size() == kMaxPSKcLength);
    REQUIRE(utils::Hex(pskc) == "c3f59368445a1b6106be420a706d4cc9");
}

TEST_CASE("pskc-test-invalid-args", "[pskc]")
{
    SECTION("passphrase is too short")
    {
        const std::string passphrase    = "12S";
        const std::string networkName   = "Test Network";
        const ByteArray   extendedPanId = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
        ByteArray         pskc;

        REQUIRE(Commissioner::GeneratePSKc(pskc, passphrase, networkName, extendedPanId) == Error::kInvalidArgs);
    }

    SECTION("passphrase is too long")
    {
        const std::string passphrase(256, '1');
        const std::string networkName   = "Test Network";
        const ByteArray   extendedPanId = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
        ByteArray         pskc;

        REQUIRE(Commissioner::GeneratePSKc(pskc, passphrase, networkName, extendedPanId) == Error::kInvalidArgs);
    }

    SECTION("network name is too long")
    {
        const std::string passphrase    = "12SECRETPASSWORD34";
        const std::string networkName   = "Too Long network name";
        const ByteArray   extendedPanId = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
        ByteArray         pskc;

        REQUIRE(Commissioner::GeneratePSKc(pskc, passphrase, networkName, extendedPanId) == Error::kInvalidArgs);
    }
}

} // namespace commissioner

} // namespace ot
