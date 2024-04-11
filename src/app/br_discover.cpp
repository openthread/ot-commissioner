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

#include "br_discover.hpp"

#include <chrono>
#include <thread>
#include <sys/socket.h>

#include "common/error_macros.hpp"
#include "common/utils.hpp"

namespace ot {

namespace commissioner {

Error DiscoverBorderAgent(BorderAgentHandler aBorderAgentHandler, size_t aTimeout, const std::string &aNetIf)
{
    static constexpr size_t             kDefaultBufferSize = 1024 * 16;
    static constexpr mdns_record_type_t kMdnsQueryType     = MDNS_RECORDTYPE_PTR;
    static const char                  *kServiceName       = "_meshcop._udp.local";

    Error   error;
    uint8_t buf[kDefaultBufferSize];

    auto begin = std::chrono::system_clock::now();

    int socket = mdns_socket_open_ipv4();
    VerifyOrExit(socket >= 0, error = ERROR_IO_ERROR("failed to open mDNS IPv4 socket"));

    if (aNetIf != "" && setsockopt(socket, SOL_SOCKET, SO_BINDTODEVICE, &aNetIf[0], sizeof(aNetIf)) < 0) {
         ExitNow(error = ERROR_IO_ERROR("failed to bind network interface: {}", aNetIf));
    }

    if (mdns_query_send(socket, kMdnsQueryType, kServiceName, strlen(kServiceName), buf, sizeof(buf)) != 0)
    {
        ExitNow(error = ERROR_IO_ERROR("failed to send mDNS query"));
    }

    while (begin + std::chrono::milliseconds(aTimeout) >= std::chrono::system_clock::now())
    {
        BorderAgentOrErrorMsg curBorderAgentOrErrorMsg;

        mdns_query_recv(socket, buf, sizeof(buf), HandleRecord, &curBorderAgentOrErrorMsg, 1);

        if (curBorderAgentOrErrorMsg.mError != ErrorCode::kNone)
        {
            aBorderAgentHandler(nullptr, curBorderAgentOrErrorMsg.mError);
        }
        else if (curBorderAgentOrErrorMsg.mBorderAgent.mPresentFlags != 0)
        {
            aBorderAgentHandler(&curBorderAgentOrErrorMsg.mBorderAgent, ERROR_NONE);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

exit:
    if (socket >= 0)
    {
        mdns_socket_close(socket);
    }

    return error;
}

} // namespace commissioner

} // namespace ot
