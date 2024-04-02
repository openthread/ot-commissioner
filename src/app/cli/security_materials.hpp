/*
 *    Copyright (c) 2020, The OpenThread Commissioner Authors.
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
 *   The file defines security materials storage.
 */

#ifndef OT_COMM_APP_CLI_SECURITY_MATERIALS_HPP_
#define OT_COMM_APP_CLI_SECURITY_MATERIALS_HPP_

#include <string>

#include "commissioner/commissioner.hpp"
#include "commissioner/defines.hpp"
#include "commissioner/error.hpp"

namespace ot {

namespace commissioner {

namespace security_material {

struct SecurityMaterials
{
    // Mandatory for non-CCM Thread network.
    ByteArray mPSKc; ///< The pre-shared commissioner key.

    // Mandatory for CCM Thread network.
    ByteArray mPrivateKey; ///< The private EC key.

    // Mandatory for CCM Thread network.
    ByteArray mCertificate; ///< The certificate signed by domain registrar.

    // Mandatory for CCM Thread network.
    ByteArray mTrustAnchor; ///< The trust anchor of 'mCertificate'.

    // Optional for CCM Thread network.
    ByteArray mCommissionerToken; ///< COM_TOK

    // See if any part of credentials is already found depending on the
    // credentials type
    bool IsAnyFound(bool aNeedCert, bool aNeedPSKc, bool aNeedToken = false) const;

    // See if any part of credentials is missing depending on the
    // credentials type
    bool IsIncomplete(bool aNeedCert, bool aNeedPSKc, bool aNeedToken = false) const;

    // See if entire set of credentials is empty
    bool IsEmpty(bool isCCM) const;
};

/**
 * Initializes access to Security Materials Storage. Internally, SM
 * root path is retrieved, either from default configuration or
 * environment variable THREAD_SM_ROOT.
 */
Error Init(const Config &aDefaultConfig);

/**
 * Finds security materials related to a domain. Domain credentials
 * are always expected to be the ones applicable for CCM networks:
 * - mCertificate
 * - mPrivateKey
 * - nTrustAnchor
 * - mCommissionerToken
 * where any element is optional.
 *
 * @see JobManager::PrepareDtlsConfig()
 */
Error GetDomainSM(const std::string &aDid, SecurityMaterials &aSM);

/**
 * Finds security materials related to a network. The returned content
 * depends on the flags aNeedCert and aNeedPSKc, although again, any
 * element of returned SecurityMaterials is optional.
 *
 * The requested network is identified by an alias which may be XPAN
 * ID or network name. Only these two are allowed to form a path in SM
 * Storage, but not PAN ID.
 *
 * @see JobManager::PrepareDtlsConfig()
 */
Error GetNetworkSM(const std::string &aAlias, bool aNeedCert, bool aNeedPSKc, SecurityMaterials &aSM);

} // namespace security_material

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_APP_CLI_SECURITY_MATERIALS_HPP_
