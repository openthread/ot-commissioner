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
#include "library/commissioner_impl_internal.hpp"

#include <gtest/gtest.h>

#include "commissioner/defines.hpp"
#include "common/utils.hpp"

namespace ot {

namespace commissioner {

// This teat case is from section 8.4.1.2.2 of the Thread 1.2.0 specification.
TEST(PskcTest, PskcTestVectorFromThread12Spec)
{
    const std::string passphrase    = "12SECRETPASSWORD34";
    const std::string networkName   = "Test Network";
    const ByteArray   extendedPanId = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    ByteArray         pskc;

    EXPECT_EQ(Commissioner::GeneratePSKc(pskc, passphrase, networkName, extendedPanId), ErrorCode::kNone);
    EXPECT_EQ(pskc.size(), kMaxPSKcLength);
    EXPECT_EQ(utils::Hex(pskc), "c3f59368445a1b6106be420a706d4cc9");
}

TEST(PskcTest, InvalidArgs_PassphraseIsTooShort)
{
    const std::string passphrase    = "12S";
    const std::string networkName   = "Test Network";
    const ByteArray   extendedPanId = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    ByteArray         pskc;

    EXPECT_EQ(Commissioner::GeneratePSKc(pskc, passphrase, networkName, extendedPanId).GetCode(),
              ErrorCode::kInvalidArgs);
}

TEST(PskcTest, InvalidArgs_PassphraseIsTooLong)
{
    const std::string passphrase(256, '1');
    const std::string networkName   = "Test Network";
    const ByteArray   extendedPanId = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    ByteArray         pskc;

    EXPECT_EQ(Commissioner::GeneratePSKc(pskc, passphrase, networkName, extendedPanId).GetCode(),
              ErrorCode::kInvalidArgs);
}

TEST(PskcTest, InvalidArgs_NetworkNameIsTooLong)
{
    const std::string passphrase    = "12SECRETPASSWORD34";
    const std::string networkName   = "Too Long network name";
    const ByteArray   extendedPanId = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    ByteArray         pskc;

    EXPECT_EQ(Commissioner::GeneratePSKc(pskc, passphrase, networkName, extendedPanId).GetCode(),
              ErrorCode::kInvalidArgs);
}

TEST(CommissionerImpl, NotImplementedApis)
{
    static const std::string kDstAddr = "fd00:7d03:7d03:7d03:d020:79b7:6a02:ab5e";

    Config config;
    config.mEnableCcm = false;
    config.mPSKc = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};

    CommissionerHandler dummyHandler;
    struct event_base  *eventBase = event_base_new();
    CommissionerImpl    commImpl(dummyHandler, eventBase);
    EXPECT_EQ(commImpl.Init(config), ErrorCode::kNone);

    EXPECT_EQ(commImpl.Connect("::1", 5684), ErrorCode::kUnimplemented);

    std::string existingCommissionerId;
    EXPECT_EQ(commImpl.Petition(existingCommissionerId, "::1", 5684), ErrorCode::kUnimplemented);
    EXPECT_EQ(commImpl.Resign(), ErrorCode::kUnimplemented);

    CommissionerDataset commDataset;
    EXPECT_EQ(commImpl.GetCommissionerDataset(commDataset, 0xFFFF), ErrorCode::kUnimplemented);
    EXPECT_EQ(commImpl.SetCommissionerDataset({}), ErrorCode::kUnimplemented);

    BbrDataset bbrDataset;
    EXPECT_EQ(commImpl.GetBbrDataset(bbrDataset, 0xFFFF), ErrorCode::kUnimplemented);
    EXPECT_EQ(commImpl.SetBbrDataset({}), ErrorCode::kUnimplemented);

    ActiveOperationalDataset activeDataset;
    EXPECT_EQ(commImpl.GetActiveDataset(activeDataset, 0xFFFF), ErrorCode::kUnimplemented);
    EXPECT_EQ(commImpl.SetActiveDataset({}), ErrorCode::kUnimplemented);

    PendingOperationalDataset pendingDataset;
    EXPECT_EQ(commImpl.GetPendingDataset(pendingDataset, 0xFFFF), ErrorCode::kUnimplemented);
    EXPECT_EQ(commImpl.SetPendingDataset({}), ErrorCode::kUnimplemented);
    EXPECT_EQ(commImpl.SetSecurePendingDataset(30, {}), ErrorCode::kUnimplemented);

    EXPECT_EQ(commImpl.CommandReenroll(kDstAddr), ErrorCode::kUnimplemented);
    EXPECT_EQ(commImpl.CommandDomainReset(kDstAddr), ErrorCode::kUnimplemented);
    EXPECT_EQ(commImpl.CommandMigrate(kDstAddr, "designated-net"), ErrorCode::kUnimplemented);

    uint8_t mlrStatus;
    EXPECT_EQ(commImpl.RegisterMulticastListener(mlrStatus, {"ff02::9"}, 300).GetCode(), ErrorCode::kUnimplemented);

    EXPECT_EQ(commImpl.AnnounceBegin(0xFFFFFFFF, 10, 10, kDstAddr), ErrorCode::kUnimplemented);
    EXPECT_EQ(commImpl.PanIdQuery(0xFFFFFFFF, 0xFACE, kDstAddr), ErrorCode::kUnimplemented);
    EXPECT_EQ(commImpl.EnergyScan(0xFFFFFFFF, 10, 10, 20, kDstAddr), ErrorCode::kUnimplemented);

    ByteArray signedToken;
    EXPECT_EQ(commImpl.RequestToken(signedToken, "fdaa:bb::de6", 5684), ErrorCode::kUnimplemented);

    event_base_free(eventBase);
}

TEST(CommissionerImplTest, ValidInput_DecodeNetDiagData)
{
    ByteArray   buf;
    NetDiagData diagData;
    Error       error;
    std::string tlvsHexString =
        "00086ac6c2de12b212df0102c80002010f0512e7000400204300300af1f1f1f1f101f1f1f10608360bb9f7415c30210840fd9238a3395d"
        "0001f9043dfeb7b3edf3fd7d604fb88a0000000000fffe00c800fd7d604fb88a0000fe3e5a4c31acb559fe8000000000000068c6c2de12"
        "b212df1009601804601d046019041e227018fdc31ff45feff4e7e580431c60becfabfd110022000000008df846f3ab0c05551e227002fd"
        "c31ff45feff4e75257420f1cbd46f5fd1100220000000034e5d9e28d1952c0";

    error = utils::Hex(buf, tlvsHexString);
    EXPECT_EQ(error, ErrorCode::kNone);

    error = ot::commissioner::internal::DecodeNetDiagData(diagData, buf);
    EXPECT_EQ(error, ErrorCode::kNone);

    ByteArray extMacAddrBytes;
    uint16_t  macAddr = 0xc800;
    error             = utils::Hex(extMacAddrBytes, "6ac6c2de12b212df");

    EXPECT_EQ(error, ErrorCode::kNone);
    EXPECT_EQ(diagData.mPresentFlags, 639);
    EXPECT_EQ(diagData.mExtMacAddr, extMacAddrBytes);
    EXPECT_EQ(diagData.mMacAddr, macAddr);
    EXPECT_EQ(diagData.mMode.mIsMtd, false);
    EXPECT_EQ(diagData.mRoute64.mRouteData.size(), 9);
    EXPECT_EQ(diagData.mAddrs.size(), 4);
    EXPECT_EQ(diagData.mAddrs[0], "fd92:38a3:395d:1:f904:3dfe:b7b3:edf3");
    EXPECT_EQ(diagData.mChildTable.size(), 3);
    EXPECT_EQ(diagData.mChildTable[0].mChildId, 24);
    EXPECT_EQ(diagData.mLeaderData.mRouterId, 33);
    EXPECT_EQ(diagData.mChildIpv6AddrsInfoList.size(), 2);
    EXPECT_EQ(diagData.mChildIpv6AddrsInfoList[0].mRloc16, 28696);
    EXPECT_EQ(diagData.mChildIpv6AddrsInfoList[0].mChildId, 24);
    EXPECT_EQ(diagData.mChildIpv6AddrsInfoList[1].mAddresses[0], "fdc3:1ff4:5fef:f4e7:5257:420f:1cbd:46f5");
    EXPECT_EQ(diagData.mChildIpv6AddrsInfoList[1].mChildId, 2);
}

} // namespace commissioner

} // namespace ot
