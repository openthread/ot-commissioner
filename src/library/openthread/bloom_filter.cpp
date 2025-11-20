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

#include "library/openthread/bloom_filter.hpp"

#include <assert.h>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "commissioner/defines.hpp"
#include "library/openthread/crc16.hpp"

namespace ot {

namespace commissioner {

static void inline SetBit(ByteArray &aOut, uint8_t aBit) { aOut[aOut.size() - 1 - (aBit / 8)] |= 1 << (aBit % 8); }

void ComputeBloomFilter(ByteArray &aOut, const ByteArray &aIn)
{
    Crc16 ccitt(Crc16::Polynomial::kCcitt);
    Crc16 ansi(Crc16::Polynomial::kAnsi);

    const size_t numBits = aOut.size() * 8;

    assert(!aOut.empty());
    assert(numBits <= std::numeric_limits<uint8_t>::max());

    for (auto byte : aIn)
    {
        ccitt.Update(byte);
        ansi.Update(byte);
    }

    SetBit(aOut, ccitt.Get() % numBits);
    SetBit(aOut, ansi.Get() % numBits);
}

} // namespace commissioner

} // namespace ot
