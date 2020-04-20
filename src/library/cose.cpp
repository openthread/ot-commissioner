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
 *   This file implements COSE.
 */

#include "cose.hpp"

#include <mbedtls/bignum.h>
#include <mbedtls/ecp.h>

#include <utils.hpp>

namespace ot {

namespace commissioner {

namespace cose {

Sign1Message::Sign1Message(void)
    : mSign(nullptr)
{
}

void Sign1Message::Free(void)
{
    if (mSign != nullptr)
    {
        COSE_Sign0_Free(mSign);
        mSign = nullptr;
    }
}

Error Sign1Message::Init(int aCoseInitFlags)
{
    mSign = COSE_Sign0_Init(static_cast<COSE_INIT_FLAGS>(aCoseInitFlags), nullptr);
    return mSign ? Error::kNone : Error::kOutOfMemory;
}

Error Sign1Message::Serialize(ByteArray &aBuf)
{
    Error  error = Error::kNone;
    size_t length;

    length = COSE_Encode((HCOSE)mSign, nullptr, 0, 0) + 1;
    aBuf.resize(length);
    length = COSE_Encode((HCOSE)mSign, &aBuf[0], 0, aBuf.size());
    aBuf.resize(length);

    return error;
}

Error Sign1Message::Deserialize(Sign1Message &aCose, const ByteArray &aBuf)
{
    Error       error = Error::kNone;
    int         type;
    HCOSE_SIGN1 sign;

    VerifyOrExit(!aBuf.empty(), error = Error::kInvalidArgs);
    sign = reinterpret_cast<HCOSE_SIGN1>(COSE_Decode(&aBuf[0], aBuf.size(), &type, COSE_sign1_object, nullptr));
    VerifyOrExit(sign != nullptr && type == COSE_sign1_object, error = Error::kBadFormat);

    aCose.mSign = sign;

exit:
    return error;
}

Error Sign1Message::Validate(const CborMap &aCborPublicKey)
{
    Error error = Error::kNone;

    VerifyOrExit(aCborPublicKey.IsValid(), error = Error::kInvalidArgs);

    VerifyOrExit(COSE_Sign0_validate(mSign, aCborPublicKey.GetImpl(), nullptr), error = Error::kSecurity);

exit:
    return error;
}

Error Sign1Message::Validate(const mbedtls_pk_context &aPubKey)
{
    Error                             error = Error::kNone;
    const struct mbedtls_ecp_keypair *eckey;

    // Accepts only EC keys
    VerifyOrExit(mbedtls_pk_can_do(&aPubKey, MBEDTLS_PK_ECDSA), error = Error::kInvalidArgs);
    VerifyOrExit((eckey = mbedtls_pk_ec(aPubKey)) != nullptr, error = Error::kInvalidArgs);

    // VerifyOrExit(COSE_Sign1_validate_eckey(mSign, eckey, nullptr), error = Error::kSecurity);

exit:
    return error;
}

Error Sign1Message::Sign(const mbedtls_pk_context &aPrivateKey)
{
    Error                      error = Error::kNone;
    const mbedtls_ecp_keypair *eckey = nullptr;

    VerifyOrExit(mbedtls_pk_can_do(&aPrivateKey, MBEDTLS_PK_ECDSA), error = Error::kInvalidArgs);
    VerifyOrExit((eckey = mbedtls_pk_ec(aPrivateKey)) != nullptr, error = Error::kInvalidArgs);

    // VerifyOrExit(COSE_Sign1_Sign_eckey(mSign, eckey, nullptr), error = Error::kSecurity);

exit:
    return error;
}

Error Sign1Message::SetContent(const ByteArray &aContent)
{
    Error   error = Error::kNone;
    uint8_t emptyContent;

    if (!aContent.empty())
    {
        VerifyOrExit(COSE_Sign0_SetContent(mSign, &aContent[0], aContent.size(), nullptr), error = Error::kFailed);
    }
    else
    {
        VerifyOrExit(COSE_Sign0_SetContent(mSign, &emptyContent, 0, nullptr), error = Error::kFailed);
    }

exit:
    return error;
}

Error Sign1Message::SetExternalData(const ByteArray &aExternalData)
{
    Error error = Error::kNone;

    VerifyOrExit(!aExternalData.empty(), error = Error::kInvalidArgs);
    VerifyOrExit(COSE_Sign0_SetExternal(mSign, &aExternalData[0], aExternalData.size(), nullptr),
                 error = Error::kFailed);

exit:
    return error;
}

Error Sign1Message::AddAttribute(int key, int value, int flags)
{
    Error error = Error::kNone;

    cn_cbor *cbor = cn_cbor_int_create(value, nullptr);
    VerifyOrExit(cbor != nullptr, error = Error::kOutOfMemory);

    error = AddAttribute(key, cbor, flags);

exit:
    if (cbor != nullptr && cbor->parent == nullptr)
    {
        cn_cbor_free(cbor);
    }
    return error;
}

Error Sign1Message::AddAttribute(int aKey, const ByteArray &aValue, int aFlags)
{
    Error    error = Error::kNone;
    cn_cbor *cbor  = nullptr;

    VerifyOrExit(!aValue.empty(), error = Error::kInvalidArgs);
    cbor = cn_cbor_data_create(&aValue[0], aValue.size(), nullptr);
    VerifyOrExit(cbor != nullptr, error = Error::kOutOfMemory);

    SuccessOrExit(error = AddAttribute(aKey, cbor, aFlags));

exit:
    if (cbor != nullptr && cbor->parent == nullptr)
    {
        cn_cbor_free(cbor);
    }
    return error;
}

Error Sign1Message::AddAttribute(int key, cn_cbor *value, int flags)
{
    Error error = Error::kNone;

    VerifyOrExit(COSE_Sign0_map_put_int(mSign, key, value, flags, nullptr), error = Error::kFailed);

exit:
    return error;
}

static cn_cbor *CborArrayAt(cn_cbor *arr, size_t index)
{
    cn_cbor *ele = nullptr;

    VerifyOrExit(index <= static_cast<size_t>(arr->length));

    ele = arr->first_child;
    for (size_t i = 0; i < index; ++i)
    {
        ele = ele->next;
    }
exit:
    return ele;
}

const uint8_t *Sign1Message::GetPayload(size_t &aLength)
{
    const uint8_t *ret = nullptr;
    cn_cbor *      cbor;
    cn_cbor *      payload;

    ASSERT(mSign != nullptr);
    VerifyOrExit((cbor = COSE_get_cbor(reinterpret_cast<HCOSE>(mSign))) != nullptr);

    VerifyOrExit(cbor->type == CN_CBOR_ARRAY && (payload = CborArrayAt(cbor, 2)) != nullptr);

    VerifyOrExit(payload->type == CN_CBOR_BYTES);

    ret     = payload->v.bytes;
    aLength = payload->length;

exit:
    return ret;
}

Error MakeCoseKey(ByteArray &aEncodedCoseKey, const mbedtls_pk_context &aKey, const ByteArray &aKeyId)
{
    static constexpr size_t kMaxCoseKeyLength = 1024;

    Error                             error = Error::kInvalidArgs;
    const struct mbedtls_ecp_keypair *eckey;
    int                               ec2Curve;
    uint8_t                           xPoint[MBEDTLS_ECP_MAX_PT_LEN];
    size_t                            xLength;
    uint8_t                           yPoint[MBEDTLS_ECP_MAX_PT_LEN];
    size_t                            yLength;
    CborMap                           coseKey;
    uint8_t                           encodedCoseKey[kMaxCoseKeyLength];
    size_t                            encodedCoseKeyLength;

    VerifyOrExit(mbedtls_pk_can_do(&aKey, MBEDTLS_PK_ECDSA));
    VerifyOrExit((eckey = mbedtls_pk_ec(aKey)) != nullptr);

    SuccessOrExit(error = coseKey.Init());

    // Cose key id('kid')
    if (!aKeyId.empty())
    {
        SuccessOrExit(error = coseKey.Put(kKeyId, &aKeyId[0], aKeyId.size()));
    }

    // Cose key type
    SuccessOrExit(error = coseKey.Put(kKeyType, kKeyTypeEC2));

    // Cose key EC2 curve
    switch (eckey->grp.id)
    {
    case MBEDTLS_ECP_DP_SECP256R1:
        ec2Curve = kKeyEC2CurveP256;
        break;
    case MBEDTLS_ECP_DP_SECP384R1:
        ec2Curve = kKeyEC2CurveP384;
        break;
    case MBEDTLS_ECP_DP_SECP521R1:
        ec2Curve = kKeyEC2CurveP521;
        break;
    default:
        ExitNow();
    }
    SuccessOrExit(error = coseKey.Put(kKeyEC2Curve, ec2Curve));

    // Cose key EC2 X
    VerifyOrExit(mbedtls_mpi_write_binary(&eckey->Q.X, xPoint, sizeof(xPoint)) == 0);

    // 'mbedtls_mpi_write_binary' writes to the end of buffer
    xLength = mbedtls_mpi_size(&eckey->Q.X);
    SuccessOrExit(error = coseKey.Put(kKeyEC2X, xPoint + sizeof(xPoint) - xLength, xLength));

    // TODO(wgtdkp): handle the situation that Y point is not presented.
    // Cose key EC2 Y
    VerifyOrExit(mbedtls_mpi_write_binary(&eckey->Q.Y, yPoint, sizeof(yPoint)) == 0);
    yLength = mbedtls_mpi_size(&eckey->Q.Y);
    SuccessOrExit(error = coseKey.Put(kKeyEC2Y, yPoint + sizeof(yPoint) - yLength, yLength));

    SuccessOrExit(error = coseKey.Serialize(encodedCoseKey, encodedCoseKeyLength, sizeof(encodedCoseKey)));

    aEncodedCoseKey.assign(encodedCoseKey, encodedCoseKey + encodedCoseKeyLength);
    error = Error::kNone;

exit:
    return error;
}

} // namespace cose

} // namespace commissioner

} // namespace ot
