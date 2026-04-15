/*
 *    Copyright (c) 2026, The OpenThread Commissioner Authors.
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

#include "library/network_traverser.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>

#include <map>
#include <string>
#include <vector>

#include "commissioner/commissioner.hpp"
#include "commissioner/error.hpp"
#include "commissioner/network_data.hpp"
#include "commissioner/network_diag_data.hpp"
#include "common/address.hpp"
#include "common/error_macros.hpp"
#include "common/logging.hpp"
#include "library/commissioner_impl.hpp"
#include "library/timer.hpp"

namespace ot {
namespace commissioner {

const int NetworkTraverser::kDefaultTimeoutMs = 500;
const int NetworkTraverser::kSleepyTimeoutMs  = 10000;
const int NetworkTraverser::kMaxRetries       = 3;

// We request diagnostic data in chunks because some Thread Border Routers
// may fail to respond to large TMF Network Diagnostic Get messages.
// while Thread 1.4 should support larger messages handling, older versions
// or specific implementations might struggle with fragmentation or buffer limits.

static const std::vector<uint64_t> kLeaderChunks = {
    NetDiagData::kRoute64Bit | NetDiagData::kMacAddrBit | NetDiagData::kEui64Bit,        // Chunk 0: Topology & ID
    NetDiagData::kExtMacAddrBit | NetDiagData::kModeBit | NetDiagData::kConnectivityBit, // Chunk 1: Connectivity
    NetDiagData::kLeaderDataBit | NetDiagData::kNetworkDataBit |
        NetDiagData::kNonPreferredChannelsMaskBit,                  // Chunk 2: Network Data
    NetDiagData::kChildTableBit | NetDiagData::kMaxChildTimeoutBit, // Chunk 3: Child Table
    NetDiagData::kChildIpv6AddrsInfoListBit,                        // Chunk 4: Child IPv6 Addresses
    NetDiagData::kChildBit,                                         // Chunk 5: Child Info (Large)
    NetDiagData::kAddrsBit | NetDiagData::kRouterNeighborBit,       // Chunk 6: Neighbors & Addresses
    NetDiagData::kMleCountersBit | NetDiagData::kMacCountersBit,    // Chunk 7: Counters
    NetDiagData::kBatteryLevelBit | NetDiagData::kSupplyVoltageBit | NetDiagData::kVersionBit |
        NetDiagData::kChannelPagesBit | NetDiagData::kTypeListBit, // Chunk 8: Device Info
    NetDiagData::kVendorNameBit | NetDiagData::kVendorModelBit | NetDiagData::kVendorSWVersionBit |
        NetDiagData::kThreadStackVersionBit | NetDiagData::kVendorAppURLBit // Chunk 9: Vendor Info
};

static const std::vector<uint64_t> kRouterChunks = {
    NetDiagData::kMacAddrBit | NetDiagData::kExtMacAddrBit | NetDiagData::kEui64Bit, // Chunk 0: ID
    NetDiagData::kModeBit | NetDiagData::kConnectivityBit | NetDiagData::kRoute64Bit |
        NetDiagData::kNonPreferredChannelsMaskBit, // Chunk 1: Connectivity & Topology
    NetDiagData::kChildTableBit | NetDiagData::kChildIpv6AddrsInfoListBit |
        NetDiagData::kMaxChildTimeoutBit,                        // Chunk 2: Child Table
    NetDiagData::kChildBit,                                      // Chunk 3: Child Info (Large)
    NetDiagData::kAddrsBit | NetDiagData::kRouterNeighborBit,    // Chunk 4: Neighbors & Addresses
    NetDiagData::kMleCountersBit | NetDiagData::kMacCountersBit, // Chunk 5: Counters
    NetDiagData::kBatteryLevelBit | NetDiagData::kSupplyVoltageBit | NetDiagData::kVersionBit |
        NetDiagData::kChannelPagesBit | NetDiagData::kTypeListBit, // Chunk 6: Device Info
    NetDiagData::kVendorNameBit | NetDiagData::kVendorModelBit | NetDiagData::kVendorSWVersionBit |
        NetDiagData::kThreadStackVersionBit | NetDiagData::kVendorAppURLBit // Chunk 7: Vendor Info
};

static const std::vector<uint64_t> kChildChunks = {
    NetDiagData::kMacAddrBit | NetDiagData::kExtMacAddrBit | NetDiagData::kModeBit |
        NetDiagData::kEui64Bit, // Chunk 0: ID & Mode
    NetDiagData::kConnectivityBit | NetDiagData::kTimeoutBit | NetDiagData::kAddrsBit |
        NetDiagData::kNonPreferredChannelsMaskBit,               // Chunk 1: Connectivity & Network Data
    NetDiagData::kMleCountersBit | NetDiagData::kMacCountersBit, // Chunk 2: Counters
    NetDiagData::kBatteryLevelBit | NetDiagData::kSupplyVoltageBit | NetDiagData::kVersionBit |
        NetDiagData::kChannelPagesBit | NetDiagData::kTypeListBit, // Chunk 3: Device Info
    NetDiagData::kVendorNameBit | NetDiagData::kVendorModelBit | NetDiagData::kVendorSWVersionBit |
        NetDiagData::kThreadStackVersionBit | NetDiagData::kVendorAppURLBit // Chunk 4: Vendor Info
};

NetworkTraverser::NetworkTraverser(CommissionerImpl &aImpl)
    : mImpl(aImpl)
    , mRequestTimeoutTimer(aImpl.GetEventBase(), [this](Timer &aTimer) { HandleTimer(aTimer); })
    , mState(State::kIdle)
{
}

Error NetworkTraverser::Start(Commissioner::TraverseHandler aHandler)
{
    mCollectedData.clear();
    mRoutersToQuery.clear();
    mChildrenToQuery.clear();
    mHandler = aHandler;
    mHasSharedNetworkData = false;

    const char *env = std::getenv("OT_COMM_IGNORE_PREFIX_FOR_TEST");
    if (env && std::string(env) == "1")
    {
        mIgnoreMeshLocalPrefixForTest = true;
    }

    mState = State::kGettingDataset;
    mImpl.GetActiveDataset(
        [this](const ActiveOperationalDataset *aDataset, Error aError) { OnActiveDataset(aDataset, aError); },
        ActiveOperationalDataset::kMeshLocalPrefixBit);

    return ERROR_NONE;
}

void NetworkTraverser::OnActiveDataset(const ActiveOperationalDataset *aDataset, Error aError)
{
    if (mState != State::kGettingDataset)
    {
        return;
    }

    if (aError != ErrorCode::kNone || aDataset == nullptr)
    {
        Finalize(aError != ErrorCode::kNone ? aError : ERROR_NOT_FOUND("Active Dataset not found"));
        return;
    }

    mMeshLocalPrefix = aDataset->mMeshLocalPrefix;
    
    if (mIgnoreMeshLocalPrefixForTest)
    {
        mMeshLocalPrefix.clear();
        LOG_INFO(LOG_REGION_MESHDIAG, "Ignoring Mesh Local Prefix for test purposes");
    }

    if (mMeshLocalPrefix.empty())
    {
        StartFallbackPrefixDiscovery();
        return;
    }

    ProceedToQueryLeader();
}

void NetworkTraverser::StartFallbackPrefixDiscovery()
{
    LOG_INFO(LOG_REGION_MESHDIAG, "Mesh Local Prefix not found in dataset. Starting fallback discovery...");
    mState = State::kFallbackPrefixDiscovery;
    
    mCurrentQueryTarget = "ff03::2"; // Realm-local all routers
    mPendingChunks = {NetDiagData::kAddrsBit};
    mCurrentChunkIndex = 0;
    mRetryCount = 0;
    mDeviceRetried = false;
    
    QueryChunk();
}

void NetworkTraverser::ProceedToQueryLeader()
{
    mState = State::kQueryingLeader;

    const Address addr = GetMeshLocalAddress(0xfc00);
    if (!addr.IsValid())
    {
        Finalize(ERROR_BAD_FORMAT("Failed to construct Leader ALOC"));
        return;
    }

    mCurrentQueryTarget = addr.ToString();
    mCurrentQueryRloc16 = 0xFC00; // Leader ALOC
    mPendingChunks      = kLeaderChunks;
    mCurrentChunkIndex  = 0;
    mRetryCount         = 0;
    mDeviceRetried      = false;

    QueryChunk();
}

void NetworkTraverser::Stop()
{
    mRequestTimeoutTimer.Stop();
    mState = State::kIdle;

    if (mHandler.mOnFinished)
    {
        mHandler.mOnFinished(nullptr, ERROR_CANCELLED("Network traversal cancelled"));
    }

    mHandler = {};
}

void NetworkTraverser::QueryChunk()
{
    if (mCurrentChunkIndex >= mPendingChunks.size())
    {
        FinalizeNode();
        return;
    }

    uint64_t chunk = mPendingChunks[mCurrentChunkIndex];
    mImpl.CommandDiagGetQuery([](Error) {}, mCurrentQueryTarget, chunk);

    int timeoutMs = kDefaultTimeoutMs;

    if (mState == State::kQueryingChildren)
    {
        // is it sleepy?
        if (mChildrenToQuery.count(mCurrentQueryRloc16) && mChildrenToQuery.at(mCurrentQueryRloc16))
        {
            timeoutMs = kSleepyTimeoutMs;
        }
    }

    mRequestTimeoutTimer.Start(std::chrono::milliseconds(timeoutMs));
}

void NetworkTraverser::HandleTimer(Timer &aTimer)
{
    (void)aTimer;
    if (mRetryCount < kMaxRetries)
    {
        mRetryCount++;
        mDeviceRetried = true;
        // Retry current chunk
        QueryChunk();
    }
    else
    {
        if (mState == State::kFallbackPrefixDiscovery)
        {
            LOG_ERROR(LOG_REGION_MESHDIAG, "Fallback: failed to discover Mesh Local Prefix after retries");
            Finalize(ERROR_NOT_FOUND("Failed to discover Mesh Local Prefix via fallback"));
            return;
        }
        // Give up on this device or this chunk
        // If we fail on the first chunk (ID), we can't do much
        // If we fail on later chunks, we might have partial data

        if (mCurrentChunkIndex == 0)
        {
            // First chunk failed, skip device.
            if (mHandler.mOnDeviceResponded)
            {
                mHandler.mOnDeviceResponded(mCurrentQueryTarget, nullptr, Commissioner::TraverseStatus::kFailed);
            }
            FinalizeNode();
        }
        else
        {
            // Partial failure, continue to next chunk.
            mCurrentChunkIndex++;
            mRetryCount = 0;
            QueryChunk();
        }
    }
}

void NetworkTraverser::OnDiagGetAnswer(const std::string &aPeerAddr, const NetDiagData &aDiagAnsMsg)
{
    if (mState == State::kIdle)
    {
        return;
    }

    if (mState == State::kFallbackPrefixDiscovery)
    {
        LOG_INFO(LOG_REGION_MESHDIAG, "Fallback: received diag answer from {}", aPeerAddr);
        LOG_INFO(LOG_REGION_MESHDIAG, "Fallback: answer contains {} addresses", aDiagAnsMsg.mAddrs.size());
        for (const auto &addrStr : aDiagAnsMsg.mAddrs)
        {
            Address addr;
            IgnoreError(addr.Set(addrStr));
            auto annotation = addr.GetTypeAnnotation(mMeshLocalPrefix);
            LOG_INFO(LOG_REGION_MESHDIAG, "Fallback: reported address {}{}", addrStr, annotation.empty() ? "" : " [" + annotation + "]");
        }

        // Try to find an RLOC address in the reported list of addresses.
        // We look for an address with the RLOC/ALOC Interface Identifier pattern (0000:00ff:fe00:XXXX)
        // to reliably identify the Mesh-Local prefix. We cannot simply look for any ULA prefix
        // (starting with fd or fc) because the node might also have on-mesh ULA prefixes assigned
        // for external communication, which would lead to extracting the wrong prefix.
        for (const auto &addrStr : aDiagAnsMsg.mAddrs)
        {
            Address addr;
            Error   err = addr.Set(addrStr);
            if (err == ErrorCode::kNone && addr.IsIpv6())
            {
                auto raw = addr.GetRaw();
                if (raw.size() >= 16)
                {
                    // Check for RLOC IID pattern: 0000:00ff:fe00:XXXX
                    if (raw[8] == 0x00 && raw[9] == 0x00 && raw[10] == 0x00 &&
                        raw[11] == 0xff && raw[12] == 0xfe && raw[13] == 0x00)
                    {
                        mMeshLocalPrefix = ByteArray(raw.begin(), raw.begin() + 8);
                        auto annotation = addr.GetTypeAnnotation(mMeshLocalPrefix);
                        LOG_INFO(LOG_REGION_MESHDIAG, "Discovered Mesh Local Prefix from RLOC address {}{}: {}", addrStr, annotation.empty() ? "" : " [" + annotation + "]", utils::Hex(mMeshLocalPrefix));
                        
                        mRequestTimeoutTimer.Stop();
                        ProceedToQueryLeader();
                        return;
                    }
                }
            }
        }

        // If no RLOC address found in the reported list, check if the source address of the response
        // is an RLOC and use its prefix if so.
        Address addr;
        if (addr.Set(aPeerAddr) == ErrorCode::kNone && addr.IsIpv6())
        {
            auto raw = addr.GetRaw();
            if (raw.size() >= 16)
            {
                if (raw[8] == 0x00 && raw[9] == 0x00 && raw[10] == 0x00 &&
                    raw[11] == 0xff && raw[12] == 0xfe && raw[13] == 0x00)
                {
                    mMeshLocalPrefix = ByteArray(raw.begin(), raw.begin() + 8);
                    auto annotation = addr.GetTypeAnnotation(mMeshLocalPrefix);
                    LOG_INFO(LOG_REGION_MESHDIAG, "Discovered Mesh Local Prefix from source RLOC {}{}: {}", aPeerAddr, annotation.empty() ? "" : " [" + annotation + "]", utils::Hex(mMeshLocalPrefix));
                    
                    mRequestTimeoutTimer.Stop();
                    ProceedToQueryLeader();
                    return;
                }
            }
        }

        LOG_INFO(LOG_REGION_MESHDIAG, "Fallback: no RLOC address found in this response");
        return;
    }

    // Check if this answer matches what we are looking for.
    // Here we can check if it contains ANY of requested flags in current chunk.

    if (mCurrentChunkIndex >= mPendingChunks.size())
    {
        LOG_INFO(LOG_REGION_MESHDIAG, "OnDiagGetAnswer: Received answer but no pending chunks");
        return;
    }

    uint64_t requested = mPendingChunks[mCurrentChunkIndex];

    if (requested & NetDiagData::kNetworkDataBit)
    {
        LOG_INFO(LOG_REGION_MESHDIAG, "OnDiagGetAnswer: Expecting Network Data. Recv Flags: 0x{:X}",
                 aDiagAnsMsg.mPresentFlags);
        if (aDiagAnsMsg.mPresentFlags & NetDiagData::kNetworkDataBit)
        {
            LOG_INFO(LOG_REGION_MESHDIAG, "OnDiagGetAnswer: Network Data RECEIVED!");
        }
        else
        {
            LOG_INFO(LOG_REGION_MESHDIAG, "OnDiagGetAnswer: Network Data MISSING in response.");
        }
    }
    if ((aDiagAnsMsg.mPresentFlags & requested) == 0)
    {
        return;
    }

    // Match. Store data
    Address addr;
    // aPeerAddr is string IP. Convert to Address.
    if (addr.Set(aPeerAddr) != ErrorCode::kNone)
    {
        return;
    }

    // For children, we might want to validate MacAddr matches target RLOC16.
    if (mState == State::kQueryingChildren)
    {
        if ((aDiagAnsMsg.mPresentFlags & NetDiagData::kMacAddrBit) && (aDiagAnsMsg.mMacAddr != mCurrentQueryRloc16))
        {
            return;
        }
    }

    // If we already have data for this device, merge it with previous data.
    if (mCollectedData.count(addr))
    {
        auto &existing = mCollectedData[addr];
        existing.mPresentFlags |= aDiagAnsMsg.mPresentFlags;
        existing.Merge(aDiagAnsMsg);
    }
    // If we don't have data for this device, add it.
    else
    {
        mCollectedData[addr] = aDiagAnsMsg;
    }

    if (aDiagAnsMsg.mPresentFlags & NetDiagData::kNetworkDataBit)
    {
        mSharedNetworkData    = aDiagAnsMsg.mNetworkData;
        mHasSharedNetworkData = true;
    }

    // Stop timer and next chunk
    mRequestTimeoutTimer.Stop();
    mRetryCount = 0;
    mCurrentChunkIndex++;

    if (mCurrentChunkIndex >= mPendingChunks.size())
    {
        // Device fully queried
        if (mHasSharedNetworkData && !(mCollectedData[addr].mPresentFlags & NetDiagData::kNetworkDataBit))
        {
            mCollectedData[addr].mNetworkData = mSharedNetworkData;
            mCollectedData[addr].mPresentFlags |= NetDiagData::kNetworkDataBit;
        }

        if (mHandler.mOnDeviceResponded)
        {
            auto status = mDeviceRetried ? Commissioner::TraverseStatus::kSuccessWithRetry
                                         : Commissioner::TraverseStatus::kSuccess;
            mHandler.mOnDeviceResponded(addr.ToString(), &mCollectedData[addr], status);
        }
    }

    QueryChunk();
}

void NetworkTraverser::FinalizeNode()
{
    // First check the query state
    if (mState == State::kQueryingLeader)
    {
        // Analyze leader data to find routers.
        // We need to find the Leader's entry in collected data.
        bool        foundLeader = false;
        Address     leaderAddr;
        NetDiagData leaderData;

        for (const auto &pair : mCollectedData)
        {
            if (pair.second.mPresentFlags & NetDiagData::kRoute64Bit)
            {
                leaderAddr  = pair.first;
                leaderData  = pair.second;
                foundLeader = true;
                break;
            }
        }

        if (!foundLeader)
        {
            Finalize(ERROR_NOT_FOUND("Leader not found"));
            return;
        }

        // Parse Route64
        Route64  route64      = leaderData.mRoute64;
        uint16_t leaderRloc16 = 0xFFFF;
        if (leaderData.mPresentFlags & NetDiagData::kLeaderDataBit)
        {
            leaderRloc16 = static_cast<uint16_t>(leaderData.mLeaderData.mRouterId) << 10;
        }
        else if (leaderData.mPresentFlags & NetDiagData::kMacAddrBit)
        {
            leaderRloc16 = leaderData.mMacAddr;
        }

        for (uint8_t routerId = 0; routerId < 64; ++routerId)
        {
            uint8_t byteIdx = routerId / 8;
            uint8_t bitIdx  = 7 - (routerId % 8);

            if (byteIdx < route64.mMask.size() && (route64.mMask[byteIdx] & (1 << bitIdx)))
            {
                uint16_t rloc16 = static_cast<uint16_t>(routerId) << 10;
                if (rloc16 != leaderRloc16)
                {
                    mRoutersToQuery.insert(rloc16);
                }
            }
        }

        if (mHandler.mOnTotalRoutersCount)
        {
            mHandler.mOnTotalRoutersCount(mRoutersToQuery.size() + 1); // +1 for Leader
        }

        // Add leader's children
        if (leaderRloc16 != 0xFFFF && (leaderData.mPresentFlags & NetDiagData::kChildTableBit))
        {
            for (const auto &childEntry : leaderData.mChildTable)
            {
                mChildrenToQuery[leaderRloc16 | childEntry.mChildId] = !childEntry.mModeData.mIsRxOnWhenIdleMode;
            }
        }

        mState = State::kQueryingRouters;
        FinalizeNode(); // Recurse to start router querying (or finding next router)
    }
    else if (mState == State::kQueryingRouters)
    {
        // Process data from the *just finished* router (if any) to find its children.
        // We use mCurrentQueryRloc16 which holds the processed router's RLOC.
        uint16_t rloc16 = mCurrentQueryRloc16;

        // Find received data for this router
        for (const auto &pair : mCollectedData)
        {
            if ((pair.second.mPresentFlags & NetDiagData::kMacAddrBit) && (pair.second.mMacAddr == rloc16))
            {
                if (pair.second.mPresentFlags & NetDiagData::kChildTableBit)
                {
                    for (const auto &childEntry : pair.second.mChildTable)
                    {
                        // RLOC16 -> isSleepy
                        mChildrenToQuery[rloc16 | childEntry.mChildId] = !childEntry.mModeData.mIsRxOnWhenIdleMode;
                    }
                }
                break;
            }
        }

        if (mRoutersToQuery.empty())
        {
            mState = State::kQueryingChildren;
            if (mHandler.mOnTotalChildrenCount)
            {
                mHandler.mOnTotalChildrenCount(mChildrenToQuery.size());
            }
            FinalizeNode();
            return;
        }

        // Get next router
        uint16_t routerRloc = *mRoutersToQuery.begin();
        mRoutersToQuery.erase(mRoutersToQuery.begin());

        const Address addr = GetMeshLocalAddress(routerRloc);
        if (!addr.IsValid())
        {
            FinalizeNode();
            return;
        }

        mCurrentQueryTarget = addr.ToString();
        mCurrentQueryRloc16 = routerRloc;
        mPendingChunks      = kRouterChunks;
        mCurrentChunkIndex  = 0;
        mRetryCount         = 0;
        mDeviceRetried      = false;
        QueryChunk();
    }
    else if (mState == State::kQueryingChildren)
    {
        if (mChildrenToQuery.empty())
        {
            Finalize(ERROR_NONE);
            return;
        }

        auto     it          = mChildrenToQuery.begin();
        uint16_t childRloc16 = it->first;
        // bool isSleepy = it->second;
        mChildrenToQuery.erase(it);

        const Address addr = GetMeshLocalAddress(childRloc16);
        if (!addr.IsValid())
        {
            FinalizeNode();
            return;
        }

        mCurrentQueryTarget = addr.ToString();
        mCurrentQueryRloc16 = childRloc16;
        mPendingChunks      = kChildChunks;
        mCurrentChunkIndex  = 0;
        mRetryCount         = 0;
        mDeviceRetried      = false;
        QueryChunk();
    }
}

void NetworkTraverser::Finalize(Error aError)
{
    mState = State::kIdle;
    if (mHandler.mOnFinished)
    {
        std::map<std::string, NetDiagData> report;
        for (const auto &pair : mCollectedData)
        {
            report[pair.first.ToString()] = pair.second;
        }
        mHandler.mOnFinished(&report, aError);
    }
}

Address NetworkTraverser::GetMeshLocalAddress(uint16_t aRloc16) const
{
    Address addr;
    // RLOC is Prefix + 0000:00ff:fe00:RLOC16
    std::vector<uint8_t> addrBytes = mMeshLocalPrefix;
    if (addrBytes.size() != 8)
    {
        return addr; // Invalid
    }

    addrBytes.push_back(0x00);
    addrBytes.push_back(0x00);
    addrBytes.push_back(0x00);
    addrBytes.push_back(0xff);
    addrBytes.push_back(0xfe);
    addrBytes.push_back(0x00);
    addrBytes.push_back((aRloc16 >> 8) & 0xFF);
    addrBytes.push_back(aRloc16 & 0xFF);

    static_cast<void>(addr.Set(addrBytes));
    return addr;
}

size_t NetworkTraverser::GetLeaderChunkCount() { return kLeaderChunks.size(); }

size_t NetworkTraverser::GetRouterChunkCount() { return kRouterChunks.size(); }

size_t NetworkTraverser::GetChildChunkCount() { return kChildChunks.size(); }

} // namespace commissioner
} // namespace ot
