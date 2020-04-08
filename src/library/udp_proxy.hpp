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
 *   This file includes definitions for handling UDP proxy commands.
 */

#ifndef OT_COMM_LIBRARY_UDP_PROXY_HPP_
#define OT_COMM_LIBRARY_UDP_PROXY_HPP_

#include <commissioner/error.hpp>

#include <sys/time.h>

#include <chrono>
#include <functional>

#include "coap_secure.hpp"
#include "endpoint.hpp"
#include <address.hpp>

namespace ot {

namespace commissioner {

// The UDP proxy endpoint that sends UDP data by encapsulating
// it in the UDP_TX.ntf message.
class ProxyEndpoint : public Endpoint
{
public:
    ProxyEndpoint(coap::CoapSecure &aBrClient)
        : mBrClient(aBrClient)
        , mPeerPort(0)
    {
    }
    ~ProxyEndpoint() override = default;

    Error    Send(const ByteArray &aBuf) override;
    Address  GetPeerAddr() const override { return mPeerAddr; }
    uint16_t GetPeerPort() const override { return mPeerPort; }

    void SetPeerAddr(const Address &aPeerAddr) { mPeerAddr = aPeerAddr; }
    void SetPeerPort(uint16_t aPeerPort) { mPeerPort = aPeerPort; }

private:
    coap::CoapSecure &mBrClient;
    Address           mPeerAddr;
    uint16_t          mPeerPort;
};

// The UDP proxy CoAP client that sends CoAP requests encapsulated
// in UDP_TX.ntf message and handles/decodes UDP_RX.ntf message
// into original CoAP message.
class ProxyClient
{
public:
    ProxyClient(struct event_base *aEventBase, coap::CoapSecure &aBrClient)
        : mEndpoint(aBrClient)
        , mCoap(aEventBase, mEndpoint)
    {
    }

    void SendRequest(const coap::Request & aRequest,
                     coap::ResponseHandler aHandler,
                     const Address &       aPeerAddr,
                     uint16_t              aPeerPort);

    void SendEmptyChanged(const coap::Request &aRequest);

    Error AddResource(const coap::Resource &aResource) { return mCoap.AddResource(aResource); }

    void RemoveResource(const coap::Resource &aResource) { mCoap.RemoveResource(aResource); }

    void HandleUdpRx(const coap::Request &aUdpRx);

    void AbortRequests() { mCoap.AbortRequests(); }

private:
    ProxyEndpoint mEndpoint;
    coap::Coap    mCoap;
};

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_LIBRARY_UDP_PROXY_HPP_
