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
 *   This file tests CommissionerImpl.
 *
 *   This file includes only test cases for APIs which cannot be tested
 *   through CommissionerSafe. The rest will go to CommissionerSafe test file.
 */

#include "library/commissioner_impl.hpp"

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "common/utils.hpp"

namespace ot {

namespace commissioner {

TEST_CASE("mesh-local-address-basic", "[mesh-local-addr]")
{
    std::string meshLocalAddr;
    REQUIRE(Commissioner::GetMeshLocalAddr(meshLocalAddr, "fd00::/64", 0xBBCC) == ErrorCode::kNone);
    REQUIRE(meshLocalAddr == "fd00::ff:fe00:bbcc");
}

TEST_CASE("mesh-local-address-invalid-args", "[mesh-local-addr]")
{
    std::string meshLocalAddr;

    SECTION("invalid prefix length")
    {
        REQUIRE(Commissioner::GetMeshLocalAddr(meshLocalAddr, "fd00::/63", 0xBBCC).GetCode() ==
                ErrorCode::kInvalidArgs);
    }

    SECTION("prefix length is not 8 bytes")
    {
        REQUIRE(Commissioner::GetMeshLocalAddr(meshLocalAddr, "fd00::/48", 0xBBCC).GetCode() ==
                ErrorCode::kInvalidArgs);
    }

    SECTION("invalid prefix format")
    {
        REQUIRE(Commissioner::GetMeshLocalAddr(meshLocalAddr, "fd00::48", 0xBBCC) == ErrorCode::kInvalidArgs);
    }
}

// This teat case is from section 8.4.1.2.2 of the Thread 1.2.0 specification.
TEST_CASE("pskc-test-vector-from-thread-1.2.0-spec", "[pskc]")
{
    const std::string passphrase    = "12SECRETPASSWORD34";
    const std::string networkName   = "Test Network";
    const ByteArray   extendedPanId = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    ByteArray         pskc;

    REQUIRE(Commissioner::GeneratePSKc(pskc, passphrase, networkName, extendedPanId) == ErrorCode::kNone);
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

        REQUIRE(Commissioner::GeneratePSKc(pskc, passphrase, networkName, extendedPanId).GetCode() ==
                ErrorCode::kInvalidArgs);
    }

    SECTION("passphrase is too long")
    {
        const std::string passphrase(256, '1');
        const std::string networkName   = "Test Network";
        const ByteArray   extendedPanId = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
        ByteArray         pskc;

        REQUIRE(Commissioner::GeneratePSKc(pskc, passphrase, networkName, extendedPanId).GetCode() ==
                ErrorCode::kInvalidArgs);
    }

    SECTION("network name is too long")
    {
        const std::string passphrase    = "12SECRETPASSWORD34";
        const std::string networkName   = "Too Long network name";
        const ByteArray   extendedPanId = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
        ByteArray         pskc;

        REQUIRE(Commissioner::GeneratePSKc(pskc, passphrase, networkName, extendedPanId).GetCode() ==
                ErrorCode::kInvalidArgs);
    }
}

TEST_CASE("commissioner-impl-not-implemented-APIs", "[comm-impl]")
{
    static const std::string kDstAddr = "fd00:7d03:7d03:7d03:d020:79b7:6a02:ab5e";

    Config config;
    config.mEnableCcm = false;
    config.mPSKc = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};

    CommissionerHandler dummyHandler;
    struct event_base * eventBase = event_base_new();
    CommissionerImpl    commImpl(dummyHandler, eventBase);
    REQUIRE(commImpl.Init(config) == ErrorCode::kNone);

    REQUIRE(commImpl.Connect("::1", 5684) == ErrorCode::kUnimplemented);

    std::string existingCommissionerId;
    REQUIRE(commImpl.Petition(existingCommissionerId, "::1", 5684) == ErrorCode::kUnimplemented);
    REQUIRE(commImpl.Resign() == ErrorCode::kUnimplemented);

    CommissionerDataset commDataset;
    REQUIRE(commImpl.GetCommissionerDataset(commDataset, 0xFFFF) == ErrorCode::kUnimplemented);
    REQUIRE(commImpl.SetCommissionerDataset({}) == ErrorCode::kUnimplemented);

    BbrDataset bbrDataset;
    REQUIRE(commImpl.GetBbrDataset(bbrDataset, 0xFFFF) == ErrorCode::kUnimplemented);
    REQUIRE(commImpl.SetBbrDataset({}) == ErrorCode::kUnimplemented);

    ActiveOperationalDataset activeDataset;
    REQUIRE(commImpl.GetActiveDataset(activeDataset, 0xFFFF) == ErrorCode::kUnimplemented);
    REQUIRE(commImpl.SetActiveDataset({}) == ErrorCode::kUnimplemented);

    PendingOperationalDataset pendingDataset;
    REQUIRE(commImpl.GetPendingDataset(pendingDataset, 0xFFFF) == ErrorCode::kUnimplemented);
    REQUIRE(commImpl.SetPendingDataset({}) == ErrorCode::kUnimplemented);
    REQUIRE(commImpl.SetSecurePendingDataset(kDstAddr, 30, {}) == ErrorCode::kUnimplemented);

    REQUIRE(commImpl.CommandReenroll(kDstAddr) == ErrorCode::kUnimplemented);
    REQUIRE(commImpl.CommandDomainReset(kDstAddr) == ErrorCode::kUnimplemented);
    REQUIRE(commImpl.CommandMigrate(kDstAddr, "designated-net") == ErrorCode::kUnimplemented);

    uint8_t mlrStatus;
    REQUIRE(commImpl.RegisterMulticastListener(mlrStatus, kDstAddr, {"ff02::9"}, 300).GetCode() ==
            ErrorCode::kUnimplemented);

    REQUIRE(commImpl.AnnounceBegin(0xFFFFFFFF, 10, 10, kDstAddr) == ErrorCode::kUnimplemented);
    REQUIRE(commImpl.PanIdQuery(0xFFFFFFFF, 0xFACE, kDstAddr) == ErrorCode::kUnimplemented);
    REQUIRE(commImpl.EnergyScan(0xFFFFFFFF, 10, 10, 20, kDstAddr) == ErrorCode::kUnimplemented);

    ByteArray signedToken;
    REQUIRE(commImpl.RequestToken(signedToken, "fdaa:bb::de6", 5684) == ErrorCode::kUnimplemented);

    event_base_free(eventBase);
}

} // namespace commissioner

} // namespace ot
