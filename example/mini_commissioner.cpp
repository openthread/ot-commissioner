/*
 *  Copyright (c) 2021, The OpenThread Commissioner Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   The file implements the minimum Thread Commissioner example app.
 */

#include <iomanip>
#include <sstream>

#include <assert.h>
#include <signal.h>
#include <stdio.h>

#include <unistd.h>

#include <commissioner/commissioner.hpp>

using namespace ot::commissioner;

static std::string ToHexString(const ByteArray &aBytes)
{
    std::string str;

    for (uint8_t byte : aBytes)
    {
        char buf[3];

        sprintf(buf, "%02x", byte);
        str.append(buf);
    }

    return str;
}

static ByteArray FromHexString(const std::string &aHex)
{
    assert(aHex.size() % 2 == 0);

    ByteArray bytes;

    for (size_t i = 0; i < aHex.size(); i += 2)
    {
        assert(isxdigit(aHex[i]));
        assert(isxdigit(aHex[i + 1]));

        uint8_t x = isdigit(aHex[i]) ? (aHex[i] - '0') : (tolower(aHex[i]) - 'a' + 10);
        uint8_t y = isdigit(aHex[i + 1]) ? (aHex[i + 1] - '0') : (tolower(aHex[i + 1]) - 'a' + 10);

        bytes.push_back((x << 4) | y);
    }
    return bytes;
}

class MyCommissionerHandler : public CommissionerHandler
{
public:
    MyCommissionerHandler(const std::string &aPskd)
        : mPskd(aPskd)
    {
    }

    std::string OnJoinerRequest(const ByteArray &aJoinerId) override
    {
        auto joinerId = ToHexString(aJoinerId);

        printf("\n");
        printf("joiner \"%s\" is requesting join the Thread network\n", joinerId.c_str());

        return mPskd;
    }

    void OnJoinerConnected(const ByteArray &aJoinerId, Error aError) override
    {
        auto joinerId = ToHexString(aJoinerId);

        printf("joiner \"%s\" is connected: %s\n", joinerId.c_str(), aError.ToString().c_str());
    }

    bool OnJoinerFinalize(const ByteArray &  aJoinerId,
                          const std::string &aVendorName,
                          const std::string &aVendorModel,
                          const std::string &aVendorSwVersion,
                          const ByteArray &  aVendorStackVersion,
                          const std::string &aProvisioningUrl,
                          const ByteArray &  aVendorData) override
    {
        printf("joiner \"%s\" is commissioned\n", ToHexString(aJoinerId).c_str());
        printf("[Vendor Name]          : %s\n", aVendorName.c_str());
        printf("[Vendor Model]         : %s\n", aVendorModel.c_str());
        printf("[Vendor SW Version]    : %s\n", aVendorSwVersion.c_str());
        printf("[Vendor Stack Version] : %s\n", ToHexString(aVendorStackVersion).c_str());
        printf("[Provisioning URL]     : %s\n", aProvisioningUrl.c_str());
        printf("[Vendor Data]          : %s\n", ToHexString(aVendorData).c_str());

        return true;
    }

private:
    std::string mPskd;
};

std::shared_ptr<Commissioner> commissioner;

void SignalHandler(int signal)
{
    if (commissioner != nullptr)
    {
        printf("\nResigning the commissioner\n");
        commissioner->Resign().IgnoreError();
    }

    exit(0);
}

int main(int argc, const char *argv[])
{
    if (argc != 5)
    {
        printf("usage:\n");
        printf("    %s <br-addr> <br-port> <pskc-hex> <pskd>\n", argv[0]);
        return -1;
    }

    std::string brAddr = argv[1];
    uint16_t    brPort = std::stoul(argv[2]);
    ByteArray   pskc   = FromHexString(argv[3]);
    std::string pskd   = argv[4];

    printf("===================================================\n");
    printf("[Border Router address] : %s\n", brAddr.c_str());
    printf("[Border Router port]    : %hu\n", brPort);
    printf("[PSKc]                  : %s\n", ToHexString(pskc).c_str());
    printf("[PSKd]                  : %s\n", pskd.c_str());
    printf("===================================================\n\n");

    MyCommissionerHandler myHandler{pskd};
    commissioner = Commissioner::Create(myHandler);

    signal(SIGINT, SignalHandler);
    printf("===================================================\n");
    printf("type CRTL + C to quit!\n");
    printf("===================================================\n\n");

    Config config;
    config.mEnableCcm = false;
    config.mPSKc      = pskc;

    Error error;

    if ((error = commissioner->Init(config)) != ErrorCode::kNone)
    {
        printf("failed to initialize the commissioner: %s\n", error.ToString().c_str());
        return -1;
    }

    std::string existingCommissionerId;

    printf("petitioning to [%s]:%hu\n", brAddr.c_str(), brPort);
    error = commissioner->Petition(existingCommissionerId, brAddr, brPort);
    if (error != ErrorCode::kNone)
    {
        printf("failed to petition to BR at [%s]:%hu: %s\n", brAddr.c_str(), brPort, error.ToString().c_str());
        return -1;
    }

    // Check if we are active now.
    printf("the commissioner is active: %s\n", commissioner->IsActive() ? "true" : "false");
    assert(commissioner->IsActive());

    CommissionerDataset dataset;

    printf("enabling MeshCoP for all joiners\n");
    dataset.mPresentFlags |= CommissionerDataset::kSteeringDataBit;
    dataset.mSteeringData = {0xFF}; // Set the steeering data to all-ones to allow all joiners.
    error                 = commissioner->SetCommissionerDataset(dataset);
    if (error != ErrorCode::kNone)
    {
        printf("failed to enable MeshCop for all joiners: %s\n", error.ToString().c_str());
        return -1;
    }

    printf("waiting for joiners\n");
    while (true)
    {
        sleep(1);
    }

    commissioner->Resign().IgnoreError();

    return 0;
}
