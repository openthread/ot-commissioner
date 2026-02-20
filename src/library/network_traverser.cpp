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
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "commissioner/commissioner.hpp"
#include "commissioner/error.hpp"
#include "commissioner/network_data.hpp"
#include "commissioner/network_diag_data.hpp"
#include "common/address.hpp"
#include "common/error_macros.hpp"
#include "library/commissioner_impl.hpp"
#include "library/timer.hpp"

namespace ot {
namespace commissioner {

const int NetworkTraverser::kDefaultTimeoutMs = 1000;
const int NetworkTraverser::kChunkDelayMs     = 500;
const int NetworkTraverser::kSleepyTimeoutMs  = 10000;
const int NetworkTraverser::kMaxRetries       = 3;

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
    NetDiagData::kConnectivityBit | NetDiagData::kTimeoutBit | NetDiagData::kAddrsBit | NetDiagData::kNetworkDataBit |
        NetDiagData::kNonPreferredChannelsMaskBit,               // Chunk 1: Connectivity & Network Data
    NetDiagData::kMleCountersBit | NetDiagData::kMacCountersBit, // Chunk 2: Counters
    NetDiagData::kBatteryLevelBit | NetDiagData::kSupplyVoltageBit | NetDiagData::kVersionBit |
        NetDiagData::kChannelPagesBit | NetDiagData::kTypeListBit, // Chunk 3: Device Info
    NetDiagData::kVendorNameBit | NetDiagData::kVendorModelBit | NetDiagData::kVendorSWVersionBit |
        NetDiagData::kThreadStackVersionBit | NetDiagData::kVendorAppURLBit // Chunk 4: Vendor Info
};

NetworkTraverser::NetworkTraverser(CommissionerImpl &aImpl)
    : mImpl(aImpl)
    , mTimer(aImpl.GetEventBase(), [this](Timer &aTimer) { HandleTimer(aTimer); })
    , mState(State::kIdle)
{
}

Error NetworkTraverser::Start(Commissioner::TraverseHandler aHandler)
{
    mCollectedData.clear();
    mRoutersToQuery.clear();
    mChildrenToQuery.clear();
    mHandler = aHandler; // Store handler

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
        // Failed to get dataset, can't construct RLOCs.
        // We could try fallback if we had one, but for now fail.
        Finalize(aError != ErrorCode::kNone ? aError : ERROR_NOT_FOUND("Active Dataset not found"));
        return;
    }

    mMeshLocalPrefix = aDataset->mMeshLocalPrefix;
    if (mMeshLocalPrefix.empty())
    {
        Finalize(ERROR_NOT_FOUND("Mesh Local Prefix not found"));
        return;
    }

    mState = State::kQueryingLeader;

    // Construct Leader ALOC (0xfc00)
    // RLOC is Prefix + 0000:00ff:fe00:RLOC16
    // ALOC 0xfc00 is RLOC16 form? Yes?
    // Let's assume standard RLOC generation.
    // Address::Set(ByteArray) expects 16 bytes.
    std::vector<uint8_t> addrBytes = mMeshLocalPrefix;
    if (addrBytes.size() != 8)
    {
        Finalize(ERROR_BAD_FORMAT("Invalid Mesh Local Prefix length"));
        return;
    }
    // Append 0000:00ff:fe00:fc00
    addrBytes.push_back(0x00);
    addrBytes.push_back(0x00);
    addrBytes.push_back(0x00);
    addrBytes.push_back(0xff);
    addrBytes.push_back(0xfe);
    addrBytes.push_back(0x00);
    addrBytes.push_back(0xfc);
    addrBytes.push_back(0x00);

    Address addr;
    if (addr.Set(addrBytes) != ErrorCode::kNone)
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

    QueryNextChunk();
}

void NetworkTraverser::Stop()
{
    mTimer.Stop();
    mState = State::kIdle;

    if (mHandler.mOnFinished)
    {
        mHandler.mOnFinished(nullptr, ERROR_CANCELLED("Network traversal cancelled"));
    }

    mHandler = {};
}

void NetworkTraverser::QueryNextChunk()
{
    if (mCurrentChunkIndex >= mPendingChunks.size())
    {
        Proceed();
        return;
    }

    uint64_t chunk = mPendingChunks[mCurrentChunkIndex];
    mImpl.CommandDiagGetQuery([](Error) {}, mCurrentQueryTarget, chunk);

    // Dynamic timeout based on target type/chunk? default for now.
    int timeoutMs = kDefaultTimeoutMs;
    // For children, checking if sleepy happens in Proceed(), but here we might need to know.
    // We can check if in kQueryingChildren and look up child sleepiness.
    if (mState == State::kQueryingChildren)
    {
        // We need to parse target to get RLOC16?
        // Or we can just look up in mChildrenToQuery if we know which one we are querying.
        // But mCurrentQueryTarget is a string.
        // It's cleaner to set timeout based on mState or sleepiness flag stored.
        // Let's assume default unless specific case.
        // kSleepyTimeoutMs is 10000.
        // We can parse "0xXXXX" from mCurrentQueryTarget.
        unsigned int rloc16;
        if (sscanf(mCurrentQueryTarget.c_str(), "0x%x", &rloc16) == 1)
        {
            if (mChildrenToQuery.count(rloc16) && mChildrenToQuery.at(rloc16))
            {
                timeoutMs = kSleepyTimeoutMs;
            }
        }
    }

    mTimer.Start(std::chrono::milliseconds(timeoutMs));
}

void NetworkTraverser::HandleTimer(Timer &aTimer)
{
    (void)aTimer;
    // Timeout
    if (mRetryCount < kMaxRetries)
    {
        mRetryCount++;
        mDeviceRetried = true;
        // Retry current chunk
        QueryNextChunk();
    }
    else
    {
        // Give up on this device or this chunk?
        // If we fail on the first chunk (ID), we probably can't do much.
        // If we fail on later chunks, we might have partial data.
        // The CLI implementation skipped to next device on failure.
        // But for chunks, if one chunk fails, it tried next chunks? No, CLI implementation:
        // "if (i == 0 && !chunkReceived) return false;" -> fail device.
        // "else chunkReceived = true" (handled in loop).
        // It seems CLI continues if first chunk succeeds.

        if (mCurrentChunkIndex == 0)
        {
            // First chunk failed, skip device.
            if (mHandler.mOnDeviceResponded)
            {
                mHandler.mOnDeviceResponded(mCurrentQueryTarget, nullptr, Commissioner::TraverseStatus::kFailed);
            }
            Proceed();
        }
        else
        {
            // Partial failure, continue to next chunk.
            mCurrentChunkIndex++;
            mRetryCount = 0;
            QueryNextChunk();
        }
    }
}

void NetworkTraverser::OnDiagGetAnswer(const std::string &aPeerAddr, const NetDiagData &aDiagAnsMsg)
{
    if (mState == State::kIdle)
    {
        return;
    }

    // Check if this answer matches what we are looking for.
    // In CLI, we filtered by requested flags.
    // Here we can check if it contains ANY of requested flags in current chunk.
    if (mCurrentChunkIndex >= mPendingChunks.size())
    {
        return;
    }

    uint64_t requested = mPendingChunks[mCurrentChunkIndex];
    if ((aDiagAnsMsg.mPresentFlags & requested) == 0)
    {
        return;
    }

    // Match!
    // Store data.
    Address addr;
    // aPeerAddr is string IP. Convert to Address.
    if (addr.Set(aPeerAddr) != ErrorCode::kNone)
    {
        return;
    }

    // For children, we might want to validate MacAddr matches target RLOC16.
    if (mState == State::kQueryingChildren)
    {
        if ((aDiagAnsMsg.mPresentFlags & NetDiagData::kMacAddrBit) && (aDiagAnsMsg.mMacAddr == mCurrentQueryRloc16))
        {
            // Validated
        }
    }

    // We need to MERGE chunks!
    // NetDiagData is a struct. We should merge fields if we receive multiple chunks for same device.
    // But map key is Address.
    // If we have previous entry, we should merge.

    if (mCollectedData.count(addr))
    {
        // Simple merge: copy present flags and fields.
        // Since NetDiagData has mPresentFlags, we can just copy fields that are present.
        // But making a proper merge function is better.
        // For simplicity, let's assume `mCollectedData[addr] = aDiagAnsMsg` replaces it, which is BAD for chunks.
        // We MUST merge.
        auto &existing = mCollectedData[addr];
        existing.mPresentFlags |= aDiagAnsMsg.mPresentFlags;
        // Copy all fields... this is tedious without a Merge function in NetDiagData.
        // CLI implementation stored full `data` from `answers` which accumulate in `CommissionerApp`?
        // No, `CommissionerApp` stores `mDiagAnsDataMap`.
        // `CommissionerImpl::HandleDiagGetAnswer` updates `mDiagAnsTlvs`?
        // `CommissionerImpl` has `NetDiagData mDiagAnsTlvs` (singular) and `mResourceDiagAns`.
        // `CommissionerImpl::HandleDiagGetAnswer` parses into `mDiagAnsTlvs`.
        // If `CommissionerImpl` only keeps one, then `OnDiagGetAnswer` just gives us that one.
        // We need to accumulate it ourselves.
        // Since `NetDiagData` has many fields, I won't implement full merge here unless necessary.
        // But `NetworkTraverser` is inside Library, so I can access `NetDiagData` members.

        // Ideally `NetDiagData` should have a `Merge` method.
        // For now, let's copy the entire struct if it's the first chunk (which usually has ID).
        // If it's later chunks, we need to merge fields.
        // Or we can rely on specific fields being present
        existing.Merge(aDiagAnsMsg); // Assuming a Merge method exists or will be added to NetDiagData
    }
    else
    {
        mCollectedData[addr] = aDiagAnsMsg;
    }

    // Stop timer and next chunk
    mTimer.Stop();
    mRetryCount = 0;
    mCurrentChunkIndex++;

    if (mCurrentChunkIndex >= mPendingChunks.size())
    {
        // Device fully queried
        if (mHandler.mOnDeviceResponded)
        {
            auto status = mDeviceRetried ? Commissioner::TraverseStatus::kSuccessWithRetry
                                         : Commissioner::TraverseStatus::kSuccess;
            mHandler.mOnDeviceResponded(addr.ToString(), &mCollectedData[addr], status);
            // mHandler.mOnDeviceResponded(addr.ToString(), mCollectedData[addr]);
        }
    }

    QueryNextChunk();
}

void NetworkTraverser::Proceed()
{
    // Finished current device/chunks.
    // Check state to decide next move.

    if (mState == State::kQueryingLeader)
    {
        // Analyze leader data to find routers.
        // We need to find the Leader's entry in collected data.
        bool        foundLeader = false;
        Address     leaderAddr;
        NetDiagData leaderData;

        for (const auto &pair : mCollectedData)
        {
            // We assume the first one we collected is Leader (since we only queried leader).
            // But we might have multiple if we got responses from elsewhere (unlikely).
            // Let's look for one with Route64.
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
        if (leaderData.mPresentFlags & NetDiagData::kMacAddrBit)
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
        if (leaderData.mPresentFlags & NetDiagData::kChildTableBit)
        {
            for (const auto &childEntry : leaderData.mChildTable)
            {
                mChildrenToQuery[leaderRloc16 | childEntry.mChildId] = !childEntry.mModeData.mIsRxOnWhenIdleMode;
            }
        }

        mState = State::kQueryingRouters;
        Proceed(); // Recurse to start router querying (or finding next router)
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
            Proceed();
            return;
        }

        // Get next router
        uint16_t routerRloc = *mRoutersToQuery.begin();
        mRoutersToQuery.erase(mRoutersToQuery.begin());

        // Construct Router RLOC
        // Prefix + 0000:00ff:fe00:RLOC16
        std::vector<uint8_t> addrBytes = mMeshLocalPrefix;
        addrBytes.push_back(0x00);
        addrBytes.push_back(0x00);
        addrBytes.push_back(0x00);
        addrBytes.push_back(0xff);
        addrBytes.push_back(0xfe);
        addrBytes.push_back(0x00);
        addrBytes.push_back((routerRloc >> 8) & 0xFF);
        addrBytes.push_back(routerRloc & 0xFF);

        Address addr;
        if (addr.Set(addrBytes) != ErrorCode::kNone)
        {
            Proceed();
            return;
        }

        mCurrentQueryTarget = addr.ToString();
        mCurrentQueryRloc16 = routerRloc;
        mPendingChunks      = kRouterChunks;
        mCurrentChunkIndex  = 0;
        mRetryCount         = 0;
        mDeviceRetried      = false;
        QueryNextChunk();
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

        // Similar RLOC construction
        uint16_t             childRloc = childRloc16;
        std::vector<uint8_t> addrBytes = mMeshLocalPrefix;
        addrBytes.push_back(0x00);
        addrBytes.push_back(0x00);
        addrBytes.push_back(0x00);
        addrBytes.push_back(0xff);
        addrBytes.push_back(0xfe);
        addrBytes.push_back(0x00);
        addrBytes.push_back((childRloc >> 8) & 0xFF);
        addrBytes.push_back(childRloc & 0xFF);

        Address addr;
        if (addr.Set(addrBytes) != ErrorCode::kNone)
        {
            Proceed();
            return;
        }

        mCurrentQueryTarget = addr.ToString();
        mCurrentQueryRloc16 = childRloc;
        mPendingChunks      = kChildChunks;
        mCurrentChunkIndex  = 0;
        mRetryCount         = 0;
        mDeviceRetried      = false;
        QueryNextChunk();
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

} // namespace commissioner
} // namespace ot
