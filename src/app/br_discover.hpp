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

#ifndef OT_COMM_BR_DISCOVER_HPP
#define OT_COMM_BR_DISCOVER_HPP

#include <functional>

#include "commissioner/error.hpp"

#include "border_agent.hpp"
#include "mdns_handler.hpp"

namespace ot {

namespace commissioner {

/**
 * Callback prototype for Border Agent discovery response handling.
 *
 * @param[in] aBorderAgent   A received Border Agent information. Not null
 *                           only when aError == ErrorCode::kNone is true.
 * @param[in] aError         A resultant information about handling.
 *
 */
using BorderAgentHandler = std::function<void(const BorderAgent *aBorderAgent, const Error &aError)>;

/**
 * Discovers Border Agents in local network with mDNS.
 *
 * @param[in] aBorderAgentHandler  The handler of a single Border Agent response.
 * @param[in] aTimeout             The time to wait for mDNS responses. Any
 *                                 response not within the interval is ignored.
 * @param[in] aNetIf               The specified network interface for mDNS binding.
 *
 *
 */
Error DiscoverBorderAgent(BorderAgentHandler aBorderAgentHandler, size_t aTimeout, const std::string &aNetIf);

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_BR_DISCOVER_HPP
