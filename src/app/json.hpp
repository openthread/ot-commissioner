/*
 *    Copyright (c) 2019, The OpenThread Authors.
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
 *   The file defines the config json parser;
 */

#ifndef JSON_HPP_
#define JSON_HPP_

#include <string>

#include <commissioner/commissioner.hpp>
#include <commissioner/error.hpp>
#include <commissioner/network_data.hpp>

#include "app_config.hpp"
#include "commissioner_app.hpp"

namespace ot {

namespace commissioner {

struct NetworkData
{
    ActiveOperationalDataset  mActiveDataset;
    PendingOperationalDataset mPendingDataset;
    CommissionerDataset       mCommDataset;
    BbrDataset                mBbrDataset;
};

Error       NetworkDataFromJson(NetworkData &aNetworkData, const std::string &aJson);
std::string NetworkDataToJson(const NetworkData &aNetworkData);

Error       CommissionerDatasetFromJson(CommissionerDataset &aDataset, const std::string &aJson);
std::string CommissionerDatasetToJson(const CommissionerDataset &aDataset);

Error       BbrDatasetFromJson(BbrDataset &aDataset, const std::string &aJson);
std::string BbrDatasetToJson(const BbrDataset &aDataset);

Error       ActiveDatasetFromJson(ActiveOperationalDataset &aDataset, const std::string &aJson);
std::string ActiveDatasetToJson(const ActiveOperationalDataset &aDataset);

Error       PendingDatasetFromJson(PendingOperationalDataset &aDataset, const std::string &aJson);
std::string PendingDatasetToJson(const PendingOperationalDataset &aDataset);

Error       AppConfigFromJson(AppConfig &aAppConfig, const std::string &aJson);
std::string AppConfigToJson(const AppConfig &aAppConfig);

std::string EnergyReportToJson(const EnergyReport &aEnergyReport);

std::string EnergyReportMapToJson(const EnergyReportMap &aEnergyReportMap);

} // namespace commissioner

} // namespace ot

#endif // JSON_HPP_
