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
 *   This file defines the json parser of Network Data and Commissioner configuration.
 *
 */

#ifndef OT_COMM_APP_JSON_HPP_
#define OT_COMM_APP_JSON_HPP_

#include <string>

#include <commissioner/commissioner.hpp>
#include <commissioner/error.hpp>
#include <commissioner/network_data.hpp>
#include <commissioner/network_diag_data.hpp>

#include "app/border_agent.hpp"
#include "app/commissioner_app.hpp"

#include <nlohmann/json.hpp>

#define JSON_INDENT_DEFAULT 4

namespace ot {

namespace commissioner {

struct JsonNetworkData
{
    ActiveOperationalDataset  mActiveDataset;
    PendingOperationalDataset mPendingDataset;
    CommissionerDataset       mCommDataset;
    BbrDataset                mBbrDataset;
};

Error       NetworkDataFromJson(JsonNetworkData &aJsonNetworkData, const std::string &aJson);
std::string NetworkDataToJson(const JsonNetworkData &aJsonNetworkData);

Error       CommissionerDatasetFromJson(CommissionerDataset &aDataset, const std::string &aJson);
std::string CommissionerDatasetToJson(const CommissionerDataset &aDataset);

Error       BbrDatasetFromJson(BbrDataset &aDataset, const std::string &aJson);
std::string BbrDatasetToJson(const BbrDataset &aDataset);

Error       ActiveDatasetFromJson(ActiveOperationalDataset &aDataset, const std::string &aJson);
std::string ActiveDatasetToJson(const ActiveOperationalDataset &aDataset);

Error       PendingDatasetFromJson(PendingOperationalDataset &aDataset, const std::string &aJson);
std::string PendingDatasetToJson(const PendingOperationalDataset &aDataset);

Error ConfigFromJson(Config &aConfig, const std::string &aJson);

std::string EnergyReportToJson(const EnergyReport &aEnergyReport);

std::string EnergyReportMapToJson(const EnergyReportMap &aEnergyReportMap);

std::string NetDiagDataToJson(const NetDiagData &aNetDiagData);
std::string MacCountersToJson(const MacCounters &aMacCounters);

void BorderAgentFromJson(BorderAgent &aAgent, const nlohmann::json &aJson);
void BorderAgentToJson(const BorderAgent &aAgent, nlohmann::json &aJson);

/**
 * Get clean JSON string from a supposedly JSON file.
 *
 * If file includes comments, those are stripped off.
 * If JSON syntax is not valid, ErrorCode::kBadFormat is returned.
 */
Error JsonFromFile(std::string &aJson, const std::string &aPath);

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_APP_JSON_HPP_
