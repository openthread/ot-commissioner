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
 *   This file includes definitions of all CoAP resources.
 */

#ifndef URI_HPP_
#define URI_HPP_

#include <string>

namespace ot {

namespace commissioner {

namespace uri {

/*
 * Thread MeshCoP URIs
 */
static const std::string kPetitioning         = "/c/cp";
static const std::string kKeepAlive           = "/c/ca";
static const std::string kUdpRx               = "/c/ur";
static const std::string kUdpTx               = "/c/ut";
static const std::string kRelayRx             = "/c/rx";
static const std::string kRelayTx             = "/c/tx";
static const std::string kMgmtGet             = "/c/mg";
static const std::string kMgmtSet             = "/c/ms";
static const std::string kMgmtCommissionerGet = "/c/cg";
static const std::string kMgmtCommissionerSet = "/c/cs";
static const std::string kMgmtBbrGet          = "/c/bg";
static const std::string kMgmtBbrSet          = "/c/bs";
static const std::string kMgmtActiveGet       = "/c/ag";
static const std::string kMgmtActiveSet       = "/c/as";
static const std::string kMgmtPendingGet      = "/c/pg";
static const std::string kMgmtPendingSet      = "/c/ps";
static const std::string kMgmtDatasetChanged  = "/c/dc";
static const std::string kMgmtAnnounceBegin   = "/c/ab";
static const std::string kMgmtPanidQuery      = "/c/pq";
static const std::string kMgmtPanidConflict   = "/c/pc";
static const std::string kMgmtEdScan          = "/c/es";
static const std::string kMgmtEdReport        = "/c/er";
static const std::string kMgmtReenroll        = "/c/re";
static const std::string kMgmtDomainReset     = "/c/rt";
static const std::string kMgmtNetMigrate      = "/c/nm";
static const std::string kMgmtSecPendingSet   = "/c/sp";
static const std::string kJoinEnt             = "/c/je";
static const std::string kJoinFin             = "/c/jf";
static const std::string kJoinApp             = "/c/ja";

/*
 * Thread Network Layer URIs
 */
static const std::string kMlr = "/n/mr";

/*
 * COM_TOK URI
 */
static const std::string kComToken = "/.well-known/ccm";

} // namespace uri

} // namespace commissioner

} // namespace ot

#endif // URI_HPP_
