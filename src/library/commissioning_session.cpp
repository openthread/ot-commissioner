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
 *   This file implements 1.1 mesh Commissioning Session.
 */

#include "library/commissioning_session.hpp"

#include "library/commissioner_impl.hpp"
#include "library/logging.hpp"
#include "library/tlv.hpp"
#include "library/uri.hpp"

namespace ot {

namespace commissioner {

CommissioningSession::CommissioningSession(CommissionerImpl &aCommImpl,
                                           const JoinerInfo &aJoinerInfo,
                                           uint16_t          aJoinerUdpPort,
                                           uint16_t          aJoinerRouterLocator,
                                           const ByteArray & aJoinerIid,
                                           const Address &   aJoinerAddr,
                                           uint16_t          aJoinerPort)
    : mCommImpl(aCommImpl)
    , mJoinerInfo(aJoinerInfo)
    , mJoinerUdpPort(aJoinerUdpPort)
    , mJoinerRouterLocator(aJoinerRouterLocator)
    , mJoinerIid(aJoinerIid)
    , mJoinerSocket(std::make_shared<MockSocket>(aCommImpl.GetEventBase(), aJoinerAddr, aJoinerPort))
    , mSocket(std::make_shared<MockSocket>(aCommImpl.GetEventBase(), Address::FromString("::"), kCommissioningPort))
    , mDtlsSession(std::make_shared<DtlsSession>(aCommImpl.GetEventBase(), /* aIsServer */ true, mSocket))
    , mCoap(aCommImpl.GetEventBase(), *mDtlsSession)
    , mResourceJoinFin(uri::kJoinFin, [this](const coap::Request &aRequest) { HandleJoinFin(aRequest); })
{
    mJoinerSocket->SetEventHandler([this](short aFlags) { HandleJoinerSocketEvent(aFlags); });

    ASSERT(mJoinerSocket->Connect(mSocket) == 0);
    ASSERT(mSocket->Connect(mJoinerSocket) == 0);
    ASSERT(mCoap.AddResource(mResourceJoinFin).NoError());
}

Error CommissioningSession::Start(ConnectHandler aOnConnected)
{
    Error error;

    auto dtlsConfig = GetDtlsConfig(mCommImpl.GetConfig());
    dtlsConfig.mPSK = mJoinerInfo.mPSKd;

    mExpirationTime = Clock::now() + MilliSeconds(kDtlsHandshakeTimeoutMax * 1000 + kCommissioningTimeout * 1000);

    SuccessOrExit(error = mDtlsSession->Init(dtlsConfig));

    {
        auto onConnected = [this, aOnConnected](const DtlsSession &, Error aError) { aOnConnected(*this, aError); };
        mDtlsSession->Connect(onConnected);
    }

exit:
    return error;
}

void CommissioningSession::Stop()
{
    mDtlsSession->Disconnect(ERROR_ABORTED("the joiner commissioning session was aborted"));
}

Error CommissioningSession::RecvJoinerDtlsRecords(const ByteArray &aRecords)
{
    Error error;

    if (!aRecords.empty())
    {
        if (int fail = mJoinerSocket->Send(&aRecords[0], aRecords.size()))
        {
            ExitNow(error = ERROR_IO_ERROR("forwarding Joiner DTLS records failed: {}", fail));
        }
    }

exit:
    return error;
}

void CommissioningSession::HandleJoinerSocketEvent(short aFlags)
{
    if (aFlags & EV_READ)
    {
        ByteArray buf;
        if (int fail = mJoinerSocket->Receive(buf))
        {
            LOG_ERROR("receiving DTLS records to joiner failed: {}", fail);
        }
        else
        {
            Error error = SendRlyTx(buf);
            if (!error.NoError())
            {
                LOG_ERROR("sending RLY_TX.ntf failed: {}", error.ToString());
            }
        }
    }
}

Error CommissioningSession::SendRlyTx(const ByteArray &aBuf)
{
    Error         error;
    coap::Request rlyTx{coap::Type::kNonConfirmable, coap::Code::kPost};

    SuccessOrExit(error = rlyTx.SetUriPath(uri::kRelayTx));

    SuccessOrExit(error = AppendTlv(rlyTx, {tlv::Type::kJoinerUdpPort, GetJoinerUdpPort()}));
    SuccessOrExit(error = AppendTlv(rlyTx, {tlv::Type::kJoinerRouterLocator, GetJoinerRouterLocator()}));
    SuccessOrExit(error = AppendTlv(rlyTx, {tlv::Type::kJoinerIID, GetJoinerIid()}));
    SuccessOrExit(error = AppendTlv(rlyTx, {tlv::Type::kJoinerDtlsEncapsulation, aBuf}));

    if (!mDtlsSession->GetKek().empty())
    {
        SuccessOrExit(error = AppendTlv(rlyTx, {tlv::Type::kJoinerRouterKEK, mDtlsSession->GetKek()}));
    }

    mCommImpl.mBrClient.SendRequest(rlyTx, nullptr);

    LOG_DEBUG("sent RLY_TX.ntf: joinerIID={}, dtlsMessagelength={}", utils::Hex(GetJoinerIid()), aBuf.size());

exit:
    return error;
}

void CommissioningSession::HandleJoinFin(const coap::Request &aJoinFin)
{
    bool        accepted = false;
    Error       error;
    tlv::TlvSet tlvSet;
    tlv::TlvPtr stateTlv              = nullptr;
    tlv::TlvPtr vendorNameTlv         = nullptr;
    tlv::TlvPtr vendorModelTlv        = nullptr;
    tlv::TlvPtr vendorSwVersionTlv    = nullptr;
    tlv::TlvPtr vendorStackVersionTlv = nullptr;

    std::string provisioningUrl{};
    ByteArray   vendorData{};

    SuccessOrExit(error = GetTlvSet(tlvSet, aJoinFin));

    VerifyOrExit((stateTlv = tlvSet[tlv::Type::kState]) != nullptr,
                 error = ERROR_BAD_FORMAT("no valid State TLV found"));

    VerifyOrExit((vendorNameTlv = tlvSet[tlv::Type::kVendorName]) != nullptr,
                 error = ERROR_BAD_FORMAT("no valid Vendor Name TLV found"));

    VerifyOrExit((vendorModelTlv = tlvSet[tlv::Type::kVendorModel]) != nullptr,
                 error = ERROR_BAD_FORMAT("no valid Vendor Model TLV found"));

    VerifyOrExit((vendorSwVersionTlv = tlvSet[tlv::Type::kVendorSWVersion]) != nullptr,
                 error = ERROR_BAD_FORMAT("no valid Vendor SW Version TLV found"));

    VerifyOrExit((vendorStackVersionTlv = tlvSet[tlv::Type::kVendorStackVersion]) != nullptr,
                 error = ERROR_BAD_FORMAT("no valid Vendor Stack Version TLV found"));

    if (auto provisioningUrlTlv = tlvSet[tlv::Type::kProvisioningURL])
    {
        auto vendorDataTlv = tlvSet[tlv::Type::kVendorData];
        VerifyOrExit(vendorDataTlv != nullptr, error = ERROR_BAD_FORMAT("no valid Vendor Data TLV found"));

        provisioningUrl = provisioningUrlTlv->GetValueAsString();
        vendorData      = vendorDataTlv->GetValue();
    }

    LOG_INFO("received JOIN_FIN.req: vendorName={}, vendorModel={}, vendorSWVversion={}, "
             "vendorStackVersion={}, provisioningUrl={}, vendorData={}",
             vendorNameTlv->GetValueAsString(), vendorModelTlv->GetValueAsString(),
             vendorSwVersionTlv->GetValueAsString(), utils::Hex(vendorStackVersionTlv->GetValue()), provisioningUrl,
             utils::Hex(vendorData));

    // Validation done, request commissioning by user.
    if (mCommImpl.mCommissioningHandler != nullptr)
    {
        accepted = mCommImpl.mCommissioningHandler(
            mJoinerInfo, vendorNameTlv->GetValueAsString(), vendorModelTlv->GetValueAsString(),
            vendorSwVersionTlv->GetValueAsString(), vendorStackVersionTlv->GetValue(), provisioningUrl, vendorData);
    }
    else
    {
        // Accepts a joiner if requirement on vendor-specific provisioning.
        accepted = provisioningUrl.empty();
    }
    VerifyOrExit(accepted, error = Error::kReject);

exit:
    if (!error.NoError())
    {
        LOG_WARN("handle JOIN_FIN.req failed: {}", error.ToString());
    }

    IgnoreError(SendJoinFinResponse(aJoinFin, accepted));
    LOG_INFO("sent JOIN_FIN.rsp: accepted={}", accepted);
}

Error CommissioningSession::SendJoinFinResponse(const coap::Request &aJoinFinReq, bool aAccept)
{
    Error          error;
    coap::Response joinFin{coap::Type::kAcknowledgment, coap::Code::kChanged};
    SuccessOrExit(error = AppendTlv(joinFin, {tlv::Type::kState, aAccept ? tlv::kStateAccept : tlv::kStateReject}));

    SuccessOrExit(error = mCoap.SendResponse(aJoinFinReq, joinFin));

exit:
    return error;
}

} // namespace commissioner

} // namespace ot
