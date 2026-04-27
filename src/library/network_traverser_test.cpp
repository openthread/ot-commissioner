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

#include "commissioner/defines.hpp"
#include "commissioner/error.hpp"
#include "commissioner/network_data.hpp"
#include "commissioner/network_diag_data.hpp"
#include "event2/event.h"
#include "library/network_traverser.hpp"

#include <cstdint>
#include <functional>
#include <gtest/gtest.h>
#include <map>
#include <string>

#include "commissioner/commissioner.hpp"
#include "common/error_macros.hpp"
#include "library/commissioner_impl.hpp"

namespace ot {
namespace commissioner {

class MockCommissionerImpl : public CommissionerImpl
{
public:
    MockCommissionerImpl(CommissionerHandler &aHandler, struct event_base *aEventBase)
        : CommissionerImpl(aHandler, aEventBase)
    {
    }

    // Mock GetActiveDataset
    Commissioner::Handler<ActiveOperationalDataset> mGetActiveDatasetHandler;
    void GetActiveDataset(Commissioner::Handler<ActiveOperationalDataset> aHandler,
                          uint16_t /* aDatasetFlags */) override
    {
        mGetActiveDatasetHandler = aHandler;
    }

    // Mock CommandDiagGetQuery
    using CommandDiagGetQueryCallback = std::function<void(Commissioner::ErrorHandler, const std::string &, uint64_t)>;
    CommandDiagGetQueryCallback mCommandDiagGetQueryCallback;
    void                        CommandDiagGetQuery(Commissioner::ErrorHandler aHandler,
                                                    const std::string         &aAddr,
                                                    uint64_t                   aDiagDataFlags) override
    {
        if (mCommandDiagGetQueryCallback)
        {
            mCommandDiagGetQueryCallback(aHandler, aAddr, aDiagDataFlags);
        }
    }
};

class NetworkTraverserTest : public ::testing::Test
{
public:
    NetworkTraverserTest()
        : mEventBase(event_base_new())
        , mCommissioner(mHandler, mEventBase)
        , mTraverser(mCommissioner)
    {
    }

    ~NetworkTraverserTest() override { event_base_free(mEventBase); }

    struct event_base *mEventBase;
    struct DummyHandler : public CommissionerHandler
    {
        std::string OnJoinerRequest(const ByteArray &) override { return ""; }
        void        OnJoinerConnected(const ByteArray &, Error) override {}
        bool        OnJoinerFinalize(const ByteArray &,
                                     const std::string &,
                                     const std::string &,
                                     const std::string &,
                                     const ByteArray &,
                                     const std::string &,
                                     const ByteArray &) override
        {
            return true;
        }
        void OnPanIdConflict(const std::string &, const ChannelMask &, uint16_t) override {}
        void OnEnergyReport(const std::string &, const ChannelMask &, const ByteArray &) override {}
        void OnDatasetChanged() override {}
        void OnDiagGetAnswerMessage(const std::string &, const NetDiagData &) override {}
    } mHandler;

    struct TestTraverseHandler : public Commissioner::TraverseHandler
    {
        std::function<void(const std::map<std::string, NetDiagData> *, Error)> mOnFinished;

        void OnFinished(const std::map<std::string, NetDiagData> *aReport, Error aError) override
        {
            if (mOnFinished)
            {
                mOnFinished(aReport, aError);
            }
        }
    };

    MockCommissionerImpl mCommissioner;
    NetworkTraverser     mTraverser;

    // Helper to simulate receiving answer
    void SimulateDiagAnswer(const std::string &aPeerAddr, const NetDiagData &aData)
    {
        mTraverser.OnDiagGetAnswer(aPeerAddr, aData);
    }

    void TriggerTimeout() { mTraverser.HandleTimer(mTraverser.mRequestTimeoutTimer); }

    void SetIgnoreMeshLocalPrefixForTest(bool aIgnore)
    {
        mTraverser.mIgnoreMeshLocalPrefixForTest = aIgnore;
    }
};

TEST_F(NetworkTraverserTest, Start_InitiatesDatasetQuery)
{
    TestTraverseHandler handler;
    EXPECT_EQ(mTraverser.Start(handler), ErrorCode::kNone);
    EXPECT_TRUE(mCommissioner.mGetActiveDatasetHandler != nullptr);
    EXPECT_TRUE(mTraverser.IsActive());
}

TEST_F(NetworkTraverserTest, Traverse_Flow_LeaderOnly)
{
    TestTraverseHandler handler;
    bool                               finished = false;
    Error                              finishError;
    std::map<std::string, NetDiagData> resultReport;

    handler.mOnFinished = [&](const std::map<std::string, NetDiagData> *aReport, Error aError) {
        finished    = true;
        finishError = aError;
        if (aReport)
        {
            resultReport = *aReport;
        }
    };

    EXPECT_EQ(mTraverser.Start(handler), ErrorCode::kNone);

    // 1. Return Active Dataset (Mesh Local Prefix)
    ActiveOperationalDataset dataset;
    dataset.mMeshLocalPrefix = {0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};

    // Capture Query calls
    std::string lastQueryAddr;
    uint64_t    lastQueryFlags;
    int         queryCount = 0;

    mCommissioner.mCommandDiagGetQueryCallback = [&](Commissioner::ErrorHandler, const std::string &aAddr,
                                                     uint64_t aFlags) {
        lastQueryAddr  = aAddr;
        lastQueryFlags = aFlags;
        queryCount++;
    };

    mCommissioner.mGetActiveDatasetHandler(&dataset, ERROR_NONE);

    // Should now be querying Leader (ALOC)
    // RLOC16 0xFC00 -> Leader
    // Addr: Prefix + 0000:00ff:fe00:fc00
    // fd00:0000:0000:0001:0000:00ff:fe00:fc00
    EXPECT_EQ(queryCount, 1);
    EXPECT_NE(lastQueryAddr.find("fc00"), std::string::npos);

    // 2. Respond with Leader Data Chunks
    // Chunk 0: kRoute64Bit | kMacAddrBit | kEui64Bit
    NetDiagData chunk0;
    chunk0.mPresentFlags = NetDiagData::kRoute64Bit | NetDiagData::kMacAddrBit;
    chunk0.mMacAddr      = 0xFC00; // Leader
    // Route64: Leader only
    chunk0.mRoute64.mMask.assign(9, 0);
    // Leader RLOC16 0xFC00 >> 10 = 63. Bit ID 63.
    // Bit index logic in NetworkTraverser::Proceed:
    // routerId / 8 = byteIdx. 63/8 = 7.
    // 7 - (63%8) = 7 - 7 = 0.
    // So byte 7, bit 0 (value 1).
    chunk0.mRoute64.mMask[7] = 1;

    SimulateDiagAnswer(lastQueryAddr, chunk0);
    EXPECT_EQ(queryCount, 2); // Next chunk

    // Simulate remaining chunks
    for (size_t i = 1; i < NetworkTraverser::GetLeaderChunkCount(); ++i)
    {
        NetDiagData chunk;
        chunk.mPresentFlags = lastQueryFlags;
        if (chunk.mPresentFlags & NetDiagData::kLeaderDataBit)
        {
            chunk.mLeaderData.mRouterId = 63; // Leader ID
        }
        SimulateDiagAnswer(lastQueryAddr, chunk);
        if (i < NetworkTraverser::GetLeaderChunkCount() - 1)
        {
            EXPECT_EQ(queryCount, 2 + i);
        }
    }

    // After last chunk, it should finish immediately because no other routers/children
    EXPECT_TRUE(finished);
    EXPECT_EQ(finishError, ErrorCode::kNone);
    EXPECT_EQ(resultReport.size(), 1);
    EXPECT_EQ(resultReport.begin()->first, lastQueryAddr);
}

TEST_F(NetworkTraverserTest, Traverse_Fail_ActiveDataset)
{
    TestTraverseHandler handler;
    bool                          finished = false;
    Error                         finishError;

    handler.mOnFinished = [&](const std::map<std::string, NetDiagData> *, Error aError) {
        finished    = true;
        finishError = aError;
    };

    EXPECT_EQ(mTraverser.Start(handler), ErrorCode::kNone);

    // Fail dataset
    mCommissioner.mGetActiveDatasetHandler(nullptr, ERROR_NOT_FOUND("Simulated failure"));

    EXPECT_TRUE(finished);
    EXPECT_EQ(finishError, ErrorCode::kNotFound);
}

TEST_F(NetworkTraverserTest, Traverse_Fallback_Prefix_Discovery)
{
    TestTraverseHandler handler;
    bool                               finished = false;
    Error                              finishError;
    std::map<std::string, NetDiagData> resultReport;

    handler.mOnFinished = [&](const std::map<std::string, NetDiagData> *aReport, Error aError) {
        finished    = true;
        finishError = aError;
        if (aReport)
        {
            resultReport = *aReport;
        }
    };

    SetIgnoreMeshLocalPrefixForTest(true);

    EXPECT_EQ(mTraverser.Start(handler), ErrorCode::kNone);

    // 1. Return Active Dataset with prefix (should be ignored)
    ActiveOperationalDataset dataset;
    dataset.mMeshLocalPrefix = {0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};

    std::string lastQueryAddr;
    uint64_t    lastQueryFlags;
    int         queryCount = 0;

    mCommissioner.mCommandDiagGetQueryCallback = [&](Commissioner::ErrorHandler, const std::string &aAddr,
                                                     uint64_t aFlags) {
        lastQueryAddr  = aAddr;
        lastQueryFlags = aFlags;
        queryCount++;
    };

    mCommissioner.mGetActiveDatasetHandler(&dataset, ERROR_NONE);

    // Should now be discovering prefix via multicast
    EXPECT_EQ(queryCount, 1);
    EXPECT_EQ(lastQueryAddr, "ff03::2");
    EXPECT_EQ(lastQueryFlags, NetDiagData::kAddrsBit);

    // 2. Simulate response from a router
    // The response address must contain the prefix we want to discover.
    std::string responderAddr = "fd00:0000:0000:0002:0000:00ff:fe00:0400";
    NetDiagData chunk0;
    chunk0.mPresentFlags = NetDiagData::kMacAddrBit;
    chunk0.mMacAddr = 0x0400;

    SimulateDiagAnswer(responderAddr, chunk0);

    // After discovering prefix, it should proceed to query Leader.
    EXPECT_EQ(queryCount, 2);
    EXPECT_NE(lastQueryAddr.find("fc00"), std::string::npos);

    Address leaderAddr;
    EXPECT_EQ(leaderAddr.Set(lastQueryAddr), ErrorCode::kNone);
    auto raw = leaderAddr.GetRaw();
    ASSERT_EQ(raw.size(), 16);
    EXPECT_EQ(raw[0], 0xfd);
    EXPECT_EQ(raw[7], 0x02);

    // Force finish to avoid running full flow
    mTraverser.Stop();
    EXPECT_TRUE(finished);
}

TEST_F(NetworkTraverserTest, Traverse_Fallback_Route64_Discovery)
{
    TestTraverseHandler handler;
    bool                               finished = false;
    Error                              finishError;
    std::map<std::string, NetDiagData> resultReport;

    handler.mOnFinished = [&](const std::map<std::string, NetDiagData> *aReport, Error aError) {
        finished    = true;
        finishError = aError;
        if (aReport)
        {
            resultReport = *aReport;
        }
    };

    EXPECT_EQ(mTraverser.Start(handler), ErrorCode::kNone);

    // 1. Return Active Dataset
    ActiveOperationalDataset dataset;
    dataset.mMeshLocalPrefix = {0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};

    std::string lastQueryAddr;
    uint64_t    lastQueryFlags;
    int         queryCount = 0;

    mCommissioner.mCommandDiagGetQueryCallback = [&](Commissioner::ErrorHandler, const std::string &aAddr,
                                                     uint64_t aFlags) {
        lastQueryAddr  = aAddr;
        lastQueryFlags = aFlags;
        queryCount++;
    };

    mCommissioner.mGetActiveDatasetHandler(&dataset, ERROR_NONE);

    // Should now be querying Leader (ALOC)
    EXPECT_EQ(queryCount, 1);
    EXPECT_NE(lastQueryAddr.find("fc00"), std::string::npos);

    // We need to simulate answers for ALL leader chunks to trigger FinalizeNode()
    int leaderChunkCount = NetworkTraverser::GetLeaderChunkCount();
    
    for (int i = 0; i < leaderChunkCount; ++i)
    {
        NetDiagData chunk;
        if (i == 0)
        {
            chunk.mPresentFlags = NetDiagData::kMacAddrBit; // Missing Route64
            chunk.mMacAddr      = 0xFC00;
        }
        else
        {
            chunk.mPresentFlags = lastQueryFlags; // Echo flags to pass filter
        }
        SimulateDiagAnswer(lastQueryAddr, chunk);
    }

    // Should now be in Fallback Route64 Discovery querying ff03::2
    EXPECT_EQ(queryCount, 1 + leaderChunkCount);
    EXPECT_EQ(lastQueryAddr, "ff03::2");
    EXPECT_EQ(lastQueryFlags, NetDiagData::kRoute64Bit);

    // 3. Simulate response from a router with Route64
    std::string responderAddr = "fd00:0000:0000:0001:0000:00ff:fe00:0400";
    NetDiagData routeData;
    routeData.mPresentFlags = NetDiagData::kRoute64Bit;
    routeData.mRoute64.mMask.assign(9, 0);
    routeData.mRoute64.mMask[0] |= (1 << 6); // Router ID 1 (RLOC 0x0400)

    SimulateDiagAnswer(responderAddr, routeData);

    // After discovering Route64, it should proceed to query routers.
    // Router ID 1 RLOC16 = 0x0400.
    EXPECT_EQ(queryCount, 2 + leaderChunkCount);
    EXPECT_TRUE(lastQueryAddr.find(":400") != std::string::npos || lastQueryAddr.find(":0400") != std::string::npos);

    // Force finish to avoid running full flow
    mTraverser.Stop();
    EXPECT_TRUE(finished);
}

TEST_F(NetworkTraverserTest, Traverse_Flow_RoutersAndChildren)
{
    TestTraverseHandler handler;
    bool                               finished = false;
    Error                              finishError;
    std::map<std::string, NetDiagData> resultReport;

    handler.mOnFinished = [&](const std::map<std::string, NetDiagData> *aReport, Error aError) {
        finished    = true;
        finishError = aError;
        if (aReport)
        {
            resultReport = *aReport;
        }
    };

    EXPECT_EQ(mTraverser.Start(handler), ErrorCode::kNone);

    // 1. Active Dataset
    ActiveOperationalDataset dataset;
    dataset.mMeshLocalPrefix = {0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};

    std::string lastQueryAddr;
    uint64_t    lastQueryFlags;

    mCommissioner.mCommandDiagGetQueryCallback = [&](Commissioner::ErrorHandler, const std::string &aAddr,
                                                     uint64_t aFlags) {
        lastQueryAddr  = aAddr;
        lastQueryFlags = aFlags;
    };

    mCommissioner.mGetActiveDatasetHandler(&dataset, ERROR_NONE);

    // Now querying Leader (0xFC00).

    // 2. Leader Response
    // Make leader have 1 Router (ID 1) and 1 Child (ID 2).
    NetDiagData leaderData;
    leaderData.mPresentFlags = NetDiagData::kRoute64Bit | NetDiagData::kMacAddrBit | NetDiagData::kChildTableBit;
    leaderData.mMacAddr      = 0xFC00;

    // Route64: Leader (ID 63) + Router (ID 1).
    // Router ID 1: 1 % 8 = 1. 7-1 = 6. Bit 6 of byte 0.
    // Leader ID 63: Leading 63/8=7. Bit 7 of byte 7. Wait, earlier logical said byte 7 bit 0?
    // NetworkTraverser::Proceed logic:
    // bitIdx = 7 - (routerId % 8)
    // 63 % 8 = 7. 7 - 7 = 0. So bit 0.
    // byteIdx = 63 / 8 = 7.
    // So byte 7, bit 0. (value 1)
    leaderData.mRoute64.mMask.assign(9, 0);
    leaderData.mRoute64.mMask[0] |= (1 << 6); // Router ID 1
    leaderData.mRoute64.mMask[7] |= (1 << 0); // Leader ID 63

    // ChildTable: Child ID 2.
    ChildTableEntry childEntry;
    childEntry.mChildId                      = 2;
    childEntry.mModeData.mIsRxOnWhenIdleMode = true; // Not sleepy
    leaderData.mChildTable.push_back(childEntry);

    // Verify start of Leader Query
    EXPECT_NE(lastQueryAddr.find("fc00"), std::string::npos);

    // Feed Leader Chunks
    for (size_t i = 0; i < NetworkTraverser::GetLeaderChunkCount(); ++i)
    {
        NetDiagData chunk;
        if (i == 0)
        {
            // First chunk (Topology) usually requested.
            // Feed full topology data here.
            chunk = leaderData;
        }
        chunk.mPresentFlags |= lastQueryFlags; // Ensure it passes filter

        if (chunk.mPresentFlags & NetDiagData::kLeaderDataBit)
        {
            chunk.mLeaderData.mRouterId = 63; // Leader ID
        }

        SimulateDiagAnswer(lastQueryAddr, chunk);
    }

    // 3. Router Query
    // After Leader, it should process Route64 and query Router ID 1.
    // Router ID 1 RLOC16 = 1 << 10 = 0x0400.
    // Addr: ...0400
    // Verify query
    // IPv6 string representation suppresses leading zeros: 0400 -> 400
    EXPECT_TRUE(lastQueryAddr.find(":400") != std::string::npos || lastQueryAddr.find(":0400") != std::string::npos);

    // Respond for Router
    NetDiagData routerData;
    routerData.mPresentFlags = NetDiagData::kMacAddrBit | NetDiagData::kChildTableBit;
    routerData.mMacAddr      = 0x0400;
    // Router has no children for simplicity.

    // Feed Router Chunks
    for (size_t i = 0; i < NetworkTraverser::GetRouterChunkCount(); ++i)
    {
        NetDiagData chunk;
        if (i == 0)
            chunk = routerData;
        chunk.mPresentFlags |= lastQueryFlags;
        SimulateDiagAnswer(lastQueryAddr, chunk);
    }

    // 4. Child Query
    // After Router, it should query Children.

    EXPECT_NE(lastQueryAddr.find("fc02"), std::string::npos);

    // Respond for Child
    NetDiagData childData;
    childData.mPresentFlags = NetDiagData::kMacAddrBit;
    childData.mMacAddr      = 0xFC02;

    // Feed Child Chunks
    for (size_t i = 0; i < NetworkTraverser::GetChildChunkCount(); ++i)
    {
        NetDiagData chunk;
        if (i == 0)
            chunk = childData;
        chunk.mPresentFlags |= lastQueryFlags;
        SimulateDiagAnswer(lastQueryAddr, chunk);
    }

    // 5. Wrap up
    EXPECT_TRUE(finished);
    EXPECT_EQ(finishError, ErrorCode::kNone);
    EXPECT_EQ(resultReport.size(), 3); // Leader, Router, Child
}

TEST_F(NetworkTraverserTest, Diff_Chunks_Merged)
{
    TestTraverseHandler handler;
    bool                               finished = false;
    Error                              finishError;
    std::map<std::string, NetDiagData> resultReport;

    handler.mOnFinished = [&](const std::map<std::string, NetDiagData> *aReport, Error aError) {
        finished    = true;
        finishError = aError;
        if (aReport)
        {
            resultReport = *aReport;
        }
    };

    EXPECT_EQ(mTraverser.Start(handler), ErrorCode::kNone);

    ActiveOperationalDataset dataset;
    dataset.mMeshLocalPrefix = {0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};

    std::string lastQueryAddr;
    uint64_t    lastQueryFlags;

    mCommissioner.mCommandDiagGetQueryCallback = [&](Commissioner::ErrorHandler, const std::string &aAddr,
                                                     uint64_t aFlags) {
        lastQueryAddr  = aAddr;
        lastQueryFlags = aFlags;
    };

    mCommissioner.mGetActiveDatasetHandler(&dataset, ERROR_NONE);

    // Leader Query

    // Chunk 0: Has MacAddr
    NetDiagData chunk0;
    chunk0.mPresentFlags = NetDiagData::kRoute64Bit | NetDiagData::kMacAddrBit;
    chunk0.mMacAddr      = 0xFC00;
    // Set minimal route data to finish safely
    chunk0.mRoute64.mMask.assign(9, 0);
    chunk0.mRoute64.mMask[7] = 1;

    SimulateDiagAnswer(lastQueryAddr, chunk0);

    // Chunk 8: Battery Level (kBatteryLevelBit)
    // We expect this to MERGE with chunk0 data.
    NetDiagData chunk8;
    chunk8.mPresentFlags = NetDiagData::kBatteryLevelBit;
    chunk8.mBatteryLevel = 88;

    // Simulate iterating to chunk 8
    // We need to skip chunks 1-7.
    // The test framework iterates based on received answers? No, OnDiagGetAnswer calls QueryNextChunk.
    // So we must answer every chunk.

    // Chunk 1-7: Empty placeholders
    for (int i = 1; i < 8; ++i)
    {
        NetDiagData empty;
        empty.mPresentFlags = lastQueryFlags;
        if (empty.mPresentFlags & NetDiagData::kLeaderDataBit)
        {
            empty.mLeaderData.mRouterId = 63; // Leader ID
        }
        SimulateDiagAnswer(lastQueryAddr, empty);
    }

    // Now at Chunk 8 (Battery)
    SimulateDiagAnswer(lastQueryAddr, chunk8);

    // Chunk 9
    NetDiagData chunk9;
    chunk9.mPresentFlags = lastQueryFlags;
    SimulateDiagAnswer(lastQueryAddr, chunk9);

    EXPECT_TRUE(finished);
    ASSERT_EQ(resultReport.size(), 1);

    NetDiagData finalData = resultReport.begin()->second;

    // Verify Merge
    // Should have MacAddr from Chunk 0
    EXPECT_TRUE(finalData.mPresentFlags & NetDiagData::kMacAddrBit);
    EXPECT_EQ(finalData.mMacAddr, 0xFC00);

    // Should have Battery from Chunk 8
    EXPECT_TRUE(finalData.mPresentFlags & NetDiagData::kBatteryLevelBit);
    EXPECT_EQ(finalData.mBatteryLevel, 88);
}

TEST_F(NetworkTraverserTest, Traverse_Stop)
{
    TestTraverseHandler handler;
    bool                          finished    = false;
    Error                         finishError = ERROR_NONE;

    handler.mOnFinished = [&](const std::map<std::string, NetDiagData> *, Error aError) {
        finished    = true;
        finishError = aError;
    };

    EXPECT_EQ(mTraverser.Start(handler), ErrorCode::kNone);
    EXPECT_TRUE(mTraverser.IsActive());

    mTraverser.Stop();

    EXPECT_TRUE(finished);
    EXPECT_EQ(finishError, ErrorCode::kCancelled);
    EXPECT_FALSE(mTraverser.IsActive());
}

TEST_F(NetworkTraverserTest, Traverse_Timeout_Retry_Limit)
{
    TestTraverseHandler handler;
    bool                          finished    = false;
    Error                         finishError = ERROR_NONE;

    handler.mOnFinished = [&](const std::map<std::string, NetDiagData> *, Error aError) {
        finished    = true;
        finishError = aError;
    };

    EXPECT_EQ(mTraverser.Start(handler), ErrorCode::kNone);

    // Provide Active Dataset to start querying Leader
    ActiveOperationalDataset dataset;
    dataset.mMeshLocalPrefix = {0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};

    int queryCount                             = 0;
    mCommissioner.mCommandDiagGetQueryCallback = [&](Commissioner::ErrorHandler, const std::string &, uint64_t) {
        queryCount++;
    };

    mCommissioner.mGetActiveDatasetHandler(&dataset, ERROR_NONE);

    // Initial query for Leader Chunk 0
    EXPECT_EQ(queryCount, 1);

    // Retry 1
    TriggerTimeout();
    EXPECT_EQ(queryCount, 2);

    // Retry 2
    TriggerTimeout();
    EXPECT_EQ(queryCount, 3);

    // Retry 3
    TriggerTimeout();
    EXPECT_EQ(queryCount, 4);

    // Retry 4 -> Max Retries Exceeded on Chunk 0 (ID)
    // Should give up on this node.
    // Since it is Leader, and we haven't found it, traverse should fail.
    TriggerTimeout(); // This triggers fallback to Route64 discovery
    EXPECT_EQ(queryCount, 5);

    // Fallback also needs to timeout
    TriggerTimeout(); // Fallback Retry 1
    TriggerTimeout(); // Fallback Retry 2
    TriggerTimeout(); // Fallback Retry 3
    TriggerTimeout(); // Fallback gives up

    EXPECT_EQ(queryCount, 8);
    EXPECT_TRUE(finished);
    EXPECT_EQ(finishError.GetCode(), ErrorCode::kNotFound);
}

TEST_F(NetworkTraverserTest, Traverse_Timeout_Skip_Chunk)
{
    TestTraverseHandler handler;
    bool                               finished    = false;
    Error                              finishError = ERROR_NONE;
    std::map<std::string, NetDiagData> resultReport;

    handler.mOnFinished = [&](const std::map<std::string, NetDiagData> *aReport, Error aError) {
        finished    = true;
        finishError = aError;
        if (aReport)
            resultReport = *aReport;
    };

    EXPECT_EQ(mTraverser.Start(handler), ErrorCode::kNone);

    ActiveOperationalDataset dataset;
    dataset.mMeshLocalPrefix = {0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};

    std::string lastQueryAddr;
    uint64_t    lastQueryFlags = 0;
    int         queryCount     = 0;

    mCommissioner.mCommandDiagGetQueryCallback = [&](Commissioner::ErrorHandler, const std::string &aAddr,
                                                     uint64_t aFlags) {
        lastQueryAddr  = aAddr;
        lastQueryFlags = aFlags;
        queryCount++;
    };

    mCommissioner.mGetActiveDatasetHandler(&dataset, ERROR_NONE);

    // Answer Chunk 0 (ID) for Leader
    NetDiagData chunk0;
    chunk0.mPresentFlags = NetDiagData::kRoute64Bit | NetDiagData::kMacAddrBit;
    chunk0.mMacAddr      = 0xFC00;
    // Set dummy route to avoid searching for routers
    chunk0.mRoute64.mMask.assign(9, 0);
    chunk0.mRoute64.mMask[7] = 1; // Just leader

    SimulateDiagAnswer(lastQueryAddr, chunk0);

    // Now on Chunk 1.
    int queriesBeforeTimeout = queryCount;

    // Retry Loop for Chunk 1
    // 3 retries allowed (kMaxRetries = 3)
    // Initial call was 1 (0 retries)
    // After 3 retries -> 4 total calls for this chunk.
    for (int i = 0; i < 3; ++i)
    {
        TriggerTimeout();
    }

    // We are at max retries for Chunk 1.
    // Next timeout should SKIP Chunk 1 and move to Chunk 2.

    TriggerTimeout();

    // Should have moved to Chunk 2 and issued a query
    EXPECT_GT(queryCount, queriesBeforeTimeout + 3);

    // Verify we didn't fail/finish yet (still have chunks left)
    EXPECT_FALSE(finished);

    // Force finish
    mTraverser.Stop();
    EXPECT_TRUE(finished);
}

} // namespace commissioner
} // namespace ot
