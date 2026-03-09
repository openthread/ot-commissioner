/*
 *    Copyright (c) 2026, The OpenThread Commissioner Authors.
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
 *   This file defines a test peer for Commissioner classes to access private members in tests.
 */

#ifndef OT_COMM_LIBRARY_COMMISSIONER_TEST_PEER_HPP_
#define OT_COMM_LIBRARY_COMMISSIONER_TEST_PEER_HPP_

#include <map>
#include <memory>

#include "library/commissioner_impl.hpp"
#include "library/commissioner_safe.hpp"

namespace ot {

namespace commissioner {

class CommissionerTestPeer
{
public:
    static std::shared_ptr<CommissionerImpl> &GetImpl(CommissionerSafe &aSafe) { return aSafe.mImpl; }

    static std::map<ByteArray, JoinerSession> &GetJoinerSessions(CommissionerImpl &aImpl)
    {
        return aImpl.mJoinerSessions;
    }
};

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_LIBRARY_COMMISSIONER_TEST_PEER_HPP_
