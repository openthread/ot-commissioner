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

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "app/cli/traverser.hpp"

#include "app/cli/console.hpp"
#include "app/cli/interpreter.hpp"
#include "app/cli/job_manager.hpp"
#include "app/json.hpp"
#include "commissioner/commissioner.hpp"
#include "commissioner/error.hpp"
#include "commissioner/network_diag_data.hpp"
#include "common/error_macros.hpp"
#include "common/utils.hpp"
#include "fmt/format.h"
#include "nlohmann/json.hpp"

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

std::string Traverser::ProcessTraverseNetwork(Interpreter *aInterpreter, const Interpreter::Expression &aExpr)
{
    std::string        value;
    Error              error        = ERROR_NONE;
    CommissionerAppPtr commissioner = nullptr;
    std::string        jsonFile     = "";

    Console::Write("ProcessTraverseNetwork called with " + std::to_string(aExpr.size()) + " args\n",
                   Console::Color::kWhite);
    for (size_t i = 1; i < aExpr.size(); ++i)
    {
        if (aExpr[i] == "--json")
        {
            VerifyOrExit(i + 1 < aExpr.size(), error = ERROR_INVALID_ARGS("Missing JSON filename"));
            jsonFile = aExpr[++i];
            Console::Write("JSON output enabled: " + jsonFile, Console::Color::kWhite);
        }
        else if (aExpr[i][0] == '-')
        {
            ExitNow(error = ERROR_INVALID_ARGS("Invalid argument: {}", aExpr[i]));
        }
    }

    for (size_t i = 0; i < aInterpreter->mContext.mCommandKeys.size(); ++i)
    {
        if (aInterpreter->mContext.mCommandKeys[i] == "--json")
        {
            VerifyOrExit(i + 1 < aInterpreter->mContext.mCommandKeys.size(),
                         error = ERROR_INVALID_ARGS("Missing JSON filename"));
            jsonFile = aInterpreter->mContext.mCommandKeys[++i];
            Console::Write("JSON output enabled: " + jsonFile, Console::Color::kWhite);
        }
    }

    SuccessOrExit(error = aInterpreter->mJobManager->GetSelectedCommissioner(commissioner));
    value = ProcessTraverseNetworkJob(aInterpreter, commissioner, aExpr, jsonFile);

exit:
    if (error != ErrorCode::kNone)
    {
        return error.ToString();
    }
    return value;
}

std::string Traverser::ProcessTraverseNetworkJob(Interpreter        *aInterpreter,
                                                 CommissionerAppPtr &aCommissioner,
                                                 const Interpreter::Expression &,
                                                 const std::string        &aJsonFile,
                                                 std::chrono::milliseconds aTimeout)
{
    std::string                        value;
    std::stringstream                  resultStream;
    std::string                        leaderAloc;
    size_t                             expectedChildCount  = 0;
    size_t                             expectedRouterCount = 0;
    bool                               route64Found        = false;
    std::set<uint16_t>                 expectedRouters;
    std::map<std::string, NetDiagData> collectedData;
    std::map<std::string, NetDiagData> routers;
    std::map<std::string, NetDiagData> children;
    Error                              error;
    // Call the new async-based but blocking API
    Console::Write("Starting Network Traversal...", Console::Color::kCyan);

    // Callback handler
    class CliTraverseHandler : public Commissioner::TraverseHandler
    {
    public:
        void OnTotalRoutersCount(size_t aCount) override
        {
            totalRouters = aCount;
            Console::Write(fmt::format("{} Routers found", aCount), Console::Color::kWhite);

            if (leaderResponded)
            {
                char           symbol = 'R';
                Console::Color color  = Console::Color::kRed;
                if (leaderStatus == Commissioner::TraverseStatus::kSuccess)
                    color = Console::Color::kGreen;
                else if (leaderStatus == Commissioner::TraverseStatus::kSuccessWithRetry)
                    color = Console::Color::kYellow;

                Console::WriteNoNewline(std::string(1, symbol), color);
                routersFound++;

                if (routersFound % 5 == 0)
                    Console::WriteNoNewline(" ", Console::Color::kWhite);
            }
        }

        void OnTotalChildrenCount(size_t aCount) override
        {
            Console::Write("\n", Console::Color::kWhite); // End Router line
            totalChildren = aCount;
            Console::Write(fmt::format("{} Children found", aCount), Console::Color::kWhite);
        }

        void OnDeviceResponded(const std::string &aAddr, const NetDiagData *aData, Commissioner::TraverseStatus aStatus) override
        {
            bool isRouterPhase = (totalRouters == 0) || (routersFound < totalRouters);

            if (totalRouters == 0)
            {
                // Leader
                leaderStatus    = aStatus;
                leaderResponded = true;
                if (aData)
                    collectedData[aAddr] = *aData;
                return;
            }

            // Print symbols for fetched devices
            char           symbol = isRouterPhase ? 'R' : 'C';
            Console::Color color  = Console::Color::kRed;
            if (aStatus == Commissioner::TraverseStatus::kSuccess)
                color = Console::Color::kGreen;
            else if (aStatus == Commissioner::TraverseStatus::kSuccessWithRetry)
                color = Console::Color::kYellow;

            Console::WriteNoNewline(std::string(1, symbol), color);

            size_t &counter = isRouterPhase ? routersFound : childrenFound;
            counter++;

            // Create chunks of 5 printed symbols for easy visualization
            if (counter % 5 == 0)
                Console::WriteNoNewline(" ", Console::Color::kWhite);

            if (aData && aStatus != Commissioner::TraverseStatus::kFailed)
            {
                collectedData[aAddr] = *aData;
            }
        }

        void OnFinished(const std::map<std::string, NetDiagData> *aReport, Error aError) override
        {
            if (aError != ErrorCode::kNone)
            {
                value = aError.ToString();
            }
            else if (aReport != nullptr)
            {
                collectedData = *aReport;
            }
            isFinished = true;
        }

        size_t                             totalRouters    = 0;
        size_t                             totalChildren   = 0;
        size_t                             routersFound    = 0;
        size_t                             childrenFound   = 0;
        Commissioner::TraverseStatus       leaderStatus    = Commissioner::TraverseStatus::kFailed;
        bool                               leaderResponded = false;
        std::map<std::string, NetDiagData> collectedData;
        std::atomic<bool>                  isFinished{false};
        std::string                        value;
    };

    CliTraverseHandler handler;

    // Use a long enough timeout to allow large network traversal but prevent infinite hangs
    auto startTime = std::chrono::steady_clock::now();

    aCommissioner->TraverseNetwork(handler);

    while (!handler.isFinished)
    {
        if (aInterpreter->IsCancelled())
        {
            aCommissioner->CancelRequests();
        }

        if (std::chrono::steady_clock::now() - startTime > aTimeout)
        {
            Console::Write("\nGlobal traversal timeout reached! Cancelling...\n", Console::Color::kRed);
            aCommissioner->CancelRequests();

            // Give a small grace period for cancellation to process callbacks
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if (!handler.isFinished)
            {
                value      = "Traversal timed out";
                handler.isFinished = true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (handler.value != "")
    {
        value = handler.value;
        goto exit;
    }

    collectedData = handler.collectedData;

    for (const auto &pair : collectedData)
    {
        uint16_t rloc16 = pair.second.mMacAddr;

        bool isRouter = false;
        // Check router ID from RLOC16
        uint8_t routerId = (rloc16 >> 10);
        if (routerId < 64 && (rloc16 & 0x03FF) == 0)
        {
            isRouter = true;
        }

        if (isRouter)
        {
            routers[pair.first] = pair.second;
        }
        else
        {
            children[pair.first] = pair.second;
        }
    }

    // Calculate expected counts
    for (const auto &pair : collectedData)
    {
        const auto &data = pair.second;
        if (!route64Found && (data.mPresentFlags & NetDiagData::kRoute64Bit))
        {
            // Count set bits in Route64 mask to estimate active routers
            for (uint8_t byte : data.mRoute64.mMask)
            {
                // Brian Kernighan's algorithm to count set bits
                uint8_t n = byte;
                while (n > 0)
                {
                    n &= (n - 1);
                    expectedRouterCount++;
                }
            }
            route64Found = true;
        }

        if (data.mPresentFlags & NetDiagData::kChildTableBit)
        {
            expectedChildCount += data.mChildTable.size();
        }
    }

    if (!route64Found)
    {
        expectedRouterCount = routers.size();
    }

    Console::Write("", Console::Color::kWhite);

    Console::Write("\n--- Traversal Summary ---", Console::Color::kGreen);
    Console::Write(fmt::format("Routers:  {} / {} (Expected)", routers.size(), expectedRouterCount),
                   Console::Color::kWhite);
    Console::Write(fmt::format("Children: {} / {} (Expected)", children.size(), expectedChildCount),
                   Console::Color::kWhite);

    PrintNetworkData(collectedData);
    value = "";

    if (!aJsonFile.empty())
    {
        Console::Write("Writing JSON to " + aJsonFile + " with " + std::to_string(collectedData.size()) + " entries\n",
                       Console::Color::kWhite);

        nlohmann::json report;

        // Try to find Network Data first to put it at top level (before devices)
        for (const auto &pair : collectedData)
        {
            if (pair.second.mPresentFlags & NetDiagData::kNetworkDataBit)
            {
                std::string jsonStr = NetDiagDataToJson(pair.second);
                try
                {
                    auto jsonObj = nlohmann::json::parse(jsonStr);
                    if (jsonObj.contains("NetworkData"))
                    {
                        report["NetworkData"] = jsonObj["NetworkData"];
                        break; // Only need one
                    }
                } catch (const std::exception &)
                {
                    // Ignore parsing error here, will be caught in loop below
                }
            }
        }

        nlohmann::json &devices = report["devices"];

        for (const auto &pair : collectedData)
        {
            const auto &addr = pair.first;
            const auto &data = pair.second;
            std::string key  = addr;

            if (data.mPresentFlags & NetDiagData::kEui64Bit)
            {
                key = utils::Hex(data.mEui64);
            }
            else if (data.mPresentFlags & NetDiagData::kExtMacAddrBit)
            {
                key = utils::Hex(data.mExtMacAddr);
            }

            std::string jsonStr = NetDiagDataToJson(data);
            try
            {
                auto deviceObj = nlohmann::json::parse(jsonStr);
                devices[key] = deviceObj;
            } catch (const std::exception &e)
            {
                Console::Write(fmt::format("Failed to parse JSON for {}: {}\n", key, e.what()), Console::Color::kRed);
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

void Traverser::PrintNetworkData(const std::map<std::string, NetDiagData> &aCapturedData)
{
    Console::Write("--- Net Data ---", Console::Color::kGreen);

    const NetDiagData *sourceData = nullptr;

    // Find the best source for Network Data (prefer Leader/Router with Route64)
    for (const auto &pair : aCapturedData)
    {
        const NetDiagData &d = pair.second;
        if (d.mPresentFlags & NetDiagData::kNetworkDataBit)
        {
            if (!sourceData || (d.mPresentFlags & NetDiagData::kRoute64Bit))
            {
                sourceData = &d;
                if (d.mPresentFlags & NetDiagData::kRoute64Bit)
                {
                    break;
                }
            }
        }
    }

    if (!sourceData)
    {
        Console::Write("No Network Data found.\n", Console::Color::kWhite);
        return;
    }

    const NetworkData &netData = sourceData->mNetworkData;

    for (const auto &prefixEntry : netData.mPrefixList)
    {
        std::string prefixStr = Ipv6PrefixToString(prefixEntry.mPrefix);
        Console::WriteNoNewline(fmt::format("Prefix {}", prefixStr), Console::Color::kWhite);
        if (prefixEntry.mIsStable)
        {
            Console::Write(" [Stable]", Console::Color::kGreen);
        }
        else
        {
            Console::Write("\n", Console::Color::kWhite);
        }

        // Has Route TLV
        for (const auto &hasRoute : prefixEntry.mHasRouteList)
        {
            std::string prfStr;
            switch (hasRoute.mRouterPreference)
            {
            case 1:
                prfStr = "High";
                break;
            case 0:
                prfStr = "Mid";
                break;
            case 3:
                prfStr = "Low";
                break;
            default:
                prfStr = "Rsrv";
                break;
            }

            Console::WriteNoNewline(fmt::format("  Has Route: RLOC 0x{:04X}, PRF {}", hasRoute.mRloc16, prfStr),
                                    Console::Color::kWhite);

            if (hasRoute.mIsNat64)
            {
                Console::WriteNoNewline(" [NAT64]", Console::Color::kGreen);
            }
            if (hasRoute.mIsStable)
            {
                Console::WriteNoNewline(" [Stable]", Console::Color::kGreen);
            }
            Console::Write("", Console::Color::kWhite); // Newline
        }

        // Border Router TLV
        for (const auto &br : prefixEntry.mBorderRouterList)
        {
            std::string prfStr;
            switch (br.mPrefixPreference)
            {
            case 1:
                prfStr = "High";
                break;
            case 0:
                prfStr = "Mid";
                break;
            case 3:
                prfStr = "Low";
                break;
            default:
                prfStr = "Rsrv";
                break;
            }

            Console::WriteNoNewline(fmt::format("  Border Router: RLOC 0x{:04X}, PRF {}", br.mRloc16, prfStr),
                                    Console::Color::kWhite);

            auto printFlag = [](bool val, const char *name) {
                if (val)
                {
                    Console::WriteNoNewline(fmt::format(" [{}]", name), Console::Color::kGreen);
                }
            };

            printFlag(br.mIsOnMesh, "On-mesh");
            printFlag(br.mIsDefaultRoute, "Default");
            printFlag(br.mIsSlaac, "SLAAC");
            printFlag(br.mIsDhcp, "DHCP");
            printFlag(br.mIsConfigure, "Configure");
            printFlag(br.mIsPreferred, "Preferred");
            printFlag(br.mIsNdDns, "ND DNS");
            printFlag(br.mIsDp, "DP");
            printFlag(br.mIsStable, "Stable");

            Console::Write("", Console::Color::kWhite); // Newline
        }

        // 6LoWPAN TLV
        const auto &ctx = prefixEntry.mSixLowPanContext;
        if (ctx.mIsCompress)
        {
            Console::Write(fmt::format("  6LoWPAN: Compress, CID {}, Len {}", ctx.mContextId, ctx.mContextLength),
                           Console::Color::kWhite);
        }
    }

    // Service TLVs
    for (const auto &service : netData.mServiceList)
    {
        Console::WriteNoNewline(fmt::format("  Service: ID 0x{:02X}, Ent 0x{:X}, Data {}", service.mId,
                                            service.mEnterpriseNumber, utils::Hex(service.mServiceData)),
                                Console::Color::kWhite);

        if (service.mIsThread)
        {
            Console::WriteNoNewline(" [Thread]", Console::Color::kGreen);
        }
        if (service.mIsStable)
        {
            Console::WriteNoNewline(" [Stable]", Console::Color::kGreen);
        }
        Console::Write("", Console::Color::kWhite);

        for (const auto &server : service.mServerList)
        {
            Console::Write(
                fmt::format("    Server: RLOC 0x{:04X}, Data {}", server.mRloc16, utils::Hex(server.mServerData)),
                Console::Color::kWhite);
        }
    }
    Console::Write("\n", Console::Color::kWhite);
}

} // namespace commissioner

} // namespace ot
