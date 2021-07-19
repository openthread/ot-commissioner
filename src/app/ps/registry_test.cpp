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
 *   The file implements registry test suite.
 */

#include <gtest/gtest.h>

#include "registry.hpp"

#include <vector>

#include <unistd.h>

#include "app/cli/console.hpp"

#define INFO(str) Console::Write(str)

using namespace ot::commissioner::persistent_storage;
using namespace ot::commissioner;

const char json_path[] = "./registry_test.json";

TEST(RegJson, CreateEmptyRegistry)
{
    unlink(json_path);
    Registry reg(json_path);

    EXPECT_TRUE(reg.Open() == Registry::Status::kSuccess);
    EXPECT_TRUE(reg.Close() == Registry::Status::kSuccess);
}

TEST(RegJson, CreateBorderRouterFromBorderAgent)
{
    unlink(json_path);
    Registry reg(json_path);

    EXPECT_TRUE(reg.Open() == Registry::Status::kSuccess);

    // Create BorderRouter with network
    {
        BorderAgent ba{"1.1.1.1",
                       1,
                       ByteArray{},
                       "",
                       BorderAgent::State{0},
                       "net1",
                       0x0101010101010101,
                       "",
                       "",
                       Timestamp{},
                       0,
                       "",
                       ByteArray{},
                       "",
                       0,
                       0,
                       "",
                       0,
                       BorderAgent::kAddrBit | BorderAgent::kPortBit | BorderAgent::kNetworkNameBit |
                           BorderAgent::kExtendedPanIdBit};
        EXPECT_TRUE(reg.Add(ba) == Registry::Status::kSuccess);
        BorderRouter ret_val;
        EXPECT_TRUE(reg.GetBorderRouter(BorderRouterId{0}, ret_val) == Registry::Status::kSuccess);
        EXPECT_TRUE(ret_val.mNetworkId.mId == 0);
        Network nwk;
        EXPECT_TRUE(reg.GetNetworkByXpan(XpanId{0}, nwk) == Registry::Status::kSuccess);
        EXPECT_TRUE(nwk.mDomainId.mId == EMPTY_ID);
    }

    // Create BorderRouter with network and domain
    {
        BorderAgent ba{"1.1.1.2",
                       2,
                       ByteArray{},
                       "",
                       BorderAgent::State{0},
                       "net2",
                       0x0202020202020202,
                       "",
                       "",
                       Timestamp{},
                       0,
                       "",
                       ByteArray{},
                       "dom2",
                       0,
                       0,
                       "",
                       0,
                       BorderAgent::kAddrBit | BorderAgent::kPortBit | BorderAgent::kNetworkNameBit |
                           BorderAgent::kExtendedPanIdBit | BorderAgent::kDomainNameBit};
        EXPECT_TRUE(reg.Add(ba) == Registry::Status::kSuccess);

        BorderRouter ret_val;
        EXPECT_TRUE(reg.GetBorderRouter(BorderRouterId{1}, ret_val) == Registry::Status::kSuccess);
        EXPECT_TRUE(ret_val.mNetworkId.mId == 1);
        Network nwk;
        EXPECT_TRUE(reg.GetNetworkByName(ba.mNetworkName, nwk) == Registry::Status::kSuccess);
        EXPECT_TRUE(nwk.mId.mId == 1);
        EXPECT_TRUE(nwk.mDomainId.mId == 0);
        std::string dom;
        EXPECT_TRUE(reg.GetDomainNameByXpan(nwk.mXpan, dom) == Registry::Status::kSuccess);
        EXPECT_TRUE(dom == ba.mDomainName);
    }

    // Fail to create BorderRouter without network
    {
        // Modify explicitly
        BorderAgent ba{"1.1.1.1",
                       1,
                       ByteArray{},
                       "",
                       BorderAgent::State{0},
                       "",
                       0,
                       "vendorName",
                       "",
                       Timestamp{},
                       0,
                       "",
                       ByteArray{},
                       "",
                       0,
                       0,
                       "",
                       0,
                       BorderAgent::kAddrBit | BorderAgent::kPortBit | BorderAgent::kVendorNameBit};
        EXPECT_TRUE(reg.Add(ba) == Registry::Status::kError);
    }
    // Update BorderRouter fields
    {
        // Check the BorderRouter present
        BorderRouter val;
        EXPECT_TRUE(reg.GetBorderRouter(BorderRouterId{0}, val) == Registry::Status::kSuccess);
        EXPECT_TRUE((val.mAgent.mPresentFlags & (BorderAgent::kAddrBit | BorderAgent::kPortBit)) ==
                    (BorderAgent::kAddrBit | BorderAgent::kPortBit));
        EXPECT_TRUE(val.mAgent.mAddr == "1.1.1.1");
        INFO(val.mAgent.mNetworkName + " : " + XpanId(val.mAgent.mExtendedPanId).str());
        Network nwk;
        EXPECT_TRUE(reg.GetNetworkByXpan(val.mAgent.mExtendedPanId, nwk) == Registry::Status::kSuccess);
        // Modify explicitly
        BorderAgent ba{"1.1.1.1",
                       1,
                       ByteArray{},
                       "",
                       BorderAgent::State{BorderAgent::State::ConnectionMode::kVendorSpecific,
                                          BorderAgent::State::ThreadInterfaceStatus::kActive,
                                          BorderAgent::State::Availability::kHigh, 0, 0},
                       "net1",
                       0x0101010101010101,
                       "vendorName",
                       "",
                       Timestamp{},
                       0,
                       "vendorData",
                       ByteArray{},
                       "",
                       0,
                       0,
                       "",
                       0,
                       BorderAgent::kAddrBit | BorderAgent::kPortBit | BorderAgent::kStateBit |
                           BorderAgent::kNetworkNameBit | BorderAgent::kExtendedPanIdBit | BorderAgent::kVendorNameBit |
                           BorderAgent::kVendorDataBit};
        EXPECT_TRUE(reg.Add(ba) == Registry::Status::kSuccess);
        BorderRouter new_val;
        EXPECT_TRUE(reg.GetBorderRouter(BorderRouterId{0}, new_val) == Registry::Status::kSuccess);
        EXPECT_TRUE(val.mId.mId == new_val.mId.mId);
        EXPECT_TRUE(val.mNetworkId.mId == new_val.mNetworkId.mId);
        EXPECT_TRUE((new_val.mAgent.mPresentFlags & BorderAgent::kStateBit) != 0);
        EXPECT_TRUE(new_val.mAgent.mState.mConnectionMode == ba.mState.mConnectionMode);
        EXPECT_TRUE(new_val.mAgent.mState.mThreadIfStatus == ba.mState.mThreadIfStatus);
        EXPECT_TRUE(new_val.mAgent.mState.mAvailability == ba.mState.mAvailability);
        EXPECT_TRUE((new_val.mAgent.mPresentFlags & BorderAgent::kNetworkNameBit) != 0);
        EXPECT_TRUE(new_val.mAgent.mNetworkName == ba.mNetworkName);
        EXPECT_TRUE((new_val.mAgent.mPresentFlags & BorderAgent::kExtendedPanIdBit) != 0);
        EXPECT_TRUE(new_val.mAgent.mExtendedPanId == ba.mExtendedPanId);
        EXPECT_TRUE((new_val.mAgent.mPresentFlags & BorderAgent::kVendorNameBit) != 0);
        EXPECT_TRUE(new_val.mAgent.mVendorName == ba.mVendorName);
        EXPECT_TRUE((new_val.mAgent.mPresentFlags & BorderAgent::kVendorDataBit) != 0);
        EXPECT_TRUE(new_val.mAgent.mVendorData == ba.mVendorData);
    }

    // Update BorderRouter - switch network
    {
        // Modify explicitly
        BorderAgent ba{"1.1.1.1",
                       1,
                       ByteArray{},
                       "",
                       BorderAgent::State{0},
                       "net3",
                       0x0303030303030303,
                       "vendorName",
                       "",
                       Timestamp{},
                       0,
                       "",
                       ByteArray{},
                       "",
                       0,
                       0,
                       "",
                       0,
                       BorderAgent::kAddrBit | BorderAgent::kPortBit | BorderAgent::kNetworkNameBit |
                           BorderAgent::kExtendedPanIdBit | BorderAgent::kVendorNameBit};
        EXPECT_TRUE(reg.Add(ba) == Registry::Status::kSuccess);
        BorderRouter new_val;
        EXPECT_TRUE(reg.GetBorderRouter(BorderRouterId{0}, new_val) == Registry::Status::kSuccess);
        EXPECT_TRUE(new_val.mNetworkId.mId == 2);
    }

    // Update BorderRouter - add network into domain
    {
        // Modify explicitly
        BorderAgent ba{"1.1.1.1",
                       1,
                       ByteArray{},
                       "",
                       BorderAgent::State{0},
                       "net3",
                       0x0303030303030303,
                       "vendorName",
                       "",
                       Timestamp{},
                       0,
                       "",
                       ByteArray{},
                       "dom2",
                       0,
                       0,
                       "",
                       0,
                       BorderAgent::kAddrBit | BorderAgent::kPortBit | BorderAgent::kNetworkNameBit |
                           BorderAgent::kExtendedPanIdBit | BorderAgent::kVendorNameBit | BorderAgent::kDomainNameBit};
        EXPECT_TRUE(reg.Add(ba) == Registry::Status::kSuccess);
        BorderRouter new_val;
        EXPECT_TRUE(reg.GetBorderRouter(BorderRouterId{0}, new_val) == Registry::Status::kSuccess);
        EXPECT_TRUE(new_val.mNetworkId.mId == 2);
        Network nwk;
        EXPECT_TRUE(reg.GetNetworkByName(ba.mNetworkName, nwk) == Registry::Status::kSuccess);
        EXPECT_TRUE(nwk.mDomainId.mId == 0);
    }

    // Update BorderRouter - move network into another domain
    {
        // Modify explicitly
        BorderAgent ba{"1.1.1.1",
                       1,
                       ByteArray{},
                       "",
                       BorderAgent::State{0},
                       "net3",
                       0x0303030303030303,
                       "vendorName",
                       "",
                       Timestamp{},
                       0,
                       "",
                       ByteArray{},
                       "dom1", // New domain, will have id == 1
                       0,
                       0,
                       "",
                       0,
                       BorderAgent::kAddrBit | BorderAgent::kPortBit | BorderAgent::kNetworkNameBit |
                           BorderAgent::kExtendedPanIdBit | BorderAgent::kVendorNameBit | BorderAgent::kDomainNameBit};
        EXPECT_TRUE(reg.Add(ba) == Registry::Status::kSuccess);
        BorderRouter new_val;
        EXPECT_TRUE(reg.GetBorderRouter(BorderRouterId{0}, new_val) == Registry::Status::kSuccess);
        EXPECT_TRUE(new_val.mNetworkId.mId == 2);
        Network nwk;
        EXPECT_TRUE(reg.GetNetworkByName(ba.mNetworkName, nwk) == Registry::Status::kSuccess);
        EXPECT_TRUE(nwk.mDomainId.mId == 1);
    }

    EXPECT_TRUE(reg.Close() == Registry::Status::kSuccess);
}
