/*
 *    Copyright (c) 2021, The OpenThread Commissioner Authors.
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
 * @file Mock object for CommissionerApp consumers testing.
 */

#ifndef OT_COMM_APP_COMMISSIONER_APP_MOCK_HPP_
#define OT_COMM_APP_COMMISSIONER_APP_MOCK_HPP_

#include "gmock/gmock.h"

#include "app/commissioner_app.hpp"

using namespace ::ot::commissioner;

class CommissionerAppMock : public ::ot::commissioner::CommissionerApp
{
    using Error = ::ot::commissioner::Error;

public:
    virtual ~CommissionerAppMock() = default;
    MOCK_METHOD(std::string, OnJoinerRequest, (const ByteArray &), (override));
    MOCK_METHOD(void, OnJoinerConnected, (const ByteArray &, Error), (override));
    MOCK_METHOD(bool,
                OnJoinerFinalize,
                (const ByteArray &,
                 const std::string &,
                 const std::string &,
                 const std::string &,
                 const ByteArray &,
                 const std::string &,
                 const ByteArray &),
                (override));
    MOCK_METHOD(void, OnKeepAliveResponse, (Error), (override));
    MOCK_METHOD(void, OnPanIdConflict, (const std::string &, const ChannelMask &, uint16_t), (override));
    MOCK_METHOD(void, OnEnergyReport, (const std::string &, const ChannelMask &, const ByteArray &), (override));
    MOCK_METHOD(void, OnDatasetChanged, (), (override));

    MOCK_METHOD(Error, Start, (std::string &, const std::string &, uint16_t));
    MOCK_METHOD(void, Stop, ());
    MOCK_METHOD(void, CancelRequests, ());
    MOCK_METHOD(bool, IsActive, (), (const));
    MOCK_METHOD(Error, SaveNetworkData, (const std::string &));
    MOCK_METHOD(Error, SyncNetworkData, ());

    MOCK_METHOD(Error, GetSessionId, (uint16_t &), (const));
    MOCK_METHOD(Error, GetBorderAgentLocator, (uint16_t &), (const));
    MOCK_METHOD(Error, GetSteeringData, (ByteArray &, JoinerType), (const));
    MOCK_METHOD(Error, EnableJoiner, (JoinerType, uint64_t, const std::string &, const std::string &));
    MOCK_METHOD(Error, DisableJoiner, (JoinerType, uint64_t));
    MOCK_METHOD(Error, EnableAllJoiners, (JoinerType, const std::string &, const std::string &));
    MOCK_METHOD(Error, DisableAllJoiners, (JoinerType));
    MOCK_METHOD(Error, GetJoinerUdpPort, (uint16_t &, JoinerType), (const));
    MOCK_METHOD(Error, SetJoinerUdpPort, (JoinerType, uint16_t));
    MOCK_METHOD(Error, GetCommissionerDataset, (CommissionerDataset &, uint16_t));
    MOCK_METHOD(Error, SetCommissionerDataset, (const CommissionerDataset &));
    MOCK_METHOD(Error, GetActiveTimestamp, (Timestamp &), (const));
    MOCK_METHOD(Error, GetChannel, (Channel &));
    MOCK_METHOD(Error, SetChannel, (const Channel &, MilliSeconds));
    MOCK_METHOD(Error, GetChannelMask, (ChannelMask &), (const));
    MOCK_METHOD(Error, SetChannelMask, (const ChannelMask &));
    MOCK_METHOD(Error, GetExtendedPanId, (ByteArray &), (const));
    MOCK_METHOD(Error, SetExtendedPanId, (const ByteArray &));
    MOCK_METHOD(Error, GetMeshLocalPrefix, (std::string &));
    MOCK_METHOD(Error, SetMeshLocalPrefix, (const std::string &, MilliSeconds));
    MOCK_METHOD(Error, GetMeshLocalAddr, (std::string &, const std::string &, uint16_t));
    MOCK_METHOD(Error, GetNetworkMasterKey, (ByteArray &));
    MOCK_METHOD(Error, SetNetworkMasterKey, (const ByteArray &, MilliSeconds));
    MOCK_METHOD(Error, GetNetworkName, (std::string &), (const));
    MOCK_METHOD(Error, SetNetworkName, (const std::string &));
    MOCK_METHOD(Error, GetPanId, (PanId &));
    MOCK_METHOD(Error, SetPanId, (PanId, MilliSeconds));
    MOCK_METHOD(Error, GetPSKc, (ByteArray &), (const));
    MOCK_METHOD(Error, SetPSKc, (const ByteArray &));
    MOCK_METHOD(Error, GetSecurityPolicy, (SecurityPolicy &), (const));
    MOCK_METHOD(Error, SetSecurityPolicy, (const SecurityPolicy &));
    MOCK_METHOD(Error, GetActiveDataset, (ActiveOperationalDataset &, uint16_t));
    MOCK_METHOD(Error, SetActiveDataset, (const ActiveOperationalDataset &));
    MOCK_METHOD(Error, GetPendingDataset, (PendingOperationalDataset &, uint16_t));
    MOCK_METHOD(Error, SetPendingDataset, (const PendingOperationalDataset &));
    MOCK_METHOD(Error, GetTriHostname, (std::string &), (const));
    MOCK_METHOD(Error, SetTriHostname, (const std::string &));
    MOCK_METHOD(Error, GetRegistrarHostname, (std::string &), (const));
    MOCK_METHOD(Error, SetRegistrarHostname, (const std::string &));
    MOCK_METHOD(Error, GetRegistrarIpv6Addr, (std::string &), (const));
    MOCK_METHOD(Error, GetBbrDataset, (BbrDataset &, uint16_t));
    MOCK_METHOD(Error, SetBbrDataset, (const BbrDataset &));
    MOCK_METHOD(Error, Reenroll, (const std::string &));
    MOCK_METHOD(Error, DomainReset, (const std::string &));
    MOCK_METHOD(Error, Migrate, (const std::string &, const std::string &));
    MOCK_METHOD(const ByteArray &, GetToken, (), (const));
    MOCK_METHOD(Error, RequestToken, (const std::string &, uint16_t));
    MOCK_METHOD(Error, SetToken, (const ByteArray &));
    MOCK_METHOD(Error, RegisterMulticastListener, (const std::vector<std::string> &, Seconds));
    MOCK_METHOD(Error, AnnounceBegin, (uint32_t, uint8_t, MilliSeconds, const std::string &));
    MOCK_METHOD(Error, PanIdQuery, (uint32_t, uint16_t, const std::string &));
    MOCK_METHOD(bool, HasPanIdConflict, (uint16_t), (const));
    MOCK_METHOD(Error, EnergyScan, (uint32_t, uint8_t, uint16_t, uint16_t, const std::string &));
    MOCK_METHOD(const EnergyReport *, GetEnergyReport, (const Address &), (const));
    MOCK_METHOD(const EnergyReportMap &, GetAllEnergyReports, (), (const));
};

class CommissionerAppStaticExpecter
{
public:
    MOCK_METHOD(Error, Create, (std::shared_ptr<CommissionerApp> & aCommApp, const Config &aConfig));
    virtual ~CommissionerAppStaticExpecter() = default;
};

void SetCommissionerAppStaticExpecter(CommissionerAppStaticExpecter *ptr);
void ClearCommissionerAppStaticExpecter();

#endif // OT_COMM_APP_COMMISSIONER_APP_MOCK_HPP_
