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
 *   This file implements CBOR.
 */

#include "library/cbor.hpp"

#include <assert.h>

#include <cn-cbor/cn-cbor.h>

namespace ot {

namespace commissioner {

CborValue::CborValue()
    : mIsRoot(true)
    , mCbor(nullptr)
{
}

void CborValue::Free(void)
{
    if (mIsRoot && mCbor != nullptr)
    {
        cn_cbor_free(mCbor);
        mCbor = nullptr;
    }
}

void CborValue::Move(CborValue &dst, CborValue &src)
{
    dst.Free();
    dst = src;

    src.mIsRoot = true;
    src.mCbor   = nullptr;
}

Error CborValue::Serialize(uint8_t *aBuf, size_t &aLength, size_t aMaxLength) const
{
    assert(mIsRoot && mCbor != nullptr);
    ssize_t written = cn_cbor_encoder_write(aBuf, 0, aMaxLength, mCbor);
    if (written == -1)
        return Error::kOutOfMemory;
    aLength = written;
    return Error::kNone;
}

Error CborValue::Deserialize(CborValue &aValue, const uint8_t *aBuf, size_t aLength)
{
    cn_cbor *cbor = cn_cbor_decode(aBuf, aLength, nullptr);
    if (cbor == nullptr)
        return Error::kBadFormat;
    aValue.mIsRoot = true;
    aValue.mCbor   = cbor;
    return Error::kNone;
}

Error CborMap::Init(void)
{
    assert(mCbor == nullptr);
    mIsRoot = true;
    mCbor   = cn_cbor_map_create(nullptr);
    return mCbor ? Error::kNone : Error::kOutOfMemory;
}

Error CborMap::Put(int aKey, const CborMap &aMap)
{
    aMap.mIsRoot = false;
    return cn_cbor_mapput_int(mCbor, aKey, aMap.mCbor, nullptr) ? Error::kNone : Error::kOutOfMemory;
}

Error CborMap::Put(int aKey, int aValue)
{
    cn_cbor *cborInt = cn_cbor_int_create(aValue, nullptr);
    bool     succeed = cborInt && cn_cbor_mapput_int(mCbor, aKey, cborInt, nullptr);
    return succeed ? Error::kNone : Error::kOutOfMemory;
}

Error CborMap::Put(int aKey, const uint8_t *aBytes, size_t aLength)
{
    cn_cbor *cborBytes = cn_cbor_data_create(aBytes, aLength, nullptr);
    bool     succeed   = cborBytes && cn_cbor_mapput_int(mCbor, aKey, cborBytes, nullptr);
    return succeed ? Error::kNone : Error::kOutOfMemory;
}

Error CborMap::Put(int aKey, const char *aStr)
{
    cn_cbor *cborStr = cn_cbor_string_create(aStr, nullptr);
    bool     succeed = cborStr && cn_cbor_mapput_int(mCbor, aKey, cborStr, nullptr);
    return succeed ? Error::kNone : Error::kOutOfMemory;
}

Error CborMap::Get(int aKey, CborMap &aMap) const
{
    cn_cbor *cbor = cn_cbor_mapget_int(mCbor, aKey);
    if (cbor == nullptr)
        return Error::kNotFound;
    aMap.mIsRoot = false;
    aMap.mCbor   = cbor;
    return Error::kNone;
}

Error CborMap::Get(int aKey, int &aInt) const
{
    cn_cbor *cborInt = cn_cbor_mapget_int(mCbor, aKey);
    if (cborInt == nullptr)
        return Error::kNotFound;
    aInt = cborInt->v.sint;
    return Error::kNone;
}

Error CborMap::Get(int aKey, const uint8_t *&aBytes, size_t &aLength) const
{
    cn_cbor *cborBytes = cn_cbor_mapget_int(mCbor, aKey);
    if (cborBytes == nullptr)
        return Error::kNotFound;
    aBytes  = cborBytes->v.bytes;
    aLength = cborBytes->length;
    return Error::kNone;
}

Error CborMap::Get(int aKey, const char *&aStr, size_t &aLength) const
{
    cn_cbor *cborStr = cn_cbor_mapget_int(mCbor, aKey);
    if (cborStr == nullptr)
        return Error::kNotFound;
    aStr    = cborStr->v.str;
    aLength = cborStr->length;
    return Error::kNone;
}

} // namespace commissioner

} // namespace ot
