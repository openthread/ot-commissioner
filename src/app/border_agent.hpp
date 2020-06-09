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
 *   The file defines Border Agent structure and discovery
 *   by mDNS in local network.
 */

#ifndef OT_COMM_BORDER_AGENT_HPP_
#define OT_COMM_BORDER_AGENT_HPP_

#include <functional>
#include <list>
#include <string>

#include <commissioner/defines.hpp>
#include <commissioner/network_data.hpp>

namespace ot {

namespace commissioner {

/**
 * The definition of Border Agent discovered by Commissioner.
 *
 */
struct BorderAgent
{
    /**
     * Border Agent Address. Mandatory.
     */
    std::string mAddr;

    /**
     * Thread Mesh Commissioner Port. Mandatory.
     */
    uint16_t mPort;

    /**
     * Version of Thread Specification implemented by the Thread
     * Interface specified as a UTF-8 encoded decimal. Mandatory.
     */
    std::string mThreadVersion;

    /**
     * State bitmap. Mandatory.
     */
    struct State
    {
        uint32_t mConnectionMode : 3;
        uint32_t mThreadIfStatus : 2;
        uint32_t mAvailability : 2;
        uint32_t mBbrIsActive : 1;
        uint32_t mBbrIsPrimary : 1;
    } mState;

    /**
     * Network Name in the PSKc computation used for Commissioner Authentication.
     * Optional (depending on the Connection Mode of State bitmap).
     */
    std::string mNetworkName;

    /**
     * Extended PAN ID in the PSKc computation used for Commissioner Authentication.
     * Optional (depending on the Connection Mode of State bitmap).
     */
    uint64_t mExtendedPanId;

    /**
     * Vendor Name. Optional.
     */
    std::string mVendorName;

    /**
     * Model Name. Optional.
     */
    std::string mModelName;

    /**
     * Active Operational Dataset Timestamp of the current
     * active Thread Network Partition of the Thread Interface. Optional.
     */
    Timestamp mActiveTimestamp;

    /**
     * Partition ID of the Thread Interface. Optional.
     */
    uint32_t mPartitionId;

    /**
     * Vendor-specific data which may guide application specific discovery. Optional.
     */
    std::string mVendorData;

    /**
     * Device Vendor OUI as assigned by IEEE. Required if `mVendorData` is present.
     */
    ByteArray mVendorOui;

    /**
     * Thread Domain Name. Required in only 1.2 CCM network.
     */
    std::string mDomainName;

    /**
     * BBR Sequence Number. Required by only Version>=1.2.0.
     */
    uint8_t mBbrSeqNumber;

    /**
     * BBR Port. Required by only Version>=1.2.0.
     */
    uint16_t mBbrPort;

    uint16_t mPresentFlags = 0;

    static constexpr uint16_t kAddrBit            = 1 << 0;
    static constexpr uint16_t kPortBit            = 1 << 1;
    static constexpr uint16_t kThreadVersionBit   = 1 << 2;
    static constexpr uint16_t kStateBit           = 1 << 3;
    static constexpr uint16_t kNetworkNameBit     = 1 << 4;
    static constexpr uint16_t kExtendedPanIdBit   = 1 << 5;
    static constexpr uint16_t kVendorNameBit      = 1 << 6;
    static constexpr uint16_t kModelNameBit       = 1 << 7;
    static constexpr uint16_t kActiveTimestampBit = 1 << 8;
    static constexpr uint16_t kPartitionIdBit     = 1 << 9;
    static constexpr uint16_t kVendorDataBit      = 1 << 10;
    static constexpr uint16_t kVendorOuiBit       = 1 << 11;
    static constexpr uint16_t kDomainNameBit      = 1 << 12;
    static constexpr uint16_t kBbrSeqNumberBit    = 1 << 13;
    static constexpr uint16_t kBbrPortBit         = 1 << 14;
};

/**
 * This function is the callback of a discovered Border Agent.
 *
 * @param[in] aBorderAgent   The discovered Border Agent. Not null
 *                           only when aError== ErrorCode::kNone is true.
 * @param[in] aError         The error;
 *
 */
using BorderAgentHandler = std::function<void(const BorderAgent *aBorderAgent, const Error &aError)>;

/**
 * Discovery Border Agent in local network with mDNS.
 *
 * @param[in] aBorderAgentHandler  The handler of found Border Agent.
 *                                 called once for each Border Agent.
 * @param[in] aTimeout             The time waiting for mDNS responses.
 *
 */
Error DiscoverBorderAgent(BorderAgentHandler aBorderAgentHandler, size_t aTimeout);

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_BORDER_AGENT_HPP_
