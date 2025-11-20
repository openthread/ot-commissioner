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

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "commissioner/commissioner.hpp"
#include "commissioner/defines.hpp"
#include "commissioner/error.hpp"
#include "commissioner/network_data.hpp"
#include "common/address.hpp"
#include "common/error_macros.hpp"
#include "common/logging.hpp"
#include "common/utils.hpp"
#include "library/coap.hpp"
#include "library/coap_secure.hpp"
#include "library/commissioner_impl.hpp"
#include "library/message.hpp"
#include "library/tlv.hpp"
#include "library/uri.hpp"

namespace ot {

namespace commissioner {

static constexpr size_t kMeshLocalPrefixLength = 8;

ProxyClient::ProxyClient(CommissionerImpl &aCommissioner, coap::CoapSecure &aBrClient)
    : mCommissioner(aCommissioner)
    , mEndpoint(aBrClient)
    , mCoap(aCommissioner.GetEventBase(), mEndpoint)
{
}

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

    // set the source port to default mamagement port for UDP_Tx message
    utils::Encode<uint16_t>(udpPayload, kDefaultMmPort);
    utils::Encode<uint16_t>(udpPayload, GetPeerPort());
    udpPayload.insert(udpPayload.end(), aRequest.begin(), aRequest.end());

    SuccessOrExit(error = udpTx.SetUriPath(uri::kUdpTx));
    SuccessOrExit(error = AppendTlv(udpTx, {tlv::Type::kIpv6Address, mPeerAddr.GetRaw()}));
    SuccessOrExit(error = AppendTlv(udpTx, {tlv::Type::kUdpEncapsulation, udpPayload}));

    mBrClient.SendRequest(udpTx, nullptr);

exit:
    if (error != ErrorCode::kNone)
    {
        error = Error{error.GetCode(), "sending UDP_TX.ntf message failed, " + error.GetMessage()};
    }
    return error;
}

void ProxyClient::SendRequest(const coap::Request  &aRequest,
                              coap::ResponseHandler aHandler,
                              uint16_t              aPeerAloc16,
                              uint16_t              aPeerPort)
{
    if (mMeshLocalPrefix.empty())
    {
        FetchMeshLocalPrefix([=](Error aError) {
            if (aError == ErrorCode::kNone)
            {
                SendRequest(aRequest, aHandler, GetAnycastLocator(aPeerAloc16), aPeerPort);
            }
            else
            {
                aHandler(nullptr, aError);
            }
        });
    }
    else
    {
        SendRequest(aRequest, aHandler, GetAnycastLocator(aPeerAloc16), aPeerPort);
    }
}

void ProxyClient::SendRequest(const coap::Request  &aRequest,
                              coap::ResponseHandler aHandler,
                              const Address        &aPeerAddr,
                              uint16_t              aPeerPort)
{
    VerifyOrDie(aPeerAddr.IsValid() && (aPeerAddr.IsIpv6() || aPeerAddr.IsRloc16()));

    if (aPeerAddr.IsRloc16())
    {
        SendRequest(aRequest, aHandler, aPeerAddr.GetRloc16(), aPeerPort);
        return;
    }

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
    uint16_t    destPort;
    tlv::TlvPtr srcAddr  = nullptr;
    tlv::TlvPtr udpEncap = nullptr;

    VerifyOrExit((srcAddr = GetTlv(tlv::Type::kIpv6Address, aUdpRx)) != nullptr,
                 error = ERROR_BAD_FORMAT("no valid IPv6 Address TLV found"));
    VerifyOrExit((udpEncap = GetTlv(tlv::Type::kUdpEncapsulation, aUdpRx)) != nullptr,
                 error = ERROR_BAD_FORMAT("no valid UDP Encapsulation TLV found"));

    SuccessOrExit(error = peerAddr.Set(srcAddr->GetValue()));

    peerPort = utils::Decode<uint16_t>(udpEncap->GetValue());

    // get the dest port from the UDP_Rx message
    destPort = utils::Decode<uint16_t>(&udpEncap->GetValue()[2], sizeof(uint16_t));
    VerifyOrExit(destPort == kDefaultMmPort,
                 error = ERROR_UNIMPLEMENTED("dropping UDP_RX.ntf message to port {}: only port {} is supported",
                                             destPort, kDefaultMmPort));

    mEndpoint.SetPeerAddr(peerAddr);
    mEndpoint.SetPeerPort(peerPort);

    mCoap.Receive({udpEncap->GetValue().begin() + 4, udpEncap->GetValue().end()});

exit:
    if (error != ErrorCode::kNone)
    {
        LOG_WARN(LOG_REGION_COAP, "client(={}) handle UDP_RX.ntf request failed: {}", static_cast<void *>(this),
                 error.ToString());
    }
}

void ProxyClient::FetchMeshLocalPrefix(Commissioner::ErrorHandler aHandler)
{
    auto onResponse = [=](const ActiveOperationalDataset *aActiveDataset, Error aError) {
        Error error;

        SuccessOrExit(error = aError);
        ASSERT(aActiveDataset != nullptr);

        VerifyOrExit(aActiveDataset->mPresentFlags & ActiveOperationalDataset::kMeshLocalPrefixBit,
                     error = ERROR_BAD_FORMAT("Mesh-Local prefix not included in response"));
        SuccessOrExit(error = SetMeshLocalPrefix(aActiveDataset->mMeshLocalPrefix));

    exit:
        aHandler(aError);
    };

    mCommissioner.GetActiveDataset(onResponse, ActiveOperationalDataset::kMeshLocalPrefixBit);
}

Error ProxyClient::SetMeshLocalPrefix(const ByteArray &aMeshLocalPrefix)
{
    constexpr uint8_t meshLocalPrefix[] = {0xfd};
    Error             error;

    VerifyOrExit(aMeshLocalPrefix.size() == kMeshLocalPrefixLength,
                 error = ERROR_INVALID_ARGS("Thread Mesh-Local Prefix length must be {}", kMeshLocalPrefixLength));
    VerifyOrExit(
        std::equal(aMeshLocalPrefix.begin(), aMeshLocalPrefix.begin() + sizeof(meshLocalPrefix), meshLocalPrefix),
        error = ERROR_INVALID_ARGS("Thread Mesh-Local Prefix must start with fd00::/8"));

    mMeshLocalPrefix = aMeshLocalPrefix;

exit:
    return error;
}

Address ProxyClient::GetAnycastLocator(uint16_t aAloc16) const
{
    ASSERT(!mMeshLocalPrefix.empty());

    Address   ret;
    Error     error;
    ByteArray alocAddr = mMeshLocalPrefix;

    utils::Encode<uint16_t>(alocAddr, 0x0000);
    utils::Encode<uint16_t>(alocAddr, 0x00FF);
    utils::Encode<uint16_t>(alocAddr, 0xFE00);
    utils::Encode<uint16_t>(alocAddr, aAloc16);

    error = ret.Set(alocAddr);
    ASSERT(error == ErrorCode::kNone);

    return ret;
}

} // namespace commissioner

} // namespace ot
