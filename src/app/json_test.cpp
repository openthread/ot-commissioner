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
 *   This file defines test cases for JSON.
 */

#include "app/json.hpp"

#include <gtest/gtest.h>

#include "cli/console.hpp"
#include "commissioner/network_data.hpp"
#include "common/utils.hpp"

#define INFO(str) Console::Write(str)

namespace ot {

namespace commissioner {

TEST(Json, ActiveOperationalDatasetEncodingDecoding)
{
    const std::string        kNetworkMasterKey = "0123456789abcdef0123456789abcdef";
    ActiveOperationalDataset dataset;

    // network master key serialization & deserialization
    {
        EXPECT_TRUE(utils::Hex(dataset.mNetworkMasterKey, kNetworkMasterKey) == ErrorCode::kNone);
        dataset.mPresentFlags |= ActiveOperationalDataset::kNetworkMasterKeyBit;

        std::string json = ActiveDatasetToJson(dataset);

        ActiveOperationalDataset dataset1;
        EXPECT_TRUE(ActiveDatasetFromJson(dataset1, json) == ErrorCode::kNone);

        EXPECT_TRUE((dataset1.mPresentFlags & ActiveOperationalDataset::kNetworkMasterKeyBit));
        EXPECT_TRUE(utils::Hex(dataset1.mNetworkMasterKey) == kNetworkMasterKey);
    }

    // security policy serialization & deserialization
    {
        const ByteArray kSecurityPolicyFlags  = {0x05, 0xff};
        dataset.mSecurityPolicy.mRotationTime = 32;
        dataset.mSecurityPolicy.mFlags        = kSecurityPolicyFlags;
        dataset.mPresentFlags |= ActiveOperationalDataset::kSecurityPolicyBit;

        std::string              json = ActiveDatasetToJson(dataset);
        ActiveOperationalDataset dataset1;
        INFO(json);
        EXPECT_TRUE(ActiveDatasetFromJson(dataset1, json) == ErrorCode::kNone);

        EXPECT_TRUE((dataset1.mPresentFlags & ActiveOperationalDataset::kSecurityPolicyBit));
        EXPECT_EQ(dataset1.mSecurityPolicy.mFlags, kSecurityPolicyFlags);
    }

    // channel mask serialization & deserialization
    {
        dataset.mChannelMask = {{1, {0xFF, 0xEE}}};
        dataset.mPresentFlags |= ActiveOperationalDataset::kChannelMaskBit;

        std::string              json = ActiveDatasetToJson(dataset);
        ActiveOperationalDataset dataset1;
        INFO(json);
        EXPECT_TRUE(ActiveDatasetFromJson(dataset1, json) == ErrorCode::kNone);

        EXPECT_TRUE((dataset1.mPresentFlags & ActiveOperationalDataset::kChannelMaskBit));
        EXPECT_TRUE(dataset1.mChannelMask.size() == 1);
        EXPECT_TRUE(dataset1.mChannelMask[0].mPage == 1);
        EXPECT_TRUE(dataset1.mChannelMask[0].mMasks == dataset.mChannelMask[0].mMasks);
    }
}

} // namespace commissioner

} // namespace ot
