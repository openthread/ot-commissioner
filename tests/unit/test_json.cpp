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
 *   This file defines test cases for JSON.
 */

#include <catch2/catch.hpp>

#include <json.hpp>
#include <utils.hpp>
#include <commissioner/network_data.hpp>

namespace ot {

namespace commissioner {

TEST_CASE("active-operational-dataset-encoding-decoding", "[json]")
{
    const std::string        kNetworkMasterKey = "0123456789abcdef0123456789abcdef";
    ActiveOperationalDataset dataset;

    SECTION("network master key serialization & deserialization")
    {
        REQUIRE(utils::Hex(dataset.mNetworkMasterKey, kNetworkMasterKey) == Error::kNone);
        dataset.mPresentFlags |= ActiveOperationalDataset::kNetworkMasterKeyBit;

        std::string json = ActiveDatasetToJson(dataset);

        ActiveOperationalDataset dataset1;
        REQUIRE(ActiveDatasetFromJson(dataset1, json) == Error::kNone);

        REQUIRE((dataset1.mPresentFlags & ActiveOperationalDataset::kNetworkMasterKeyBit));
        REQUIRE(utils::Hex(dataset1.mNetworkMasterKey) == kNetworkMasterKey);
    }

    SECTION("security policy serialization & deserialization")
    {
        const ByteArray kSecurityPolicyFlags  = {0x05, 0xff};
        dataset.mSecurityPolicy.mRotationTime = 32;
        dataset.mSecurityPolicy.mFlags        = kSecurityPolicyFlags;
        dataset.mPresentFlags |= ActiveOperationalDataset::kSecurityPolicyBit;

        std::string              json = ActiveDatasetToJson(dataset);
        ActiveOperationalDataset dataset1;
        INFO(json);
        REQUIRE(ActiveDatasetFromJson(dataset1, json) == Error::kNone);

        REQUIRE((dataset1.mPresentFlags & ActiveOperationalDataset::kSecurityPolicyBit));
        REQUIRE(dataset1.mSecurityPolicy.mFlags == ByteArray{0x05, 0xff});
    }

    SECTION("channel mask serialization & deserialization")
    {
        dataset.mChannelMask = {{1, {0xFF, 0xEE}}};
        dataset.mPresentFlags |= ActiveOperationalDataset::kChannelMaskBit;

        std::string              json = ActiveDatasetToJson(dataset);
        ActiveOperationalDataset dataset1;
        REQUIRE(ActiveDatasetFromJson(dataset1, json) == Error::kNone);

        REQUIRE((dataset1.mPresentFlags & ActiveOperationalDataset::kChannelMaskBit));
        REQUIRE(dataset1.mChannelMask.size() == 1);
        REQUIRE(dataset1.mChannelMask[0].mPage == 1);
        REQUIRE(dataset1.mChannelMask[0].mMasks == ByteArray{0xFF, 0xEE});
    }
}

} // namespace commissioner

} // namespace ot
