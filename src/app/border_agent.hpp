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

#include <ctime>
#include <functional>
#include <list>
#include <string>

#include <commissioner/defines.hpp>
#include <commissioner/network_data.hpp>

namespace ot {

namespace commissioner {

/**
 * Unix time
 *
 * @note C++ supports std::chrono parsing starting from C++20, so using low-level C API.
 */
struct UnixTime
{
    std::time_t mTime;
    UnixTime(std::time_t aTime);
    UnixTime();
    // Requires date format "%Y%m%dT%H%M%S"
    UnixTime(const std::string &aTimeStr);
    bool operator==(const UnixTime &other) const;
         operator std::string() const;
};

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
     * The discriminator which uniquely identifies the Border Agent.
     * Required by only Version>=1.2.0.
     */
    ByteArray mDiscriminator;

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

        static constexpr uint32_t kConnectionModeOffset = 6;
        static constexpr uint32_t kThreadIfStatusOffset = 4;
        static constexpr uint32_t kAvailabilityOffset   = 2;
        static constexpr uint32_t kBbrIsActiveOffset    = 1;
        static constexpr uint32_t kBbrIsPrimaryOffset   = 0;

        static constexpr uint32_t kConnectionModeMask = 7 << kConnectionModeOffset;
        static constexpr uint32_t kThreadIfStatusMask = 3 << kThreadIfStatusOffset;
        static constexpr uint32_t kAvailabilityMask   = 3 << kAvailabilityOffset;
        static constexpr uint32_t kBbrIsActiveMask    = 1 << kBbrIsActiveOffset;
        static constexpr uint32_t kBbrIsPrimaryMask   = 1 << kBbrIsPrimaryOffset;

        State(uint32_t aConnectionMode,
              uint32_t aThreadIfStatus,
              uint32_t aAvailability,
              uint32_t aBbrIsActive,
              uint32_t aBbrIsPrimary);
        State(uint32_t aState);
        State();
        operator uint32_t() const;
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

    /**
     * mDNS service name
     */
    std::string mServiceName;

    /**
     * Information update time stamp
     */
    UnixTime mUpdateTimestamp;

    uint32_t mPresentFlags = 0;

    static constexpr uint32_t kAddrBit            = 1 << 0;
    static constexpr uint32_t kPortBit            = 1 << 1;
    static constexpr uint32_t kThreadVersionBit   = 1 << 2;
    static constexpr uint32_t kStateBit           = 1 << 3;
    static constexpr uint32_t kNetworkNameBit     = 1 << 4;
    static constexpr uint32_t kExtendedPanIdBit   = 1 << 5;
    static constexpr uint32_t kVendorNameBit      = 1 << 6;
    static constexpr uint32_t kModelNameBit       = 1 << 7;
    static constexpr uint32_t kActiveTimestampBit = 1 << 8;
    static constexpr uint32_t kPartitionIdBit     = 1 << 9;
    static constexpr uint32_t kVendorDataBit      = 1 << 10;
    static constexpr uint32_t kVendorOuiBit       = 1 << 11;
    static constexpr uint32_t kDomainNameBit      = 1 << 12;
    static constexpr uint32_t kBbrSeqNumberBit    = 1 << 13;
    static constexpr uint32_t kBbrPortBit         = 1 << 14;
    static constexpr uint32_t kDiscriminatorBit   = 1 << 15;
    static constexpr uint32_t kServiceNameBit     = 1 << 16;
    static constexpr uint32_t kUpdateTimestampBit = 1 << 17;

    BorderAgent();

    BorderAgent(std::string const &aAddr,
                uint16_t           aPort,
                ByteArray const &  aDiscriminator,
                std::string const &aThreadVersion,
                BorderAgent::State aState,
                std::string const &aNetworkName,
                uint64_t           aExtendedPanId,
                std::string const &aVendorName,
                std::string const &aModelName,
                Timestamp          aActiveTimestamp,
                uint32_t           aPartitionId,
                std::string const &aVendorData,
                ByteArray const &  aVendorOui,
                std::string const &aDomainName,
                uint8_t            aBbrSeqNumber,
                uint16_t           aBbrPort,
                std::string const &aServiceName,
                UnixTime           aUpdateTimestamp,
                uint32_t           aPresentFlags);
    /**
     * Virtual destructor for polymorph storage comunications
     */
    virtual ~BorderAgent();
};

struct BorderAgentOrErrorMsg
{
    BorderAgent mBorderAgent;
    Error       mError;
};

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_BORDER_AGENT_HPP_
