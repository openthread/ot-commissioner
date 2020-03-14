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
static const char *kPetitioning         = "/c/cp";
static const char *kKeepAlive           = "/c/ca";
static const char *kUdpRx               = "/c/ur";
static const char *kUdpTx               = "/c/ut";
static const char *kRelayRx             = "/c/rx";
static const char *kRelayTx             = "/c/tx";
static const char *kMgmtGet             = "/c/mg";
static const char *kMgmtSet             = "/c/ms";
static const char *kMgmtCommissionerGet = "/c/cg";
static const char *kMgmtCommissionerSet = "/c/cs";
static const char *kMgmtBbrGet          = "/c/bg";
static const char *kMgmtBbrSet          = "/c/bs";
static const char *kMgmtActiveGet       = "/c/ag";
static const char *kMgmtActiveSet       = "/c/as";
static const char *kMgmtPendingGet      = "/c/pg";
static const char *kMgmtPendingSet      = "/c/ps";
static const char *kMgmtDatasetChanged  = "/c/dc";
static const char *kMgmtAnnounceBegin   = "/c/ab";
static const char *kMgmtPanidQuery      = "/c/pq";
static const char *kMgmtPanidConflict   = "/c/pc";
static const char *kMgmtEdScan          = "/c/es";
static const char *kMgmtEdReport        = "/c/er";
static const char *kMgmtReenroll        = "/c/re";
static const char *kMgmtDomainReset     = "/c/rt";
static const char *kMgmtNetMigrate      = "/c/nm";
static const char *kMgmtSecPendingSet   = "/c/sp";
static const char *kJoinEnt             = "/c/je";
static const char *kJoinFin             = "/c/jf";
static const char *kJoinApp             = "/c/ja";

/*
 * Thread Network Layer URIs
 */
static const char *kMlr = "/n/mr";

/*
 * COM_TOK URI
 */
static const char *kComToken = "/.well-known/ccm";

} // namespace uri

} // namespace commissioner

} // namespace ot

#endif // URI_HPP_
