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
 *   The file implements JSON-based persistent storage.
 */

#include "persistent_storage_json.hpp"
#include "app/file_util.hpp"
#include "common/utils.hpp"

namespace ot {
namespace commissioner {
namespace persistent_storage {

using namespace ::ot::commissioner::utils;

const std::string JSON_RGR     = "rgr";
const std::string JSON_RGR_SEQ = "rgr_seq";

const std::string JSON_DOM     = "dom";
const std::string JSON_DOM_SEQ = "dom_seq";

const std::string JSON_NWK     = "nwk";
const std::string JSON_NWK_SEQ = "nwk_seq";

const std::string JSON_BR     = "br";
const std::string JSON_BR_SEQ = "br_seq";

const std::string JSON_CURR_NWK = "curr_nwk";

using nlohmann::json;
using SemaphoreStatus = ot::os::semaphore::SemaphoreStatus;

PersistentStorageJson::PersistentStorageJson(std::string const &aFileName)
    : mFileName(aFileName)
    , mCache()
    , mStorageLock()
{
    SemaphoreOpen("thrcomm_json_storage", mStorageLock);
}

PersistentStorageJson::~PersistentStorageJson()
{
    Close();
    mCache.clear();
    SemaphoreClose(mStorageLock);
}

json PersistentStorageJson::JsonDefault()
{
    return json{
        {JSON_RGR_SEQ, RegistrarId(0)},       {JSON_DOM_SEQ, DomainId(0)},
        {JSON_NWK_SEQ, NetworkId(0)},         {JSON_BR_SEQ, BorderRouterId(0)},

        {JSON_RGR, std::vector<Registrar>{}}, {JSON_DOM, std::vector<Domain>{}},
        {JSON_NWK, std::vector<Network>{}},   {JSON_BR, std::vector<BorderRouter>{}},

        {JSON_CURR_NWK, NetworkId{EMPTY_ID}},
    };
}

bool PersistentStorageJson::CacheStructValidation()
{
    json base = JsonDefault();

    for (auto &el : base.items())
    {
        auto toCheck = mCache.find(el.key());
        if (toCheck == mCache.end() || toCheck->type_name() != el.value().type_name())
        {
            return false;
        }
    }

    return true;
}

PersistentStorage::Status PersistentStorageJson::CacheFromFile()
{
    if (mFileName.empty())
    {
        // No persistence, nothing to do
        return PersistentStorage::Status::PS_SUCCESS;
    }

    if (SemaphoreWait(mStorageLock) != SemaphoreStatus::kSuccess)
    {
        return PersistentStorage::Status::PS_ERROR;
    }

    if (RestoreFilePath(mFileName).GetCode() != ErrorCode::kNone)
    {
        SemaphorePost(mStorageLock);
        return PersistentStorage::Status::PS_ERROR;
    }

    std::string fdata;
    Error       error = ReadFile(fdata, mFileName);
    if (error.GetCode() != ErrorCode::kNone)
    {
        SemaphorePost(mStorageLock);
        return PersistentStorage::Status::PS_ERROR;
    }

    if (fdata.size() == 0)
    {
        mCache = JsonDefault();

        SemaphorePost(mStorageLock);
        return PersistentStorage::Status::PS_SUCCESS;
    }

    try
    {
        mCache = json::parse(fdata);
    } catch (json::parse_error const &)
    {
        SemaphorePost(mStorageLock);
        return PersistentStorage::Status::PS_ERROR;
    }

    if (mCache.empty())
    {
        mCache = JsonDefault();
    }

    // base validation
    if (!CacheStructValidation())
    {
        SemaphorePost(mStorageLock);
        return PersistentStorage::Status::PS_ERROR;
    }

    SemaphorePost(mStorageLock);
    return PersistentStorage::Status::PS_SUCCESS;
}

PersistentStorage::Status PersistentStorageJson::CacheToFile()
{
    if (mFileName.empty())
    {
        // No persistence, nothing to do
        return PersistentStorage::Status::PS_SUCCESS;
    }

    if (SemaphoreWait(mStorageLock) != SemaphoreStatus::kSuccess)
    {
        return PersistentStorage::Status::PS_ERROR;
    }

    Error error = WriteFile(mCache.dump(4), mFileName);
    if (error.GetCode() != ErrorCode::kNone)
    {
        SemaphorePost(mStorageLock);
        return PersistentStorage::Status::PS_ERROR;
    }

    SemaphorePost(mStorageLock);
    return PersistentStorage::Status::PS_SUCCESS;
}

PersistentStorage::Status PersistentStorageJson::Open()
{
    if (mFileName.empty())
    {
        // No persistence, use default contents
        mCache = JsonDefault();
    }
    CacheFromFile();
    return CacheToFile();
}

PersistentStorage::Status PersistentStorageJson::Close()
{
    return CacheToFile();
}

PersistentStorage::Status PersistentStorageJson::Add(Registrar const &aValue, RegistrarId &aRetId)
{
    return AddOne<Registrar, RegistrarId>(aValue, aRetId, JSON_RGR_SEQ, JSON_RGR);
}

PersistentStorage::Status PersistentStorageJson::Add(Domain const &aValue, DomainId &aRetId)
{
    return AddOne<Domain, DomainId>(aValue, aRetId, JSON_DOM_SEQ, JSON_DOM);
}

PersistentStorage::Status PersistentStorageJson::Add(Network const &aValue, NetworkId &aRetId)
{
    return AddOne<Network, NetworkId>(aValue, aRetId, JSON_NWK_SEQ, JSON_NWK);
}

PersistentStorage::Status PersistentStorageJson::Add(BorderRouter const &aValue, BorderRouterId &aRetId)
{
    return AddOne<BorderRouter, BorderRouterId>(aValue, aRetId, JSON_BR_SEQ, JSON_BR);
}

PersistentStorage::Status PersistentStorageJson::Del(RegistrarId const &aId)
{
    return DelId<Registrar, RegistrarId>(aId, JSON_RGR);
}

PersistentStorage::Status PersistentStorageJson::Del(DomainId const &aId)
{
    return DelId<Domain, DomainId>(aId, JSON_DOM);
}

PersistentStorage::Status PersistentStorageJson::Del(NetworkId const &aId)
{
    return DelId<Network, NetworkId>(aId, JSON_NWK);
}

PersistentStorage::Status PersistentStorageJson::Del(BorderRouterId const &aId)
{
    return DelId<BorderRouter, BorderRouterId>(aId, JSON_BR);
}

PersistentStorage::Status PersistentStorageJson::Get(RegistrarId const &aId, Registrar &aRetValue)
{
    return GetId<Registrar, RegistrarId>(aId, aRetValue, JSON_RGR);
}

PersistentStorage::Status PersistentStorageJson::Get(DomainId const &aId, Domain &aRetValue)
{
    return GetId<Domain, DomainId>(aId, aRetValue, JSON_DOM);
}

PersistentStorage::Status PersistentStorageJson::Get(NetworkId const &aId, Network &aRetValue)
{
    return GetId<Network, NetworkId>(aId, aRetValue, JSON_NWK);
}

PersistentStorage::Status PersistentStorageJson::Get(BorderRouterId const &aId, BorderRouter &aRetValue)
{
    return GetId<BorderRouter, BorderRouterId>(aId, aRetValue, JSON_BR);
}

PersistentStorage::Status PersistentStorageJson::Update(Registrar const &aValue)
{
    return UpdId<Registrar>(aValue, JSON_RGR);
}

PersistentStorage::Status PersistentStorageJson::Update(Domain const &aValue)
{
    return UpdId<Domain>(aValue, JSON_DOM);
}

PersistentStorage::Status PersistentStorageJson::Update(Network const &aValue)
{
    return UpdId<Network>(aValue, JSON_NWK);
}

PersistentStorage::Status PersistentStorageJson::Update(BorderRouter const &aValue)
{
    return UpdId<BorderRouter>(aValue, JSON_BR);
}

PersistentStorage::Status PersistentStorageJson::Lookup(Registrar const &aValue, std::vector<Registrar> &aRet)
{
    std::function<bool(Registrar const &)> pred = [](Registrar const &) { return true; };

    pred = [aValue](Registrar const &el) {
        bool aRet = (aValue.mId.mId == EMPTY_ID || (el.mId.mId == aValue.mId.mId)) &&
                    (aValue.mAddr.empty() || CaseInsensitiveEqual(aValue.mAddr, el.mAddr)) &&
                    (aValue.mPort == 0 || (aValue.mPort == el.mPort));

        if (aRet && !aValue.mDomains.empty())
        {
            std::vector<std::string> el_tmp(el.mDomains);
            std::vector<std::string> aValue_tmp(aValue.mDomains);
            std::sort(std::begin(el_tmp), std::end(el_tmp));
            std::sort(std::begin(aValue_tmp), std::end(aValue_tmp));
            aRet = std::includes(std::begin(el_tmp), std::end(el_tmp), std::begin(aValue_tmp), std::end(aValue_tmp));
        }
        return aRet;
    };

    return LookupPred<Registrar>(pred, aRet, JSON_RGR);
}

PersistentStorage::Status PersistentStorageJson::Lookup(Domain const &aValue, std::vector<Domain> &aRet)
{
    std::function<bool(Domain const &)> pred = [](Domain const &) { return true; };

    pred = [aValue](Domain const &el) {
        bool aRet = (aValue.mId.mId == EMPTY_ID || (el.mId.mId == aValue.mId.mId)) &&
                    (aValue.mName.empty() || (aValue.mName == el.mName));
        return aRet;
    };

    return LookupPred<Domain>(pred, aRet, JSON_DOM);
}

PersistentStorage::Status PersistentStorageJson::Lookup(Network const &aValue, std::vector<Network> &aRet)
{
    std::function<bool(Network const &)> pred = [](Network const &) { return true; };

    pred = [aValue](Network const &el) {
        bool aRet = (aValue.mCcm < 0 || aValue.mCcm == el.mCcm) &&
                    (aValue.mId.mId == EMPTY_ID || (el.mId.mId == aValue.mId.mId)) &&
                    (aValue.mDomainId.mId == EMPTY_ID || (el.mDomainId.mId == aValue.mDomainId.mId)) &&
                    (aValue.mName.empty() || (aValue.mName == el.mName)) &&
                    (aValue.mXpan.mValue == 0 || aValue.mXpan == el.mXpan) &&
                    (aValue.mPan.empty() || CaseInsensitiveEqual(aValue.mPan, el.mPan)) &&
                    (aValue.mMlp.empty() || CaseInsensitiveEqual(aValue.mMlp, el.mMlp)) &&
                    (aValue.mChannel == 0 || (aValue.mChannel == el.mChannel));

        return aRet;
    };

    return LookupPred<Network>(pred, aRet, JSON_NWK);
}

PersistentStorage::Status PersistentStorageJson::Lookup(BorderRouter const &aValue, std::vector<BorderRouter> &aRet)
{
    std::function<bool(BorderRouter const &)> pred = [](BorderRouter const &) { return true; };

    pred = [aValue](BorderRouter const &el) {
        bool aRet =
            (aValue.mId.mId == EMPTY_ID || (el.mId.mId == aValue.mId.mId)) &&
            (aValue.mNetworkId.mId == EMPTY_ID || (el.mNetworkId.mId == aValue.mNetworkId.mId)) &&
            ((aValue.mAgent.mPresentFlags & BorderAgent::kAddrBit) == 0 ||
             ((el.mAgent.mPresentFlags & BorderAgent::kAddrBit) != 0 &&
              CaseInsensitiveEqual(el.mAgent.mAddr, aValue.mAgent.mAddr))) &&
            ((aValue.mAgent.mPresentFlags & BorderAgent::kPortBit) == 0 ||
             ((el.mAgent.mPresentFlags & BorderAgent::kPortBit) != 0 && el.mAgent.mPort == aValue.mAgent.mPort)) &&
            ((aValue.mAgent.mPresentFlags & BorderAgent::kThreadVersionBit) == 0 ||
             ((el.mAgent.mPresentFlags & BorderAgent::kThreadVersionBit) != 0 &&
              aValue.mAgent.mThreadVersion == el.mAgent.mThreadVersion)) &&
            ((aValue.mAgent.mPresentFlags & BorderAgent::kStateBit) == 0 ||
             ((el.mAgent.mPresentFlags & BorderAgent::kStateBit) != 0 && el.mAgent.mState == aValue.mAgent.mState)) &&
            ((aValue.mAgent.mPresentFlags & BorderAgent::kVendorNameBit) == 0 ||
             ((el.mAgent.mPresentFlags & BorderAgent::kVendorNameBit) != 0 &&
              CaseInsensitiveEqual(el.mAgent.mVendorName, aValue.mAgent.mVendorName))) &&
            ((aValue.mAgent.mPresentFlags & BorderAgent::kModelNameBit) == 0 ||
             ((el.mAgent.mPresentFlags & BorderAgent::kModelNameBit) != 0 &&
              CaseInsensitiveEqual(el.mAgent.mModelName, aValue.mAgent.mModelName))) &&
            ((aValue.mAgent.mPresentFlags & BorderAgent::kActiveTimestampBit) == 0 ||
             ((el.mAgent.mPresentFlags & BorderAgent::kActiveTimestampBit) != 0 &&
              el.mAgent.mActiveTimestamp.Encode() == aValue.mAgent.mActiveTimestamp.Encode())) &&
            ((aValue.mAgent.mPresentFlags & BorderAgent::kPartitionIdBit) == 0 ||
             ((el.mAgent.mPresentFlags & BorderAgent::kPartitionIdBit) != 0 &&
              el.mAgent.mPartitionId == aValue.mAgent.mPartitionId)) &&
            ((aValue.mAgent.mPresentFlags & BorderAgent::kVendorDataBit) == 0 ||
             ((el.mAgent.mPresentFlags & BorderAgent::kVendorDataBit) != 0 &&
              el.mAgent.mVendorData == aValue.mAgent.mVendorData)) &&
            ((aValue.mAgent.mPresentFlags & BorderAgent::kVendorOuiBit) == 0 ||
             ((el.mAgent.mPresentFlags & BorderAgent::kVendorOuiBit) != 0 &&
              el.mAgent.mVendorOui == aValue.mAgent.mVendorOui)) &&
            ((aValue.mAgent.mPresentFlags & BorderAgent::kBbrSeqNumberBit) == 0 ||
             ((el.mAgent.mPresentFlags & BorderAgent::kBbrSeqNumberBit) != 0 &&
              el.mAgent.mBbrSeqNumber == aValue.mAgent.mBbrSeqNumber)) &&
            ((aValue.mAgent.mPresentFlags & BorderAgent::kBbrPortBit) == 0 ||
             ((el.mAgent.mPresentFlags & BorderAgent::kBbrPortBit) != 0 &&
              el.mAgent.mBbrPort == aValue.mAgent.mBbrPort));
        return aRet;
    };

    return LookupPred<BorderRouter>(pred, aRet, JSON_BR);
}

PersistentStorage::Status PersistentStorageJson::LookupAny(Registrar const &aValue, std::vector<Registrar> &aRet)
{
    std::function<bool(Registrar const &)> pred = [](Registrar const &) { return true; };

    pred = [aValue](Registrar const &el) {
        bool aRet = (aValue.mId.mId == EMPTY_ID || (el.mId.mId == aValue.mId.mId)) ||
                    (aValue.mAddr.empty() || CaseInsensitiveEqual(aValue.mAddr, el.mAddr)) ||
                    (aValue.mPort == 0 || (aValue.mPort == el.mPort));

        if (!aValue.mDomains.empty())
        {
            std::vector<std::string> tmpElement(el.mDomains);
            std::vector<std::string> tmpValue(aValue.mDomains);
            std::sort(std::begin(tmpElement), std::end(tmpElement));
            std::sort(std::begin(tmpValue), std::end(tmpValue));
            aRet = aRet || std::includes(std::begin(tmpElement), std::end(tmpElement), std::begin(tmpValue),
                                         std::end(tmpValue));
        }
        return aRet;
    };

    return LookupPred<Registrar>(pred, aRet, JSON_RGR);
}

PersistentStorage::Status PersistentStorageJson::LookupAny(Domain const &aValue, std::vector<Domain> &aRet)
{
    std::function<bool(Domain const &)> pred = [](Domain const &) { return true; };

    pred = [aValue](Domain const &el) {
        bool aRet = (aValue.mId.mId == EMPTY_ID || (el.mId.mId == aValue.mId.mId)) ||
                    (aValue.mName.empty() || (aValue.mName == el.mName));
        return aRet;
    };

    return LookupPred<Domain>(pred, aRet, JSON_DOM);
}

PersistentStorage::Status PersistentStorageJson::LookupAny(Network const &aValue, std::vector<Network> &aRet)
{
    std::function<bool(Network const &)> pred = [](Network const &) { return true; };

    pred = [aValue](Network const &el) {
        bool aRet = (aValue.mCcm < 0 || aValue.mCcm == el.mCcm) ||
                    (aValue.mId.mId == EMPTY_ID || (el.mId.mId == aValue.mId.mId)) ||
                    (aValue.mDomainId.mId == EMPTY_ID || (el.mDomainId.mId == aValue.mDomainId.mId)) ||
                    (aValue.mName.empty() || (aValue.mName == el.mName)) ||
                    (aValue.mXpan.mValue == 0 || aValue.mXpan == el.mXpan) ||
                    (aValue.mPan.empty() || CaseInsensitiveEqual(aValue.mPan, el.mPan)) ||
                    (aValue.mMlp.empty() || CaseInsensitiveEqual(aValue.mMlp, el.mMlp)) ||
                    (aValue.mChannel == 0 || (aValue.mChannel == el.mChannel));

        return aRet;
    };

    return LookupPred<Network>(pred, aRet, JSON_NWK);
}

PersistentStorage::Status PersistentStorageJson::LookupAny(BorderRouter const &aValue, std::vector<BorderRouter> &aRet)
{
    std::function<bool(BorderRouter const &)> pred = [](BorderRouter const &) { return true; };

    pred = [aValue](BorderRouter const &el) {
        bool aRet =
            (aValue.mId.mId == EMPTY_ID || (el.mId.mId == aValue.mId.mId)) ||
            (aValue.mNetworkId.mId == EMPTY_ID || (el.mNetworkId.mId == aValue.mNetworkId.mId)) ||
            ((aValue.mAgent.mPresentFlags & BorderAgent::kAddrBit) == 0 ||
             ((el.mAgent.mPresentFlags & BorderAgent::kAddrBit) != 0 ||
              CaseInsensitiveEqual(el.mAgent.mAddr, aValue.mAgent.mAddr))) ||
            ((aValue.mAgent.mPresentFlags & BorderAgent::kPortBit) == 0 ||
             ((el.mAgent.mPresentFlags & BorderAgent::kPortBit) != 0 || el.mAgent.mPort == aValue.mAgent.mPort)) ||
            ((aValue.mAgent.mPresentFlags & BorderAgent::kThreadVersionBit) == 0 ||
             ((el.mAgent.mPresentFlags & BorderAgent::kThreadVersionBit) != 0 ||
              aValue.mAgent.mThreadVersion == el.mAgent.mThreadVersion)) ||
            ((aValue.mAgent.mPresentFlags & BorderAgent::kStateBit) == 0 ||
             ((el.mAgent.mPresentFlags & BorderAgent::kStateBit) != 0 || el.mAgent.mState == aValue.mAgent.mState)) ||
            ((aValue.mAgent.mPresentFlags & BorderAgent::kVendorNameBit) == 0 ||
             ((el.mAgent.mPresentFlags & BorderAgent::kVendorNameBit) != 0 ||
              CaseInsensitiveEqual(el.mAgent.mVendorName, aValue.mAgent.mVendorName))) ||
            ((aValue.mAgent.mPresentFlags & BorderAgent::kModelNameBit) == 0 ||
             ((el.mAgent.mPresentFlags & BorderAgent::kModelNameBit) != 0 ||
              CaseInsensitiveEqual(el.mAgent.mModelName, aValue.mAgent.mModelName))) ||
            ((aValue.mAgent.mPresentFlags & BorderAgent::kActiveTimestampBit) == 0 ||
             ((el.mAgent.mPresentFlags & BorderAgent::kActiveTimestampBit) != 0 ||
              el.mAgent.mActiveTimestamp.Encode() == aValue.mAgent.mActiveTimestamp.Encode())) ||
            ((aValue.mAgent.mPresentFlags & BorderAgent::kPartitionIdBit) == 0 ||
             ((el.mAgent.mPresentFlags & BorderAgent::kPartitionIdBit) != 0 ||
              el.mAgent.mPartitionId == aValue.mAgent.mPartitionId)) ||
            ((aValue.mAgent.mPresentFlags & BorderAgent::kVendorDataBit) == 0 ||
             ((el.mAgent.mPresentFlags & BorderAgent::kVendorDataBit) != 0 ||
              el.mAgent.mVendorData == aValue.mAgent.mVendorData)) ||
            ((aValue.mAgent.mPresentFlags & BorderAgent::kVendorOuiBit) == 0 ||
             ((el.mAgent.mPresentFlags & BorderAgent::kVendorOuiBit) != 0 ||
              el.mAgent.mVendorOui == aValue.mAgent.mVendorOui)) ||
            ((aValue.mAgent.mPresentFlags & BorderAgent::kBbrSeqNumberBit) == 0 ||
             ((el.mAgent.mPresentFlags & BorderAgent::kBbrSeqNumberBit) != 0 ||
              el.mAgent.mBbrSeqNumber == aValue.mAgent.mBbrSeqNumber)) ||
            ((aValue.mAgent.mPresentFlags & BorderAgent::kBbrPortBit) == 0 ||
             ((el.mAgent.mPresentFlags & BorderAgent::kBbrPortBit) != 0 ||
              el.mAgent.mBbrPort == aValue.mAgent.mBbrPort));
        return aRet;
    };

    return LookupPred<BorderRouter>(pred, aRet, JSON_BR);
}

PersistentStorage::Status PersistentStorageJson::CurrentNetworkSet(const NetworkId &aNwkId)
{
    if (CacheFromFile() != PersistentStorage::Status::PS_SUCCESS)
    {
        return PersistentStorage::Status::PS_ERROR;
    }

    mCache[JSON_CURR_NWK] = aNwkId;

    return CacheToFile();
}

PersistentStorage::Status PersistentStorageJson::CurrentNetworkGet(NetworkId &aNwkId)
{
    if (!mCache.contains(JSON_CURR_NWK))
    {
        aNwkId                = NetworkId{};
        mCache[JSON_CURR_NWK] = aNwkId;
    }
    else
    {
        aNwkId = mCache[JSON_CURR_NWK];
    }
    return PersistentStorage::Status::PS_SUCCESS;
}

} // namespace persistent_storage
} // namespace commissioner
} // namespace ot
