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

/**
 * @file
 *   The file implements CLI network traverser.
 */

#include "app/cli/traverser.hpp"
#include <chrono>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "app/cli/console.hpp"
#include "app/cli/interpreter.hpp"
#include "app/cli/job_manager.hpp"
#include "app/commissioner_app.hpp"
#include "app/json.hpp"
#include "commissioner/commissioner.hpp"
#include "commissioner/network_diag_data.hpp"
#include "common/address.hpp"
#include "common/error_macros.hpp"
#include "common/utils.hpp"
#include "fmt/format.h"

namespace ot {

namespace commissioner {

static const std::map<uint64_t, std::string> kDiagFlagNames = {
    {NetDiagData::kExtMacAddrBit, "extaddr"},
    {NetDiagData::kMacAddrBit, "rloc16"},
    {NetDiagData::kModeBit, "mode"},
    {NetDiagData::kRoute64Bit, "route64"},
    {NetDiagData::kLeaderDataBit, "leaderdata"},
    {NetDiagData::kAddrsBit, "addrs"},
    {NetDiagData::kChildTableBit, "childtable"},
    {NetDiagData::kEui64Bit, "eui64"},
    {NetDiagData::kMacCountersBit, "maccounters"},
    {NetDiagData::kChildIpv6AddrsInfoListBit, "childaddrs"},
    {NetDiagData::kNetworkDataBit, "networkdata"},
    {NetDiagData::kTimeoutBit, "timeout"},
    {NetDiagData::kConnectivityBit, "connectivity"},
    {NetDiagData::kBatteryLevelBit, "batterylevel"},
    {NetDiagData::kSupplyVoltageBit, "supplyvoltage"},
    {NetDiagData::kChannelPagesBit, "channelpages"},
    {NetDiagData::kTypeListBit, "typelist"},
    {NetDiagData::kMaxChildTimeoutBit, "maxchildtimeout"},
    {NetDiagData::kVersionBit, "version"},
    {NetDiagData::kVendorNameBit, "vendorname"},
    {NetDiagData::kVendorModelBit, "vendormodel"},
    {NetDiagData::kVendorSWVersionBit, "vendorswversion"},
    {NetDiagData::kThreadStackVersionBit, "threadstackversion"},
    {NetDiagData::kChildBit, "child"},
    {NetDiagData::kRouterNeighborBit, "routerneighbor"},
    {NetDiagData::kMleCountersBit, "mlecounters"},
    {NetDiagData::kVendorAppURLBit, "vendorappurl"},
    {NetDiagData::kNonPreferredChannelsMaskBit, "channelsmask"}};

Interpreter::Value Traverser::ProcessTraverseNetwork(Interpreter *aInterpreter, const Interpreter::Expression &aExpr)
{
    Interpreter::Value value;
    CommissionerAppPtr commissioner = nullptr;
    std::string        jsonFile     = "";

    Console::Write("ProcessTraverseNetwork called with " + std::to_string(aExpr.size()) + " args\n", Console::Color::kWhite);
    for (size_t i = 1; i < aExpr.size(); ++i)
    {
        if (aExpr[i] == "--json")
        {
            VerifyOrExit(i + 1 < aExpr.size(), value = ERROR_INVALID_ARGS("Missing JSON filename"));
            jsonFile = aExpr[++i];
            Console::Write("JSON output enabled: " + jsonFile + "\n", Console::Color::kWhite);
        }
        else if (aExpr[i][0] == '-')
        {
             ExitNow(value = ERROR_INVALID_ARGS("Invalid argument: {}", aExpr[i]));
        }
    }

    for (size_t i = 0; i < aInterpreter->mContext.mCommandKeys.size(); ++i)
    {
        if (aInterpreter->mContext.mCommandKeys[i] == "--json")
        {
            VerifyOrExit(i + 1 < aInterpreter->mContext.mCommandKeys.size(), value = ERROR_INVALID_ARGS("Missing JSON filename"));
            jsonFile = aInterpreter->mContext.mCommandKeys[++i];
            Console::Write("JSON output enabled: " + jsonFile + "\n", Console::Color::kWhite);
        }
    }

    SuccessOrExit(value = aInterpreter->mJobManager->GetSelectedCommissioner(commissioner));
    value = ProcessTraverseNetworkJob(aInterpreter, commissioner, aExpr, jsonFile);
exit:
    return value;
}

Interpreter::Value Traverser::ProcessTraverseNetworkJob(Interpreter *aInterpreter, CommissionerAppPtr &aCommissioner, const Interpreter::Expression &, const std::string &aJsonFile)
{
    Interpreter::Value value;
    std::stringstream  resultStream;
    std::string        mlp;
    std::string        leaderAloc;
    Address            leaderAddr;
    uint16_t           leaderRloc16 = 0xFFFF;
    Route64            route64;
    bool               foundRoute64 = false;
    size_t             routerCount  = 0;
    size_t             childCount   = 0;
    size_t             expectedChildCount = 0;
    std::set<uint16_t> expectedRouters;
    std::map<uint16_t, bool> expectedChildren; // RLOC16 -> isSleepy
    std::set<Address> processedNodes;
    std::set<uint16_t> routersToQuery;
    std::map<uint16_t, bool> childrenToQuery;


    std::map<Address, NetDiagData> collectedData;

    // Define chunks to minimize TLV size per message
    const std::vector<uint64_t> kLeaderChunks = {
        NetDiagData::kRoute64Bit | NetDiagData::kMacAddrBit | NetDiagData::kEui64Bit, // Chunk 0: Topology & ID
        NetDiagData::kExtMacAddrBit | NetDiagData::kModeBit | NetDiagData::kConnectivityBit, // Chunk 1: Connectivity
        NetDiagData::kLeaderDataBit | NetDiagData::kNetworkDataBit | NetDiagData::kNonPreferredChannelsMaskBit, // Chunk 2: Network Data
        NetDiagData::kChildTableBit | NetDiagData::kMaxChildTimeoutBit, // Chunk 3: Child Table
        NetDiagData::kChildIpv6AddrsInfoListBit, // Chunk 4: Child IPv6 Addresses
        NetDiagData::kChildBit, // Chunk 5: Child Info (Large)
        NetDiagData::kAddrsBit | NetDiagData::kRouterNeighborBit, // Chunk 6: Neighbors & Addresses
        NetDiagData::kMleCountersBit | NetDiagData::kMacCountersBit, // Chunk 7: Counters
        NetDiagData::kBatteryLevelBit | NetDiagData::kSupplyVoltageBit | NetDiagData::kVersionBit | NetDiagData::kChannelPagesBit | NetDiagData::kTypeListBit, // Chunk 8: Device Info
        NetDiagData::kVendorNameBit | NetDiagData::kVendorModelBit | NetDiagData::kVendorSWVersionBit | NetDiagData::kThreadStackVersionBit | NetDiagData::kVendorAppURLBit // Chunk 9: Vendor Info
    };

    const std::vector<uint64_t> kRouterChunks = {
        NetDiagData::kMacAddrBit | NetDiagData::kExtMacAddrBit | NetDiagData::kEui64Bit, // Chunk 0: ID
        NetDiagData::kModeBit | NetDiagData::kConnectivityBit | NetDiagData::kRoute64Bit | NetDiagData::kNonPreferredChannelsMaskBit, // Chunk 1: Connectivity & Topology
        NetDiagData::kChildTableBit | NetDiagData::kChildIpv6AddrsInfoListBit | NetDiagData::kMaxChildTimeoutBit, // Chunk 2: Child Table
        NetDiagData::kChildBit, // Chunk 3: Child Info (Large)
        NetDiagData::kAddrsBit | NetDiagData::kRouterNeighborBit, // Chunk 4: Neighbors & Addresses
        NetDiagData::kMleCountersBit | NetDiagData::kMacCountersBit, // Chunk 5: Counters
        NetDiagData::kBatteryLevelBit | NetDiagData::kSupplyVoltageBit | NetDiagData::kVersionBit | NetDiagData::kChannelPagesBit | NetDiagData::kTypeListBit, // Chunk 6: Device Info
        NetDiagData::kVendorNameBit | NetDiagData::kVendorModelBit | NetDiagData::kVendorSWVersionBit | NetDiagData::kThreadStackVersionBit | NetDiagData::kVendorAppURLBit // Chunk 7: Vendor Info
    };

    const std::vector<uint64_t> kChildChunks = {
        NetDiagData::kMacAddrBit | NetDiagData::kExtMacAddrBit | NetDiagData::kModeBit | NetDiagData::kEui64Bit, // Chunk 0: ID & Mode
        NetDiagData::kConnectivityBit | NetDiagData::kTimeoutBit | NetDiagData::kAddrsBit | NetDiagData::kNetworkDataBit | NetDiagData::kNonPreferredChannelsMaskBit, // Chunk 1: Connectivity & Network Data
        NetDiagData::kMleCountersBit | NetDiagData::kMacCountersBit, // Chunk 2: Counters
        NetDiagData::kBatteryLevelBit | NetDiagData::kSupplyVoltageBit | NetDiagData::kVersionBit | NetDiagData::kChannelPagesBit | NetDiagData::kTypeListBit, // Chunk 3: Device Info
        NetDiagData::kVendorNameBit | NetDiagData::kVendorModelBit | NetDiagData::kVendorSWVersionBit | NetDiagData::kThreadStackVersionBit | NetDiagData::kVendorAppURLBit // Chunk 4: Vendor Info
    };

    auto ReportMissingTlvs = [&](uint64_t aRequested, uint64_t aReceived) {
        uint64_t missing = aRequested & ~aReceived;
        if (missing == 0) return;

        std::string missingStr = "Missing TLVs: ";
        bool        first      = true;
        for (const auto &pair : kDiagFlagNames)
        {
            if (missing & pair.first)
            {
                if (!first) missingStr += ", ";
                missingStr += pair.second;
                first = false;
            }
        }
        Console::Write(missingStr + "\n", Console::Color::kYellow);
    };

    auto PollForFlags = [&](uint64_t aRequestedFlags, const Address *aExpectedAddr, int aTimeoutMs) -> const Address * {
        int intervalMs = 10;
        int steps = aTimeoutMs / intervalMs;
        for (int i = 0; i < steps; ++i)
        {
            if (aInterpreter->mCancelCommand)
                return nullptr;

            std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
            const auto &answers = aCommissioner->GetNetDiagTlvs();
            for (const auto &answer : answers)
            {
                if (aExpectedAddr != nullptr && answer.first != *aExpectedAddr)
                    continue;

                // Check if we received ANY of the requested flags
                if (answer.second.mPresentFlags & aRequestedFlags)
                {
                    return &answer.first;
                }
            }
        }
        return nullptr;
    };

    auto QueryDevice = [&](const std::string &aTarget, const std::string &aLabel, const std::vector<uint64_t> &aChunks,
                           Address &aDeviceAddr, uint64_t &aRequestedFlags, int aTimeoutMs,
                           std::function<bool(const Address &)> aValidator = nullptr) -> bool {
        aRequestedFlags = 0;
        bool deviceFound = false;

        for (size_t i = 0; i < aChunks.size(); ++i)
        {
            uint64_t chunk = aChunks[i];
            aRequestedFlags |= chunk;
            bool chunkReceived = false;

            if (aInterpreter->mCancelCommand) return false;

            aCommissioner->CommandDiagGetQuery(aTarget, chunk).IgnoreError();

            const Address *foundAddr = PollForFlags(chunk, (i == 0 ? nullptr : &aDeviceAddr), aTimeoutMs);

            if (foundAddr != nullptr)
            {
                if (i == 0)
                {
                    if (aValidator && !aValidator(*foundAddr))
                    {
                        // Validation failed
                    }
                    else
                    {
                        aDeviceAddr   = *foundAddr;
                        chunkReceived = true;
                    }
                }
                else
                {
                    chunkReceived = true;
                }

                if (chunkReceived)
                {
                    Console::Write(fmt::format("{} Chunk {} received", aLabel, i), Console::Color::kDefault);
                }
            }

            if (i == 0 && !chunkReceived) return false;
            deviceFound = true;
        }
        return deviceFound;
    };

    aCommissioner->ClearNetDiagTlvs();

    // 1. Query the leader
    uint64_t leaderRequestedFlags = 0;
    uint64_t leaderReceivedFlags  = 0;

    if (aCommissioner->GetMeshLocalPrefix(mlp) == ErrorCode::kNone)
    {
        // Resolve the full leader IPv6 address by combining the Mesh Local Prefix with the Leader Anycast Locator (0xFC00).
        (void)aCommissioner->GetMeshLocalAddr(leaderAloc, mlp, 0xFC00);
    }
    else
    {
        leaderAloc = "fc00";
    }

    Console::Write("Querying Leader: " + leaderAloc, Console::Color::kCyan);

    if (!QueryDevice(leaderAloc, "Leader", kLeaderChunks, leaderAddr, leaderRequestedFlags, 800))
    {
        ExitNow(value = std::string("No response from leader at " + leaderAloc));
    }

    // Scoping block is required here because ExitNow() macro acts as a 'goto exit'.
    // C++ forbids jumping over the initialization of variables with automatic storage duration
    // (like 'answers' and 'data') if they are still in scope at the target label.
    {
        const auto &answers = aCommissioner->GetNetDiagTlvs();
        if (answers.count(leaderAddr))
        {
            const auto &data = answers.at(leaderAddr);
            leaderReceivedFlags = data.mPresentFlags;

            collectedData[leaderAddr] = data;

            if (data.mPresentFlags & NetDiagData::kRoute64Bit)
            {
                route64      = data.mRoute64;
                foundRoute64 = true;
            }
            if (data.mPresentFlags & NetDiagData::kMacAddrBit)
            {
                leaderRloc16 = data.mMacAddr;
            }
            processedNodes.insert(leaderAddr);
            Console::Write("Leader Unicast Address: " + leaderAddr.ToString() + "\n", Console::Color::kGreen);
            Console::Write(fmt::format("Leader Data Summary: Flags=0x{:08X}, RLOC16=0x{:04X}",
                                       data.mPresentFlags, leaderRloc16), Console::Color::kCyan);
            ReportMissingTlvs(leaderRequestedFlags, leaderReceivedFlags);

            if (data.mPresentFlags & NetDiagData::kChildTableBit)
            {
                expectedChildCount += data.mChildTable.size();
                Console::Write(fmt::format("Leader 0x{:04X} has {} children\n", leaderRloc16, data.mChildTable.size()), Console::Color::kCyan);
                for (const auto &childEntry : data.mChildTable)
                {
                    expectedChildren[leaderRloc16 | childEntry.mChildId] = !childEntry.mModeData.mIsRxOnWhenIdleMode;
                }
            }
        }
    }

    if (!foundRoute64)
    {
        ExitNow(value = std::string("No Route64 information received from leader"));
    }

    // Extract routers from mask
    for (uint8_t routerId = 0; routerId < 64; ++routerId)
    {
        uint8_t byteIdx = routerId / 8;
        uint8_t bitIdx  = 7 - (routerId % 8);

        if (byteIdx < route64.mMask.size() && (route64.mMask[byteIdx] & (1 << bitIdx)))
        {
            uint16_t rloc16 = static_cast<uint16_t>(routerId) << 10;
            if (rloc16 == leaderRloc16)
            {
                routerCount++;
            }
            else
            {
                routersToQuery.insert(rloc16);
            }
            expectedRouters.insert(rloc16);
        }
    }

    Console::Write(fmt::format("Expected routers from mask: {}", expectedRouters.size()), Console::Color::kCyan);
    Console::Write("--- Discovering Network Routers ---", Console::Color::kCyan);

    // 2. Query Routers
    {
        std::vector<uint16_t> currentRouters(routersToQuery.begin(), routersToQuery.end());
        for (uint16_t rloc16 : currentRouters)
        {
            if (aInterpreter->mCancelCommand) break;

            aCommissioner->ClearNetDiagTlvs(); // Clear previous responses
            std::string routerTarget = fmt::format("0x{:04X}", rloc16);
            Address     routerAddr;
            bool        routerFound = false;
            uint64_t    routerRequestedFlags = 0;
            uint64_t    routerReceivedFlags  = 0;

            if (QueryDevice(routerTarget, "Router " + routerTarget, kRouterChunks, routerAddr, routerRequestedFlags, 800))
            {
                routerFound = true;
            }

            if (routerFound)
            {
                routerCount++;
                processedNodes.insert(routerAddr);
                routersToQuery.erase(rloc16);

                const auto &answers = aCommissioner->GetNetDiagTlvs();
                const auto &data    = answers.at(routerAddr);
                routerReceivedFlags = data.mPresentFlags;

                // Store collected data
                collectedData[routerAddr] = data;

                Console::Write("Router Address: " + routerAddr.ToString(), Console::Color::kGreen);
                Console::Write(fmt::format("Router Data Summary: Flags=0x{:08X}", data.mPresentFlags), Console::Color::kCyan);
                ReportMissingTlvs(routerRequestedFlags, routerReceivedFlags);

                if (data.mPresentFlags & NetDiagData::kChildTableBit)
                {
                    expectedChildCount += data.mChildTable.size();
                    Console::Write(fmt::format("Router 0x{:04X} has {} children", rloc16, data.mChildTable.size()), Console::Color::kCyan);
                    for (const auto &childEntry : data.mChildTable)
                    {
                        expectedChildren[rloc16 | childEntry.mChildId] = !childEntry.mModeData.mIsRxOnWhenIdleMode;
                    }
                }
            }
        }
    }

    if (!routersToQuery.empty())
    {
        Console::Write(fmt::format("Timed out querying {} routers.", routersToQuery.size()), Console::Color::kRed);
    }

    // 3. Query Children
    if (!expectedChildren.empty())
    {
        childrenToQuery = expectedChildren;
        Console::Write("--- Discovering Network Children ---", Console::Color::kCyan);

        std::vector<std::pair<uint16_t, bool>> currentChildren(childrenToQuery.begin(), childrenToQuery.end());
        for (const auto &entry : currentChildren)
        {
            if (aInterpreter->mCancelCommand) break;

            aCommissioner->ClearNetDiagTlvs(); // Clear previous responses
            uint16_t    childRloc   = entry.first;
            bool        isSleepy    = entry.second;
            std::string childTarget = fmt::format("0x{:04X}", childRloc);
            Address     childAddr;
            bool        childFound  = false;
            int         timeoutMs   = isSleepy ? 10000 : 1500;
            uint64_t    childRequestedFlags = 0;
            uint64_t    childReceivedFlags  = 0;

            auto validator = [&](const Address &aAddr) {
                const auto &answers = aCommissioner->GetNetDiagTlvs();
                if (answers.count(aAddr) == 0) return false;
                const auto &data = answers.at(aAddr);
                return (data.mPresentFlags & NetDiagData::kMacAddrBit) && (data.mMacAddr == childRloc);
            };

            if (QueryDevice(childTarget, "Child " + childTarget, kChildChunks, childAddr, childRequestedFlags, timeoutMs,
                            validator))
            {
                childFound = true;
            }

            if (childFound)
            {
                childCount++;
                processedNodes.insert(childAddr);
                childrenToQuery.erase(childRloc);

                const auto &answers = aCommissioner->GetNetDiagTlvs();
                const auto &data    = answers.at(childAddr);
                childReceivedFlags = data.mPresentFlags;

                // Store collected data
                collectedData[childAddr] = data;

                Console::Write(fmt::format("{} Child Address: {}", isSleepy ? "[Sleepy]" : "[Non-Sleepy]", childAddr.ToString()), Console::Color::kGreen);
                ReportMissingTlvs(childRequestedFlags, childReceivedFlags);
            }
        }

        if (!childrenToQuery.empty())
        {
             Console::Write(fmt::format("Timed out querying {} children.", childrenToQuery.size()), Console::Color::kRed);
        }
    }

    if (aInterpreter->mCancelCommand)
    {
        ExitNow(value = ERROR_CANCELLED("Traverse network cancelled by user"));
    }

    resultStream << "\n--- Traversal Summary ---\n";
    resultStream << "Expected routers from mask: " << expectedRouters.size() << "\n";
    resultStream << "Total routers responded:    " << routerCount << "\n";
    resultStream << "Total children from tables: " << expectedChildCount << "\n";
    resultStream << "Total children responded:   " << childCount << "\n";
    value = resultStream.str();

    if (!aJsonFile.empty())
    {
        Console::Write("Writing JSON to " + aJsonFile + " with " + std::to_string(processedNodes.size()) + " entries\n", Console::Color::kWhite);
        
        nlohmann::json report;
        
        // Summary Block
        report["summary"]["leader_addr"]        = leaderAddr.ToString();
        report["summary"]["expected_routers"]   = expectedRouters.size();
        report["summary"]["responded_routers"]  = routerCount;
        report["summary"]["expected_children"]  = expectedChildCount;
        report["summary"]["responded_children"] = childCount;

        // Devices Map
        nlohmann::json &devices = report["devices"];
        for (const auto &pair : collectedData)
        {
            const auto &addr = pair.first;
            const auto &data = pair.second;
            
            // We use NetDiagDataToJson to serialize each entry, but it returns a string.
            // We need to parse it back to json object to put into the report.
            std::string jsonStr = NetDiagDataToJson(data);
            try
            {
                devices[addr.ToString()] = nlohmann::json::parse(jsonStr);
            }
            catch (const std::exception &e)
            {
                Console::Write(fmt::format("Failed to parse JSON for {}: {}\n", addr.ToString(), e.what()), Console::Color::kRed);
            }
        }
        
        std::ofstream file(aJsonFile);
        if (file.is_open())
        {
            file << report.dump(4);
            Console::Write(fmt::format("JSON report written to {}\n", aJsonFile), Console::Color::kGreen);
        }
        else
        {
            Console::Write(fmt::format("Failed to open file {} for writing\n", aJsonFile), Console::Color::kRed);
        }
    }

exit:
    return value;
}

} // namespace commissioner

} // namespace ot
