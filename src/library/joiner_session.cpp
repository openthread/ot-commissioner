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
 *   This file implements 1.1 mesh joiner session.
 */

#include "library/joiner_session.hpp"

#include "library/commissioner_impl.hpp"
#include "library/logging.hpp"
#include "library/tlv.hpp"
#include "library/uri.hpp"

namespace ot {

namespace commissioner {

JoinerSession::JoinerSession(CommissionerImpl & aCommImpl,
                             const ByteArray &  aJoinerId,
                             const std::string &aJoinerPSkd,
                             uint16_t           aJoinerUdpPort,
                             uint16_t           aJoinerRouterLocator,
                             const Address &    aJoinerAddr,
                             uint16_t           aJoinerPort,
                             const Address &    aLocalAddr,
                             uint16_t           aLocalPort)
    : mCommImpl(aCommImpl)
    , mJoinerId(aJoinerId)
    , mJoinerPSKd(aJoinerPSkd)
    , mJoinerUdpPort(aJoinerUdpPort)
    , mJoinerRouterLocator(aJoinerRouterLocator)
    , mRelaySocket(std::make_shared<RelaySocket>(*this, aJoinerAddr, aJoinerPort, aLocalAddr, aLocalPort))
    , mDtlsSession(std::make_shared<DtlsSession>(aCommImpl.GetEventBase(), /* aIsServer */ true, mRelaySocket))
    , mCoap(aCommImpl.GetEventBase(), *mDtlsSession)
    , mResourceJoinFin(uri::kJoinFin, [this](const coap::Request &aRequest) { HandleJoinFin(aRequest); })
{
    SuccessOrDie(mCoap.AddResource(mResourceJoinFin));
}

void JoinerSession::Connect()
{
    Error error = Error::kNone;

    auto dtlsConfig = GetDtlsConfig(mCommImpl.GetConfig());
    dtlsConfig.mPSK = {mJoinerPSKd.begin(), mJoinerPSKd.end()};

    mExpirationTime = Clock::now() + MilliSeconds(kDtlsHandshakeTimeoutMax * 1000 + kJoinerTimeout * 1000);

    SuccessOrExit(error = mDtlsSession->Init(dtlsConfig));

    {
        auto onConnected = [this](const DtlsSession &, Error aError) { HandleConnect(aError); };
        mDtlsSession->Connect(onConnected);
    }

exit:
    if (error != Error::kNone)
    {
        HandleConnect(error);
    }
}

void JoinerSession::Disconnect()
{
    mDtlsSession->Disconnect(Error::kAbort);
}

ByteArray JoinerSession::GetJoinerIid() const
{
    auto joinerIid = mJoinerId;
    joinerIid[0] ^= kLocalExternalAddrMask;
    return joinerIid;
}

void JoinerSession::HandleConnect(Error aError)
{
    mCommImpl.mCommissionerHandler.OnJoinerConnected(mJoinerId, aError);
}

void JoinerSession::RecvJoinerDtlsRecords(const ByteArray &aRecords)
{
    mRelaySocket->RecvJoinerDtlsRecords(aRecords);
}

Error JoinerSession::SendRlyTx(const ByteArray &aDtlsMessage, bool aIncludeKek)
{
    Error         error = Error::kNone;
    coap::Request rlyTx{coap::Type::kNonConfirmable, coap::Code::kPost};

    SuccessOrExit(error = rlyTx.SetUriPath(uri::kRelayTx));

    SuccessOrExit(error = AppendTlv(rlyTx, {tlv::Type::kJoinerUdpPort, GetJoinerUdpPort()}));
    SuccessOrExit(error = AppendTlv(rlyTx, {tlv::Type::kJoinerRouterLocator, GetJoinerRouterLocator()}));
    SuccessOrExit(error = AppendTlv(rlyTx, {tlv::Type::kJoinerIID, GetJoinerIid()}));
    SuccessOrExit(error = AppendTlv(rlyTx, {tlv::Type::kJoinerDtlsEncapsulation, aDtlsMessage}));

    if (aIncludeKek)
    {
        VerifyOrExit(!mDtlsSession->GetKek().empty(), error = Error::kInvalidState);
        SuccessOrExit(error = AppendTlv(rlyTx, {tlv::Type::kJoinerRouterKEK, mDtlsSession->GetKek()}));
    }

    mCommImpl.mBrClient.SendRequest(rlyTx, nullptr);

    LOG_DEBUG(LOG_REGION_JOINER_SESSION,
              "session(={}) sent RLY_TX.ntf: SessionState={}, joinerID={}, length={}, includeKek={}",
              static_cast<void *>(this), mDtlsSession->GetStateString(), utils::Hex(GetJoinerId()), aDtlsMessage.size(),
              aIncludeKek);

exit:
    return error;
}

void JoinerSession::HandleJoinFin(const coap::Request &aJoinFin)
{
    bool        accepted = false;
    Error       error    = Error::kNone;
    tlv::TlvSet tlvSet;
    tlv::TlvPtr stateTlv              = nullptr;
    tlv::TlvPtr vendorNameTlv         = nullptr;
    tlv::TlvPtr vendorModelTlv        = nullptr;
    tlv::TlvPtr vendorSwVersionTlv    = nullptr;
    tlv::TlvPtr vendorStackVersionTlv = nullptr;

    std::string provisioningUrl{};
    ByteArray   vendorData{};

    SuccessOrExit(error = GetTlvSet(tlvSet, aJoinFin));

    VerifyOrExit((stateTlv = tlvSet[tlv::Type::kState]) != nullptr, error = Error::kNotFound);
    VerifyOrExit(stateTlv->IsValid(), error = Error::kBadFormat);

    VerifyOrExit((vendorNameTlv = tlvSet[tlv::Type::kVendorName]) != nullptr, error = Error::kNotFound);
    VerifyOrExit(vendorNameTlv->IsValid(), error = Error::kBadFormat);

    VerifyOrExit((vendorModelTlv = tlvSet[tlv::Type::kVendorModel]) != nullptr, error = Error::kNotFound);
    VerifyOrExit(vendorModelTlv->IsValid(), error = Error::kBadFormat);

    VerifyOrExit((vendorSwVersionTlv = tlvSet[tlv::Type::kVendorSWVersion]) != nullptr, error = Error::kNotFound);
    VerifyOrExit(vendorSwVersionTlv->IsValid(), error = Error::kBadFormat);

    VerifyOrExit((vendorStackVersionTlv = tlvSet[tlv::Type::kVendorStackVersion]) != nullptr, error = Error::kNotFound);
    VerifyOrExit(vendorStackVersionTlv->IsValid(), error = Error::kBadFormat);

    if (auto provisioningUrlTlv = tlvSet[tlv::Type::kProvisioningURL])
    {
        auto vendorDataTlv = tlvSet[tlv::Type::kVendorData];
        VerifyOrExit(vendorDataTlv != nullptr, error = Error::kNotFound);
        VerifyOrExit(vendorDataTlv->IsValid(), error = Error::kBadFormat);

        provisioningUrl = provisioningUrlTlv->GetValueAsString();
        vendorData      = vendorDataTlv->GetValue();
    }

    LOG_INFO(LOG_REGION_JOINER_SESSION,
             "session(={}) received JOIN_FIN.req: vendorName={}, vendorModel={}, vendorSWVversion={}, "
             "vendorStackVersion={}, provisioningUrl={}, vendorData={}",
             static_cast<void *>(this), vendorNameTlv->GetValueAsString(), vendorModelTlv->GetValueAsString(),
             vendorSwVersionTlv->GetValueAsString(), utils::Hex(vendorStackVersionTlv->GetValue()), provisioningUrl,
             utils::Hex(vendorData));

    // Validation done, request commissioning by user.
    accepted = mCommImpl.mCommissionerHandler.OnJoinerFinalize(
        mJoinerId, vendorNameTlv->GetValueAsString(), vendorModelTlv->GetValueAsString(),
        vendorSwVersionTlv->GetValueAsString(), vendorStackVersionTlv->GetValue(), provisioningUrl, vendorData);
    VerifyOrExit(accepted, error = Error::kReject);

exit:
    if (error != Error::kNone)
    {
        LOG_WARN(LOG_REGION_JOINER_SESSION, "session(={}) handle JOIN_FIN.req failed: {}", static_cast<void *>(this),
                 ErrorToString(error));
    }

    IgnoreError(SendJoinFinResponse(aJoinFin, accepted));
    LOG_INFO(LOG_REGION_JOINER_SESSION, "session(={}) sent JOIN_FIN.rsp: accepted={}", static_cast<void *>(this),
             accepted);
}

Error JoinerSession::SendJoinFinResponse(const coap::Request &aJoinFinReq, bool aAccept)
{
    Error          error = Error::kNone;
    coap::Response joinFin{coap::Type::kAcknowledgment, coap::Code::kChanged};
    SuccessOrExit(error = AppendTlv(joinFin, {tlv::Type::kState, aAccept ? tlv::kStateAccept : tlv::kStateReject}));

    joinFin.SetSubType(MessageSubType::kJoinFinResponse);
    SuccessOrExit(error = mCoap.SendResponse(aJoinFinReq, joinFin));

exit:
    return error;
}

JoinerSession::RelaySocket::RelaySocket(JoinerSession &aJoinerSession,
                                        const Address &aPeerAddr,
                                        uint16_t       aPeerPort,
                                        const Address &aLocalAddr,
                                        uint16_t       aLocalPort)
    : Socket(aJoinerSession.mCommImpl.GetEventBase())
    , mJoinerSession(aJoinerSession)
    , mPeerAddr(aPeerAddr)
    , mPeerPort(aPeerPort)
    , mLocalAddr(aLocalAddr)
    , mLocalPort(aLocalPort)
{
    int fail;

    mIsConnected = true;

    fail = event_assign(&mEvent, mEventBase, -1, EV_PERSIST | EV_READ | EV_WRITE | EV_ET, HandleEvent, this);
    VerifyOrDie(fail == 0);
    VerifyOrDie((fail = event_add(&mEvent, nullptr)) == 0);
}

JoinerSession::RelaySocket::RelaySocket(RelaySocket &&aOther)
    : Socket(std::move(aOther))
    , mJoinerSession(aOther.mJoinerSession)
    , mPeerAddr(std::move(aOther.mPeerAddr))
    , mPeerPort(std::move(aOther.mPeerPort))
    , mLocalAddr(aOther.mLocalAddr)
    , mLocalPort(aOther.mLocalPort)
{
}

int JoinerSession::RelaySocket::Send(const uint8_t *aBuf, size_t aLen)
{
    Error error;
    bool  includeKek = GetSubType() == MessageSubType::kJoinFinResponse;

    SuccessOrExit(error = mJoinerSession.SendRlyTx({aBuf, aBuf + aLen}, includeKek));

exit:
    if (error != Error::kNone)
    {
        LOG_ERROR(LOG_REGION_JOINER_SESSION, "session(={}) send RLY_TX.ntf failed: {}",
                  static_cast<void *>(&mJoinerSession), ErrorToString(error));
    }
    return error == Error::kNone ? static_cast<int>(aLen) : MBEDTLS_ERR_NET_SEND_FAILED;
}

int JoinerSession::RelaySocket::Receive(uint8_t *aBuf, size_t aMaxLen)
{
    int rval;

    VerifyOrExit(!mRecvBuf.empty(), rval = MBEDTLS_ERR_SSL_WANT_READ);

    rval = static_cast<int>(std::min(aMaxLen, mRecvBuf.size()));
    memcpy(aBuf, mRecvBuf.data(), rval);
    mRecvBuf.erase(mRecvBuf.begin(), mRecvBuf.begin() + rval);

exit:
    return rval;
}

void JoinerSession::RelaySocket::RecvJoinerDtlsRecords(const ByteArray &aRecords)
{
    mRecvBuf.insert(mRecvBuf.end(), aRecords.begin(), aRecords.end());

    // Notifies the DTLS session that there is incoming data.
    event_active(&mEvent, EV_READ, 0);
}

} // namespace commissioner

} // namespace ot
