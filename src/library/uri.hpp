/*
 *    Copyright (c) 2019, The OpenThread Commissioner Authors.
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

#ifndef OT_COMM_LIBRARY_URI_HPP_
#define OT_COMM_LIBRARY_URI_HPP_

namespace ot {

namespace commissioner {

namespace uri {

/*
 * Thread MeshCoP URIs
 */
static const char *const kPetitioning         = "/c/cp";
static const char *const kLeaderPetitioning   = "/c/lp";
static const char *const kKeepAlive           = "/c/ca";
static const char *const kLeaderKeepAlive     = "/c/la";
static const char *const kUdpRx               = "/c/ur";
static const char *const kUdpTx               = "/c/ut";
static const char *const kRelayRx             = "/c/rx";
static const char *const kRelayTx             = "/c/tx";
static const char *const kMgmtGet             = "/c/mg";
static const char *const kMgmtSet             = "/c/ms";
static const char *const kMgmtCommissionerGet = "/c/cg";
static const char *const kMgmtCommissionerSet = "/c/cs";
static const char *const kMgmtBbrGet          = "/c/bg";
static const char *const kMgmtBbrSet          = "/c/bs";
static const char *const kMgmtActiveGet       = "/c/ag";
static const char *const kMgmtActiveSet       = "/c/as";
static const char *const kMgmtPendingGet      = "/c/pg";
static const char *const kMgmtPendingSet      = "/c/ps";
static const char *const kMgmtDatasetChanged  = "/c/dc";
static const char *const kMgmtAnnounceBegin   = "/c/ab";
static const char *const kMgmtPanidQuery      = "/c/pq";
static const char *const kMgmtPanidConflict   = "/c/pc";
static const char *const kMgmtEdScan          = "/c/es";
static const char *const kMgmtEdReport        = "/c/er";
static const char *const kMgmtReenroll        = "/c/re";
static const char *const kMgmtDomainReset     = "/c/rt";
static const char *const kMgmtNetMigrate      = "/c/nm";
static const char *const kMgmtSecPendingSet   = "/c/sp";
static const char *const kJoinEnt             = "/c/je";
static const char *const kJoinFin             = "/c/jf";
static const char *const kJoinApp             = "/c/ja";

/*
 * Thread diagnostic URIs
 */

static const char *const kDiagGet      = "/d/dg";
static const char *const kDiagGetQuery = "/d/dq";
static const char *const kDiagGetAns   = "/d/da";
static const char *const kDiagRst      = "/d/dr";

/*
 * Thread Network Layer URIs
 */
static const char *const kMlr = "/n/mr";

/*
 * COM_TOK URI
 */
static const char *const kComToken = "/.well-known/ccm";

} // namespace uri

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_LIBRARY_URI_HPP_
