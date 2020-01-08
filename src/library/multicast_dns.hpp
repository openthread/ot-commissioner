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
 *   The file defines the mDNS querier which discovers Thread Border Agent in
 *   link-local network.
 */

#ifndef MULTICAST_DNS_HPP_
#define MULTICAST_DNS_HPP_

#include <functional>

#include <mdns/mdns.h>

#include <commissioner/commissioner.hpp>

#include "event.hpp"
#include "time.hpp"

namespace ot {

namespace commissioner {

class BorderAgentQuerier
{
public:
    using ResponseHandler = Commissioner::Handler<std::list<BorderAgent>>;

    BorderAgentQuerier(struct event_base *aEventBase);
    ~BorderAgentQuerier();

    void SendQuery(ResponseHandler aHandler);

private:
    static const size_t kDefaultBufferSize = 1024 * 16; // In bytes.
    static const size_t kQueryTimeout      = 4;         // In seconds.

    static void ReceiveResponse(evutil_socket_t aSocket, short aFlags, void *aContext);
    void        ReceiveResponse(evutil_socket_t aSocket, short aFlags);

    static int HandleRecord(const struct sockaddr *from,
                            mdns_entry_type_t      entry,
                            uint16_t               type,
                            uint16_t               rclass,
                            uint32_t               ttl,
                            const void *           data,
                            size_t                 size,
                            size_t                 offset,
                            size_t                 length,
                            void *                 user_data);
    int        HandleRecord(const struct sockaddr *from,
                            mdns_entry_type_t      entry,
                            uint16_t               type,
                            uint16_t               rclass,
                            uint32_t               ttl,
                            const void *           data,
                            size_t                 size,
                            size_t                 offset,
                            size_t                 length);
    bool       ValidateBorderAgent(const BorderAgent &aBorderAgent);
    Error      Setup();

    struct event_base *    mEventBase;
    struct event           mResponseEvent;
    int                    mSocket;
    ResponseHandler        mResponseHandler;
    std::list<BorderAgent> mBorderAgents;
    BorderAgent            mCurBorderAgent;
};

} // namespace commissioner

} // namespace ot

#endif // MULTICAST_DNS_HPP_
