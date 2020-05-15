/*
 *  Copyright (c) 2019, The OpenThread Commissioner Authors.
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
 *   This file implements UDP proxy.
 */

#include "library/udp_proxy.hpp"

#include "library/commissioner_impl.hpp"
#include "library/logging.hpp"
#include "library/uri.hpp"

namespace ot {

namespace commissioner {

/**
 * Encapsulate the request and send it as a UDP_TX.ntf message.
 */
Error ProxyEndpoint::Send(const ByteArray &aRequest, MessageSubType aSubType)
{
    Error         error;
    coap::Request udpTx{coap::Type::kNonConfirmable, coap::Code::kPost};
    ByteArray     udpPayload;

    (void)aSubType;

    VerifyOrExit(GetPeerAddr().IsValid() && GetPeerAddr().IsIpv6(),
                 error = ERROR_INVALID_STATE("no valid IPv6 peer address"));

    VerifyOrExit(mBrClient.IsConnected(), error = ERROR_INVALID_STATE("not connected to the border agent"));

    utils::Encode<uint16_t>(udpPayload, mBrClient.GetDtlsSession().GetLocalPort());
    utils::Encode<uint16_t>(udpPayload, GetPeerPort());
    udpPayload.insert(udpPayload.end(), aRequest.begin(), aRequest.end());

    SuccessOrExit(error = udpTx.SetUriPath(uri::kUdpTx));
    SuccessOrExit(error = AppendTlv(udpTx, {tlv::Type::kIpv6Address, mPeerAddr.GetRaw()}));
    SuccessOrExit(error = AppendTlv(udpTx, {tlv::Type::kUdpEncapsulation, udpPayload}));

    mBrClient.SendRequest(udpTx, nullptr);

exit:
    if (!error.NoError())
    {
        error.SetMessage("sending UDP_TX.ntf message failed, " + error.GetMessage());
    }
    return error;
}

void ProxyClient::SendRequest(const coap::Request & aRequest,
                              coap::ResponseHandler aHandler,
                              const Address &       aPeerAddr,
                              uint16_t              aPeerPort)
{
    VerifyOrDie(aPeerAddr.IsValid() && aPeerAddr.IsIpv6());
    mEndpoint.SetPeerAddr(aPeerAddr);
    mEndpoint.SetPeerPort(aPeerPort);

    mCoap.SendRequest(aRequest, aHandler);
}

void ProxyClient::SendEmptyChanged(const coap::Request &aRequest)
{
    mEndpoint.SetPeerAddr(aRequest.GetEndpoint()->GetPeerAddr());
    mEndpoint.SetPeerPort(aRequest.GetEndpoint()->GetPeerPort());

    IgnoreError(mCoap.SendEmptyChanged(aRequest));
}

/**
 * Called when the commissioner received a UDP_RX.ntf request.
 */
void ProxyClient::HandleUdpRx(const coap::Request &aUdpRx)
{
    Error       error;
    Address     peerAddr;
    uint16_t    peerPort;
    tlv::TlvPtr srcAddr  = nullptr;
    tlv::TlvPtr udpEncap = nullptr;

    VerifyOrExit((srcAddr = GetTlv(tlv::Type::kIpv6Address, aUdpRx)) != nullptr,
                 error = ERROR_BAD_FORMAT("no valid IPv6 Address TLV found"));
    VerifyOrExit((udpEncap = GetTlv(tlv::Type::kUdpEncapsulation, aUdpRx)) != nullptr,
                 error = ERROR_BAD_FORMAT("no valid UDP Encapsulation TLV found"));

    SuccessOrExit(error = peerAddr.Set(srcAddr->GetValue()));

    peerPort = utils::Decode<uint16_t>(udpEncap->GetValue());

    mEndpoint.SetPeerAddr(peerAddr);
    mEndpoint.SetPeerPort(peerPort);

    mCoap.Receive({udpEncap->GetValue().begin() + 4, udpEncap->GetValue().end()});

exit:
    if (!error.NoError())
    {
        LOG_WARN(LOG_REGION_COAP, "client(={}) handle UDP_RX.ntf request failed: {}", static_cast<void *>(this),
                 error.ToString());
    }
    return;
}

} // namespace commissioner

} // namespace ot
