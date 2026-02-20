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
                                                 const std::string &aJsonFile)
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
    std::atomic<bool>                  isFinished{false};

    // Call the new async-based but blocking API
    Console::Write("Starting Network Traversal...", Console::Color::kCyan);

    // State for live output
    size_t                       totalRouters    = 0;
    size_t                       totalChildren   = 0;
    size_t                       routersFound    = 0;
    size_t                       childrenFound   = 0;
    Commissioner::TraverseStatus leaderStatus    = Commissioner::TraverseStatus::kFailed;
    bool                         leaderResponded = false;

    // Callback handler
    Commissioner::TraverseHandler handler;
    handler.mOnTotalRoutersCount = [&](size_t aCount) {
        totalRouters = aCount;
        Console::Write(fmt::format("{} Routers found", aCount), Console::Color::kWhite);

        // Flush Leader status if we have it
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
    };
    handler.mOnTotalChildrenCount = [&](size_t aCount) {
        Console::Write("\n", Console::Color::kWhite); // End Router line
        totalChildren = aCount;
        Console::Write(fmt::format("{} Children found", aCount), Console::Color::kWhite);
    };
    handler.mOnDeviceResponded = [&](const std::string &aAddr, const NetDiagData *aData,
                                     Commissioner::TraverseStatus aStatus) {
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
    };
    handler.mOnFinished = [&](const std::map<std::string, NetDiagData> *aReport, Error aError) {
        if (aError != ErrorCode::kNone)
        {
            value = aError.ToString();
        }
        else if (aReport != nullptr)
        {
            collectedData = *aReport;
        }
        isFinished = true;

    };

    error = aCommissioner->TraverseNetwork(handler);
    if (error != ErrorCode::kNone)
    {
        value = error.ToString();
        goto exit;
    }

    while (!isFinished)
    {
        if (aInterpreter->IsCancelled())
        {
            aCommissioner->CancelRequests();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (value != "")
    {
        // An error occurred and was set by the handler
        goto exit;
    }

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

    resultStream << "\n--- Traversal Summary ---\n";
    resultStream << "Routers:  " << routers.size() << " / " << expectedRouterCount << " (Expected)\n";
    resultStream << "Children: " << children.size() << " / " << expectedChildCount << " (Expected)\n";
    value = resultStream.str();

    if (!aJsonFile.empty())
    {
        Console::Write("Writing JSON to " + aJsonFile + " with " + std::to_string(collectedData.size()) + " entries\n",
                       Console::Color::kWhite);

        nlohmann::json  report;
        nlohmann::json &devices = report["devices"];

        for (const auto &pair : collectedData)
        {
            const auto &addr = pair.first;
            const auto &data = pair.second;

            std::string jsonStr = NetDiagDataToJson(data);
            try
            {
                devices[addr] = nlohmann::json::parse(jsonStr);
            } catch (const std::exception &e)
            {
                Console::Write(fmt::format("Failed to parse JSON for {}: {}\n", addr, e.what()), Console::Color::kRed);
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
