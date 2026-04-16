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

#ifndef OT_COMM_LIBRARY_NETWORK_TRAVERSER_HPP_
#define OT_COMM_LIBRARY_NETWORK_TRAVERSER_HPP_

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "commissioner/commissioner.hpp"
#include "commissioner/network_data.hpp"
#include "commissioner/network_diag_data.hpp"
#include "common/address.hpp"
#include "library/timer.hpp"

namespace ot {
namespace commissioner {

class CommissionerImpl;

/**
 * @brief Network Traverser
 *
 * This class implements the logic to traverse the Thread network and collect
 * diagnostic information from all nodes (Leader, Routers, and Children).
 *
 * The traversal process is asynchronous and state-based:
 * 1. It starts by fetching the Active Dataset to get the Mesh Local Prefix.
 * 2. It queries the Leader (ALOC 0xfc00) for its RLOC16, Route64, and Child Table.
 * 3. Based on the Route64 data, it identifies all Routers in the network.
 * 4. It iterates through each Router, querying for its Child Table.
 * 5. It collects a list of all Children (both from Leader's and Routers' tables).
 * 6. Finally, it queries each Child for its diagnostic data.
 *
 * Diagnostic queries are split into multiple chunks to avoid fragmentation limits
 * and ensure reliability. The collected data is aggregated and returned via a callback.
 */
class NetworkTraverser
{
public:
    using TraverseCallback = std::function<void(const std::map<std::string, NetDiagData> *aReport, Error aError)>;

    explicit NetworkTraverser(CommissionerImpl &aImpl);

    Error Start(Commissioner::TraverseHandler aHandler);
    void  Stop();

    bool IsActive() const { return mState != State::kIdle; }

    static size_t GetLeaderChunkCount();
    static size_t GetRouterChunkCount();
    static size_t GetChildChunkCount();

private:
    friend class CommissionerImpl;
    friend class NetworkTraverserTest;

    void OnActiveDataset(const ActiveOperationalDataset *aDataset, Error aError);
    void OnDiagGetAnswer(const std::string &aPeerAddr, const NetDiagData &aDiagAnsMsg);

    enum class State
    {
        kIdle,
        kGettingDataset,
        kFallbackPrefixDiscovery,
        kFallbackRoute64Discovery,
        kQueryingLeader,
        kQueryingRouters,
        kQueryingChildren,
    };

    void HandleTimer(Timer &aTimer);

    /**
     * @brief Starts the fallback mode to discover Mesh Local Prefix.
     *
     * This sends a multicast DIAG_GET.qry to all routers to learn the prefix
     * from the source address of the response.
     */
    void StartFallbackPrefixDiscovery();

    /**
     * @brief Proceeds to query the leader after obtaining the prefix.
     */
    void ProceedToQueryLeader();
    void FinalizeNode();
    void Finalize(Error aError);

    void QueryNextTarget();
    void QueryChunk();

    static std::string AddressToString(const Address &aAddr);

    Address GetMeshLocalAddress(uint16_t aRloc16) const;

    CommissionerImpl             &mImpl;
    Timer                         mRequestTimeoutTimer;
    Commissioner::TraverseHandler mHandler;
    State                         mState;
    ByteArray                     mMeshLocalPrefix;
    bool                          mIgnoreMeshLocalPrefixForTest = false;
    bool                          mIgnoreRoute64ForTest = false;

    NetworkData mSharedNetworkData;
    bool        mHasSharedNetworkData = false;

    std::map<Address, NetDiagData> mCollectedData;

    // Discovery Progress
    std::string           mCurrentQueryTarget;
    Address               mCurrentQueryAddr;
    uint16_t              mCurrentQueryRloc16 = 0xFFFF;
    std::vector<uint64_t> mPendingChunks;
    size_t                mCurrentChunkIndex;
    int                   mRetryCount;
    bool                  mDeviceRetried;

    // Topology
    std::set<uint16_t>       mRoutersToQuery;
    std::map<uint16_t, bool> mChildrenToQuery; // RLOC16 -> isSleepy

    // Constants
    static const int kDefaultTimeoutMs;
    static const int kSleepyTimeoutMs;
    static const int kMaxRetries;
};

} // namespace commissioner
} // namespace ot

#endif // OT_COMM_LIBRARY_NETWORK_TRAVERSER_HPP_
