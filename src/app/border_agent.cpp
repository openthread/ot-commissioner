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

#include "border_agent.hpp"

#include <fmt/format.h>

#include "common/error_macros.hpp"
#include "common/utils.hpp"

namespace ot {

namespace commissioner {

const std::string UnixTime::kFmtString = "%Y%m%dT%H%M%S";

UnixTime::UnixTime(std::time_t aTime /* =0 */)
    : mTime(aTime)
{
}

Error UnixTime::FromString(UnixTime &aTime, const std::string &aTimeStr)
{
    std::tm lTm;
    char   *result = strptime(aTimeStr.c_str(), kFmtString.c_str(), &lTm);
    if (result != nullptr && *result == '\000')
    {
        aTime = std::mktime(&lTm);
        return ERROR_NONE;
    }
    return ERROR_BAD_FORMAT("ill formed time string {}", aTimeStr);
}

bool UnixTime::operator==(const UnixTime &other) const
{
    return mTime == other.mTime;
}

UnixTime::operator std::string() const
{
    std::tm lTm;
    gmtime_r(&mTime, &lTm);

    char lTimeStr[16];
    std::strftime(lTimeStr, sizeof(lTimeStr), kFmtString.c_str(), &lTm);
    return lTimeStr;
}

BorderAgent::State::State(uint32_t aConnectionMode,
                          uint32_t aThreadIfStatus,
                          uint32_t aAvailability,
                          uint32_t aBbrIsActive,
                          uint32_t aBbrIsPrimary)
    : mConnectionMode(aConnectionMode)
    , mThreadIfStatus(aThreadIfStatus)
    , mAvailability(aAvailability)
    , mBbrIsActive(aBbrIsActive)
    , mBbrIsPrimary(aBbrIsPrimary)
{
}

BorderAgent::State::State(uint32_t aState)
    : State((aState & kConnectionModeMask) >> kConnectionModeOffset,
            (aState & kThreadIfStatusMask) >> kThreadIfStatusOffset,
            (aState & kAvailabilityMask) >> kAvailabilityOffset,
            (aState & kBbrIsActiveMask) >> kBbrIsActiveOffset,
            (aState & kBbrIsPrimaryMask) >> kBbrIsPrimaryOffset)
{
}

BorderAgent::State::State()
    : State(0)
{
}

BorderAgent::State::operator uint32_t() const
{
    return ((mConnectionMode << kConnectionModeOffset) & kConnectionModeMask) |
           ((mThreadIfStatus << kThreadIfStatusOffset) & kThreadIfStatusMask) |
           ((mAvailability << kAvailabilityOffset) & kAvailabilityMask) |
           ((mBbrIsActive << kBbrIsActiveOffset) & kBbrIsActiveMask) | (mBbrIsPrimary & kBbrIsPrimaryMask);
}

BorderAgent::BorderAgent()
    : BorderAgent{
          "", 0,  ByteArray{}, "", State{0, 0, 0, 0, 0}, "", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "", 0,
          0,  "", 0,           0}
{
}

BorderAgent::BorderAgent(std::string const &aAddr,
                         uint16_t           aPort,
                         ByteArray const   &aDiscriminator,
                         std::string const &aThreadVersion,
                         BorderAgent::State aState,
                         std::string const &aNetworkName,
                         uint64_t           aExtendedPanId,
                         std::string const &aVendorName,
                         std::string const &aModelName,
                         Timestamp          aActiveTimestamp,
                         uint32_t           aPartitionId,
                         std::string const &aVendorData,
                         ByteArray const   &aVendorOui,
                         std::string const &aDomainName,
                         uint8_t            aBbrSeqNumber,
                         uint16_t           aBbrPort,
                         std::string const &aServiceName,
                         UnixTime           aUpdateTimestamp,
                         uint32_t           aPresentFlags)
    : mAddr(aAddr)
    , mPort(aPort)
    , mDiscriminator(aDiscriminator)
    , mThreadVersion(aThreadVersion)
    , mState{aState}
    , mNetworkName(aNetworkName)
    , mExtendedPanId(aExtendedPanId)
    , mVendorName(aVendorName)
    , mModelName(aModelName)
    , mActiveTimestamp{aActiveTimestamp}
    , mPartitionId(aPartitionId)
    , mVendorData(aVendorData)
    , mVendorOui(aVendorOui)
    , mDomainName(aDomainName)
    , mBbrSeqNumber(aBbrSeqNumber)
    , mBbrPort(aBbrPort)
    , mServiceName(aServiceName)
    , mUpdateTimestamp(aUpdateTimestamp)
    , mPresentFlags(aPresentFlags)
{
}

} // namespace commissioner

} // namespace ot
