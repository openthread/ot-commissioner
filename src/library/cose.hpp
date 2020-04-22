/*
 *  Copyright (c) 2019, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file includes definitions for COSE.
 *   Ref: https://tools.ietf.org/html/rfc8152
 *
 * @note This file originates from the OpenThread implementation.
 */

#ifndef OT_COMM_LIBRARY_COSE_HPP_
#define OT_COMM_LIBRARY_COSE_HPP_

#include <stddef.h>
#include <stdint.h>

#include <cose.h>
#include <mbedtls/pk.h>

#include <commissioner/defines.hpp>
#include <commissioner/error.hpp>

#include "library/cbor.hpp"

namespace ot {

namespace commissioner {

namespace cose {

static const int kHeaderKeyId     = COSE_Header_KID;
static const int kHeaderAlgorithm = COSE_Header_Algorithm;
static const int kHeaderIV        = COSE_Header_IV;

static const int kKeyId              = COSE_Key_ID;
static const int kKeyType            = COSE_Key_Type;
static const int kKeyTypeEC2         = COSE_Key_Type_EC2;
static const int kKeyEC2Curve        = COSE_Key_EC2_Curve;
static const int kKeyEC2CurveP256    = 1;
static const int kKeyEC2CurveP384    = 2;
static const int kKeyEC2CurveP521    = 3;
static const int kKeyEC2X            = COSE_Key_EC2_X;
static const int kKeyEC2Y            = COSE_Key_EC2_Y;
static const int kInitFlagsNone      = COSE_INIT_FLAGS_NONE;
static const int kAlgEcdsaWithSha256 = COSE_Algorithm_ECDSA_SHA_256;
static const int kProtectOnly        = COSE_PROTECT_ONLY;
static const int kUnprotectOnly      = COSE_UNPROTECT_ONLY;

class Object
{
public:
    virtual ~Object() = default;
};

class Sign1Message : public Object
{
public:
    Sign1Message(void);
    const Sign1Message &operator=(const Sign1Message &aOther) = delete;

    Error Init(int aCoseInitFlags);

    Error        Serialize(ByteArray &aBuf);
    static Error Deserialize(Sign1Message &aCose, const ByteArray &aBuf);
    // OpenThread hates destructor.
    void Free();

    Error Validate(const CborMap &aCborPublicKey);

    Error Validate(const mbedtls_pk_context &aPublicKey);

    Error Sign(const mbedtls_pk_context &aPrivateKey);

    Error SetContent(const ByteArray &aContent);

    Error SetExternalData(const ByteArray &aExternalData);

    Error AddAttribute(int aKey, int aValue, int aFlags);
    Error AddAttribute(int aKey, const ByteArray &aValue, int aFlags);

    const uint8_t *GetPayload(size_t &aLength);

private:
    Error AddAttribute(int key, cn_cbor *value, int flags);

    HCOSE_SIGN0 mSign;
};

Error MakeCoseKey(ByteArray &aEncodedCoseKey, const mbedtls_pk_context &aKey, const ByteArray &aKeyId);

} // namespace cose

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_LIBRARY_COSE_HPP_
