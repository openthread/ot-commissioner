/*
 *  Copyright (c) 2019, The OpenThread Authors.
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
 *   This file includes definitions for CoAP with DTLS.
 */

#ifndef OT_COMM_LIBRARY_COAP_SECURE_HPP_
#define OT_COMM_LIBRARY_COAP_SECURE_HPP_

#include "library/coap.hpp"
#include "library/dtls.hpp"

namespace ot {

namespace commissioner {

namespace coap {

class CoapSecure
{
public:
    explicit CoapSecure(struct event_base *aEventBase, bool aIsServer = false)
        : mSocket(std::make_shared<UdpSocket>(aEventBase))
        , mDtlsSession(aEventBase, aIsServer, mSocket)
        , mCoap(aEventBase, mDtlsSession)
    {
    }

    ~CoapSecure() = default;

    Error Init(const DtlsConfig &aConfig) { return mDtlsSession.Init(aConfig); }

    Error Start(DtlsSession::ConnectHandler aOnConnected, const std::string &aLocalAddr, uint16_t aLocalPort)
    {
        if (mSocket->Bind(aLocalAddr, aLocalPort) != 0)
        {
            return Error::kTransportFailed;
        }
        mDtlsSession.Connect(aOnConnected);
        return Error::kNone;
    }

    void Connect(DtlsSession::ConnectHandler aOnConnected, const std::string &aPeerAddr, uint16_t aPeerPort)
    {
        if (mSocket->Connect(aPeerAddr, aPeerPort) != 0)
        {
            if (aOnConnected != nullptr)
            {
                aOnConnected(mDtlsSession, Error::kTransportFailed);
                return;
            }
        }
        mDtlsSession.Connect(aOnConnected);
    }

    void Stop() { Disconnect(); }

    void Disconnect()
    {
        mDtlsSession.Disconnect(Error::kAbort);
        mCoap.ClearRequestsAndResponses();
    }

    Error AddResource(const Resource &aResource) { return mCoap.AddResource(aResource); }

    void RemoveResource(const Resource &aResource) { mCoap.RemoveResource(aResource); }

    void SendRequest(const Request &aRequest, ResponseHandler aHandler) { mCoap.SendRequest(aRequest, aHandler); }

    Error SendResponse(const Request &aRequest, Response &aResponse) { return mCoap.SendResponse(aRequest, aResponse); }

    bool IsConnected() const { return mDtlsSession.GetState() == DtlsSession::State::kConnected; }

    const DtlsSession &GetDtlsSession() const { return mDtlsSession; }

    void AbortRequests() { mCoap.AbortRequests(); }

private:
    UdpSocketPtr mSocket;
    DtlsSession  mDtlsSession;
    Coap         mCoap;
};

} // namespace coap

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_LIBRARY_COAP_SECURE_HPP_
