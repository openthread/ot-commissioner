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
        "b212df1009601804601d046019041e22c818fdc31ff45feff4e7e580431c60becfabfd110022000000008df846f3ab0c05551e22c802fd"
        "c31ff45feff4e75257420f1cbd46f5fd1100220000000034e5d9e28d1952c0077d030e0007fc0109e400108400109c000003140040fd27"
        "fd30e5ce0001070212400504e400f1000b0e8001010d09e4000a000500000e100b0881025cf40d029c0003130060fd6b51760904ffff00"
        "00000001039c00e00b1982015d0d149c00fd27fd30e5ce00018e250585edd6f1b0e5ec080b090284000b028dbc08010003040000012C04"
        "0A0105123456789ABCDEF00E01640F021388110401020304120505060708A013040000025818020005190A56656E646F724E616D651A0B"
        "56656E646F724D6F64656C1B0D56656E646F7253574D6F64656C1C12546872656164537461636B56657273696F6E";

    error = utils::Hex(buf, tlvsHexString);
    EXPECT_EQ(error, ErrorCode::kNone);

    error = ot::commissioner::internal::DecodeNetDiagData(diagData, buf);
    EXPECT_EQ(error, ErrorCode::kNone);

    ByteArray extMacAddrBytes;
    uint16_t  macAddr = 0xc800;
    error             = utils::Hex(extMacAddrBytes, "6ac6c2de12b212df");

    ByteArray channelPagesBytes;
    error = utils::Hex(channelPagesBytes, "01020304");
    EXPECT_EQ(error, ErrorCode::kNone);

    ByteArray typeListBytes;
    error = utils::Hex(typeListBytes, "05060708A0");
    EXPECT_EQ(error, ErrorCode::kNone);

    uint8_t   batteryLevel 			= 0x64;
    uint16_t  supplyVoltage 		= 0x1388;
    uint16_t  version		 		= 0x05;
    uint32_t  timeout 				= 0x12C;
    uint32_t  maxChildTimeout 		= 0x258;
    std::string vendorName          = "VendorName";
    std::string vendorModel         = "VendorModel";
    std::string vendorSWVersion     = "VendorSWModel";
    std::string threadStackVersion  = "ThreadStackVersion";

    EXPECT_EQ(error, ErrorCode::kNone);
    EXPECT_EQ(diagData.mPresentFlags, 8388223);
    EXPECT_EQ(diagData.mExtMacAddr, extMacAddrBytes);
    EXPECT_EQ(diagData.mMacAddr, macAddr);
    EXPECT_EQ(diagData.mTimeout, timeout);
    EXPECT_EQ(diagData.mBatteryLevel, batteryLevel);
    EXPECT_EQ(diagData.mVendorName, vendorName);
    EXPECT_EQ(diagData.mVendorModel, vendorModel);
    EXPECT_EQ(diagData.mSupplyVoltage, supplyVoltage);
    EXPECT_EQ(diagData.mChannelPages, channelPagesBytes);
    EXPECT_EQ(diagData.mTypeList, typeListBytes);
    EXPECT_EQ(diagData.mMaxChildTimeout, maxChildTimeout);
    EXPECT_EQ(diagData.mVersion, version);
    EXPECT_EQ(diagData.mVendorSWVersion, vendorSWVersion);
    EXPECT_EQ(diagData.mThreadStackVersion, threadStackVersion);
    EXPECT_EQ(diagData.mMode.mIsMtd, false);
    EXPECT_EQ(diagData.mRoute64.mRouteData.size(), 9);
    EXPECT_EQ(diagData.mAddrs.size(), 4);
    EXPECT_EQ(diagData.mAddrs[0], "fd92:38a3:395d:1:f904:3dfe:b7b3:edf3");
    EXPECT_EQ(diagData.mChildTable.size(), 3);
    EXPECT_EQ(diagData.mChildTable[0].mChildId, 24);
    EXPECT_EQ(diagData.mLeaderData.mRouterId, 33);
    EXPECT_EQ(diagData.mChildIpv6AddrsInfoList.size(), 2);
    EXPECT_EQ(diagData.mChildIpv6AddrsInfoList[0].mRloc16, 51224);
    EXPECT_EQ(diagData.mChildIpv6AddrsInfoList[0].mChildId, 24);
    EXPECT_EQ(diagData.mChildIpv6AddrsInfoList[0].mAddrs[0], "fdc3:1ff4:5fef:f4e7:e580:431c:60be:cfab");
    EXPECT_EQ(diagData.mChildIpv6AddrsInfoList[0].mAddrs[1], "fd11:22::8df8:46f3:ab0c:555");
    EXPECT_EQ(diagData.mChildIpv6AddrsInfoList[1].mRloc16, 51202);
    EXPECT_EQ(diagData.mChildIpv6AddrsInfoList[1].mChildId, 2);
    EXPECT_EQ(diagData.mChildIpv6AddrsInfoList[1].mAddrs[0], "fdc3:1ff4:5fef:f4e7:5257:420f:1cbd:46f5");
    EXPECT_EQ(diagData.mChildIpv6AddrsInfoList[1].mAddrs[1], "fd11:22::34e5:d9e2:8d19:52c0");
    // Parsing prefix TLV in Network Data
    EXPECT_EQ(diagData.mNetworkData.mPrefixList.size(), 3);
    EXPECT_EQ(diagData.mNetworkData.mPrefixList[0].mDomainId, 0);
    EXPECT_EQ(diagData.mNetworkData.mPrefixList[0].mPrefixLength, 1);
    EXPECT_EQ(diagData.mNetworkData.mPrefixList[0].mHasRouteList.size(), 3);
    EXPECT_EQ(diagData.mNetworkData.mPrefixList[0].mHasRouteList[0].mRloc16, 58368);
    EXPECT_EQ(diagData.mNetworkData.mPrefixList[0].mHasRouteList[0].mRouterPreference, 0);
    EXPECT_EQ(diagData.mNetworkData.mPrefixList[0].mHasRouteList[0].mIsNat64, false);
    EXPECT_EQ(diagData.mNetworkData.mPrefixList[0].mHasRouteList[1].mRloc16, 33792);
    EXPECT_EQ(diagData.mNetworkData.mPrefixList[0].mHasRouteList[1].mRouterPreference, 0);
    EXPECT_EQ(diagData.mNetworkData.mPrefixList[0].mHasRouteList[1].mIsNat64, false);
    EXPECT_EQ(diagData.mNetworkData.mPrefixList[0].mHasRouteList[2].mRloc16, 39936);
    EXPECT_EQ(diagData.mNetworkData.mPrefixList[0].mHasRouteList[2].mRouterPreference, 0);
    EXPECT_EQ(diagData.mNetworkData.mPrefixList[0].mHasRouteList[2].mIsNat64, false);
    EXPECT_EQ(diagData.mNetworkData.mPrefixList[1].mBorderRouterList.size(), 1);
    EXPECT_EQ(diagData.mNetworkData.mPrefixList[1].mBorderRouterList[0].mRloc16, 58368);
    EXPECT_EQ(diagData.mNetworkData.mPrefixList[1].mBorderRouterList[0].mPrefixPreference, 3);
    EXPECT_EQ(diagData.mNetworkData.mPrefixList[1].mBorderRouterList[0].mIsPreferred, true);
    EXPECT_EQ(diagData.mNetworkData.mPrefixList[1].mBorderRouterList[0].mIsSlaac, true);
    EXPECT_EQ(diagData.mNetworkData.mPrefixList[1].mBorderRouterList[0].mIsDhcp, false);
    EXPECT_EQ(diagData.mNetworkData.mPrefixList[1].mBorderRouterList[0].mIsConfigure, false);
    EXPECT_EQ(diagData.mNetworkData.mPrefixList[1].mBorderRouterList[0].mIsDefaultRoute, false);
    EXPECT_EQ(diagData.mNetworkData.mPrefixList[1].mBorderRouterList[0].mIsOnMesh, true);
    EXPECT_EQ(diagData.mNetworkData.mPrefixList[1].mBorderRouterList[0].mIsNdDns, false);
    EXPECT_EQ(diagData.mNetworkData.mPrefixList[1].mBorderRouterList[0].mIsDp, false);
    EXPECT_EQ(diagData.mNetworkData.mPrefixList[1].mSixLowPanContext.mIsCompress, true);
    EXPECT_EQ(diagData.mNetworkData.mPrefixList[1].mSixLowPanContext.mContextId, 2);
    EXPECT_EQ(diagData.mNetworkData.mPrefixList[1].mSixLowPanContext.mContextLength, 64);
    EXPECT_EQ(diagData.mNetworkData.mPrefixList[2].mDomainId, 0);
    EXPECT_EQ(diagData.mNetworkData.mPrefixList[2].mPrefixLength, 12);
    EXPECT_EQ(diagData.mNetworkData.mPrefixList[2].mHasRouteList.size(), 1);
    EXPECT_EQ(diagData.mNetworkData.mPrefixList[2].mHasRouteList[0].mRloc16, 39936);

    // Connectivity TLV data
    const auto &connectivity = diagData.mConnectivity;
    EXPECT_EQ(connectivity.mParentPriority, 1);
    EXPECT_EQ(connectivity.mLinkQuality3, 5);
    EXPECT_EQ(connectivity.mLinkQuality2, 0x12);
    EXPECT_EQ(connectivity.mLinkQuality1, 0x34);
    EXPECT_EQ(connectivity.mLeaderCost, 0x56);
    EXPECT_EQ(connectivity.mIdSequence, 0x78);
    EXPECT_EQ(connectivity.mActiveRouters, 0x9A);
    EXPECT_EQ(connectivity.mRxOffChildBufferSize, 0xBCDE);
    EXPECT_EQ(connectivity.mRxOffChildDatagramCount, 0xF0);
    // Verify that the presence flags for the optional fields within the Connectivity struct have been set.
    uint16_t expectedConnFlags = Connectivity::kRxOffChildBufferSizeBit | Connectivity::kRxOffChildDatagramCountBit;
    EXPECT_EQ(connectivity.mPresentFlags, expectedConnFlags);
}

TEST(CommissionerImplTest, DecodeConnectivityTlv)
{
    // // Test Case 1: All fields present (10 bytes total).
    {
        // Byte 0: PP=1 (binary 01) -> 0x01
        // Byte 1: LQ3=5 -> 0x05
        // Bytes 7-8: Buffer Size = 1024 -> 0x0400
        ByteArray buf = {
            0x01,       // Parent Priority
            0x05,       // Link Quality 3
            0x02,       // Link Quality 2
            0x03,       // Link Quality 1
            0xFA,       // Leader Cost
            0x1B,       // ID Sequence
            0x0C,       // Active Routers
            0x04, 0x00, // Rx-off Child Buffer Size
            0x0F,       // Rx-off Child Datagram Count
        };
        Connectivity connectivity;
        Error        error = ot::commissioner::internal::DecodeConnectivity(connectivity, buf);

        EXPECT_EQ(error, ErrorCode::kNone);
        EXPECT_TRUE(connectivity.mPresentFlags & Connectivity::kRxOffChildBufferSizeBit);
        EXPECT_TRUE(connectivity.mPresentFlags & Connectivity::kRxOffChildDatagramCountBit);
        EXPECT_EQ(connectivity.mParentPriority, 1);
        EXPECT_EQ(connectivity.mLinkQuality3, 5);
        EXPECT_EQ(connectivity.mLinkQuality2, 2);
        EXPECT_EQ(connectivity.mLinkQuality1, 3);
        EXPECT_EQ(connectivity.mLeaderCost, 250);
        EXPECT_EQ(connectivity.mIdSequence, 0x1B);
        EXPECT_EQ(connectivity.mActiveRouters, 12);
        EXPECT_EQ(connectivity.mRxOffChildBufferSize, 1024);
        EXPECT_EQ(connectivity.mRxOffChildDatagramCount, 15);
    }

    // Test Case 2: Mandatory fields only (7 bytes total).
    {
        // Byte 0: PP=0 (binary 00) -> 0x00
        // Byte 1: LQ3=3 -> 0x03
        ByteArray buf = {
            0x00, // Parent Priority
            0x03, // Link Quality 3
            0xFF, // Link Quality 2
            0xFE, // Link Quality 1
            0xFD, // Leader Cost
            0xFC, // ID Sequence
            0xFB, // Active Routers
        };
        Connectivity connectivity;
        Error        error = ot::commissioner::internal::DecodeConnectivity(connectivity, buf);

        EXPECT_EQ(error, ErrorCode::kNone);
        EXPECT_FALSE(connectivity.mPresentFlags & Connectivity::kRxOffChildBufferSizeBit);
        EXPECT_FALSE(connectivity.mPresentFlags & Connectivity::kRxOffChildDatagramCountBit);
        EXPECT_EQ(connectivity.mParentPriority, 0);
        EXPECT_EQ(connectivity.mLinkQuality3, 3);
        EXPECT_EQ(connectivity.mLinkQuality2, 255);
        EXPECT_EQ(connectivity.mLinkQuality1, 254);
        EXPECT_EQ(connectivity.mLeaderCost, 253);
        EXPECT_EQ(connectivity.mIdSequence, 252);
        EXPECT_EQ(connectivity.mActiveRouters, 251);
    }

    // Test Case 3: Malformed TLV (too short).
    {
        ByteArray    buf = {0x01, 0x02, 0x03, 0x04, 0x05}; // Only 5 bytes
        Connectivity connectivity;
        Error        error = ot::commissioner::internal::DecodeConnectivity(connectivity, buf);

        EXPECT_EQ(error.GetCode(), ErrorCode::kBadFormat);
    }

    // Test Case 4: Malformed TLV (extra byte at the end).
    {
        ByteArray buf = {
            0x01, 0x05, 0x02, 0x03, 0xFA, 0x1B, 0x0C, 0x04, 0x00, 0x0F,
        	0xEE, // Extra byte
        };
        Connectivity connectivity;
        Error        error = ot::commissioner::internal::DecodeConnectivity(connectivity, buf);

        EXPECT_EQ(error.GetCode(), ErrorCode::kBadFormat);
    }
}

} // namespace commissioner

} // namespace ot
