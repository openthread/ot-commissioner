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
 *   This file tests CommissionerImpl.
 *
 *   This file includes only test cases for APIs which cannot be tested
 *   through CommissionerSafe. The rest will go to CommissionerSafe test file.
 */

#include <catch2/catch.hpp>

#include <commissioner_impl.hpp>
#include <utils.hpp>

namespace ot {

namespace commissioner {

TEST_CASE("commissioner-impl-not-implemented-APIs", "[comm-impl]")
{
    static const std::string kDstAddr = "fd00:7d03:7d03:7d03:d020:79b7:6a02:ab5e";

    Config config;
    config.mEnableCcm = false;
    config.mPSKc = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};

    struct event_base *eventBase = event_base_new();
    CommissionerImpl   commImpl(eventBase);
    REQUIRE(commImpl.Init(config) == Error::kNone);

    std::list<BorderAgent> baList;
    REQUIRE(commImpl.Discover(baList) == Error::kNotImplemented);
    REQUIRE(commImpl.Connect("::1", 5684) == Error::kNotImplemented);
    REQUIRE(commImpl.Petition("::1", 5684) == Error::kNotImplemented);
    REQUIRE(commImpl.Resign() == Error::kNotImplemented);

    CommissionerDataset commDataset;
    REQUIRE(commImpl.GetCommissionerDataset(commDataset, 0xFFFF) == Error::kNotImplemented);
    REQUIRE(commImpl.SetCommissionerDataset({}) == Error::kNotImplemented);

    BbrDataset bbrDataset;
    REQUIRE(commImpl.GetBbrDataset(bbrDataset, 0xFFFF) == Error::kNotImplemented);
    REQUIRE(commImpl.SetBbrDataset({}) == Error::kNotImplemented);

    ActiveOperationalDataset activeDataset;
    REQUIRE(commImpl.GetActiveDataset(activeDataset, 0xFFFF) == Error::kNotImplemented);
    REQUIRE(commImpl.SetActiveDataset({}) == Error::kNotImplemented);

    PendingOperationalDataset pendingDataset;
    REQUIRE(commImpl.GetPendingDataset(pendingDataset, 0xFFFF) == Error::kNotImplemented);
    REQUIRE(commImpl.SetPendingDataset({}) == Error::kNotImplemented);
    REQUIRE(commImpl.SetSecurePendingDataset(kDstAddr, 30, {}) == Error::kNotImplemented);

    REQUIRE(commImpl.CommandReenroll(kDstAddr) == Error::kNotImplemented);
    REQUIRE(commImpl.CommandDomainReset(kDstAddr) == Error::kNotImplemented);
    REQUIRE(commImpl.CommandMigrate(kDstAddr, "designated-net") == Error::kNotImplemented);

    uint8_t mlrStatus;
    REQUIRE(commImpl.RegisterMulticastListener(mlrStatus, kDstAddr, {"ff02::9"}, 300) == Error::kNotImplemented);

    REQUIRE(commImpl.AnnounceBegin(0xFFFFFFFF, 10, 10, kDstAddr) == Error::kNotImplemented);
    REQUIRE(commImpl.PanIdQuery(0xFFFFFFFF, 0xFACE, kDstAddr) == Error::kNotImplemented);
    REQUIRE(commImpl.EnergyScan(0xFFFFFFFF, 10, 10, 20, kDstAddr) == Error::kNotImplemented);

    ByteArray signedToken;
    REQUIRE(commImpl.RequestToken(signedToken, "fdaa:bb::de6", 5684) == Error::kNotImplemented);

    event_base_free(eventBase);
}

} // namespace commissioner

} // namespace ot
