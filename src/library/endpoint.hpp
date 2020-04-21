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

#ifndef OT_COMM_LIBRARY_ENDPOINT_HPP_
#define OT_COMM_LIBRARY_ENDPOINT_HPP_

#include <functional>
#include <stdint.h>
#include <string>

#include <commissioner/error.hpp>

#include "common/address.hpp"

namespace ot {

namespace commissioner {

class Endpoint
{
public:
    using Receiver = std::function<void(Endpoint &, const ByteArray &)>;

    Endpoint()          = default;
    virtual ~Endpoint() = default;

    virtual Error    Send(const ByteArray &aBuf) = 0;
    virtual Address  GetPeerAddr() const         = 0;
    virtual uint16_t GetPeerPort() const         = 0;

    void SetReceiver(Receiver aReceiver) { mReceiver = aReceiver; }

protected:
    Receiver mReceiver = nullptr;
};

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_LIBRARY_ENDPOINT_HPP_
