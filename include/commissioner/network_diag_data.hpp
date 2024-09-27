/*
 *    Copyright (c) 2024, The OpenThread Commissioner Authors.
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
 *   This file defines the types of Thread Network Diagnostic TLVs used for network diagnostics.
 */

#ifndef OT_COMM_NETWORK_DIAG_TLVS_HPP_
#define OT_COMM_NETWORK_DIAG_TLVS_HPP_

#include <cstddef>
#include <cstdint>
#include <stdbool.h>
#include <string>
#include <vector>

#include "defines.hpp"
#include "error.hpp"

#define IPV6_ADDRESS_BYTES 16

namespace ot {

namespace commissioner {

/**
 * @brief IPv6 Addresses List
 */
struct Ipv6AddressList
{
    std::vector<std::string> mIpv6Addresses;
};

/**
 * @brief network diagnostic data in TMF
 *
 * Each data field of Diagnostic TLVs is optional. The field is
 * meaningful only when associative PresentFlags is included in
 * `mPresentFlags`.
 */
struct NetDiagData
{
    Ipv6AddressList mIpv6AddressList;

    /**
     * Indicates which fields are included in the dataset.
     */
    uint64_t mPresentFlags;

    static constexpr uint64_t kIpv6AddressBit = (1ull << 0);
};

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_NETWORK_DIAG_TLVS_HPP_
