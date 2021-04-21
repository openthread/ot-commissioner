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
 *   The file defines constants and type aliases.
 */

#ifndef OT_COMM_DEFINES_HPP_
#define OT_COMM_DEFINES_HPP_

#include <vector>

#include <stddef.h>
#include <stdint.h>

#if defined(__clang__)
#define OT_COMM_MUST_USE_RESULT __attribute__((warn_unused_result))
#else
#define OT_COMM_MUST_USE_RESULT
#endif

namespace ot {

namespace commissioner {

/**
 * Thread constants
 */

/**
 * The minimum Commissioner Credential length.
 */
static constexpr size_t kMinCommissionerCredentialLength = 6;

/**
 * The maximum Commissioner Credential length.
 */
static constexpr size_t kMaxCommissionerCredentialLength = 255;

/**
 * The minimum Joining Device Credential length.
 */
static constexpr size_t kMinJoinerDeviceCredentialLength = 6;

/**
 * The maximum Joining Device Credential length.
 */
static constexpr size_t kMaxJoinerDeviceCredentialLength = 32;

/**
 * The maximum PSKc length.
 */
static constexpr size_t kMaxPSKcLength = 16;

/**
 * The maximum network name length.
 */
static constexpr size_t kMaxNetworkNameLength = 16;

/**
 * The extended address length.
 */
static constexpr size_t kExtendedAddrLength = 8;

/**
 * The extended PAN ID length.
 */
static constexpr size_t kExtendedPanIdLength = 8;

/**
 * The maximum steering data length.
 */
static constexpr size_t kMaxSteeringDataLength = 16;

/**
 * The joiner ID length in bytes.
 */
static constexpr size_t kJoinerIdLength = 8;

/**
 * The Joiner Router KEK length.
 */
static constexpr size_t kJoinerRouterKekLength = 16;

/**
 * The default Joiner UDP Port used by non-CCM Thread devices if not specified.
 */
static constexpr uint16_t kDefaultJoinerUdpPort = 1000;

/**
 * The default AE UDP Port used by CCM Thread devices if not specified.
 */
static constexpr uint16_t kDefaultAeUdpPort = 1001;

/**
 * The default NMKP UDP Port used by CCM Thread devices if not specified.
 */
static constexpr uint16_t kDefaultNmkpUdpPort = 1002;

/**
 * If using radio 915Mhz. Default radio freq of Thread is 2.4Ghz.
 *
 * Used to encoding ChannelMask TLV.
 */
static constexpr bool kRadio915Mhz = false;

/**
 * Channel mask for 915Mhz.
 *
 * Used to encoding ChannelMask TLV.
 */
static constexpr uint32_t kRadio915MhzOqpskChannelMask = 0x03FF << 1;

/**
 * Channel mask for 2.4Ghz.
 *
 * Used to encoding ChannelMask TLV.
 */
static constexpr uint32_t kRadio2P4GhzOqpskChannelMask = 0xFFFF << 11;

/**
 * Channel page 0.
 *
 * Used to encoding ChannelMask TLV.
 */
static constexpr uint8_t kRadioChannelPage0 = 0;

/**
 * Channel page 2.
 *
 * Used to encoding ChannelMask TLV.
 */
static constexpr uint8_t kRadioChannelPage2 = 2;

/**
 * Type alias of raw byte array.
 */
using ByteArray = std::vector<uint8_t>;

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_DEFINES_HPP_
