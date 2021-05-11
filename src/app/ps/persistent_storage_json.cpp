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

PersistentStorageJson::PersistentStorageJson(std::string const &fname)
    : file_name(fname)
    , cache()
    , storage_lock()
{
    semaphore_open("thrcomm_json_storage", storage_lock);
}

PersistentStorageJson::~PersistentStorageJson()
{
    Close();
    cache.clear();
    semaphore_close(storage_lock);
}

json PersistentStorageJson::JsonDefault()
{
    return json{
        {JSON_RGR_SEQ, registrar_id(0)},       {JSON_DOM_SEQ, domain_id(0)},
        {JSON_NWK_SEQ, network_id(0)},         {JSON_BR_SEQ, border_router_id(0)},

        {JSON_RGR, std::vector<registrar>{}},  {JSON_DOM, std::vector<domain>{}},
        {JSON_NWK, std::vector<network>{}},    {JSON_BR, std::vector<border_router>{}},

        {JSON_CURR_NWK, network_id{EMPTY_ID}},
    };
}

bool PersistentStorageJson::CacheStructValidation()
{
    json base = JsonDefault();

    for (auto &el : base.items())
    {
        auto to_check = cache.find(el.key());
        if (to_check == cache.end() || to_check->type_name() != el.value().type_name())
        {
            return false;
        }
    }

    return true;
}

PersistentStorage::Status PersistentStorageJson::CacheFromFile()
{
    if (file_name.empty())
    {
        // No persistence, nothing to do
        return PersistentStorage::Status::PS_SUCCESS;
    }

    if (semaphore_wait(storage_lock) != ot::os::sem::sem_status::SEM_SUCCESS)
    {
        return PersistentStorage::Status::PS_ERROR;
    }

    if (RestoreFilePath(file_name).GetCode() != ErrorCode::kNone)
    {
        semaphore_post(storage_lock);
        return PersistentStorage::Status::PS_ERROR;
    }

    std::string fdata;
    Error       error = ReadFile(fdata, file_name);
    if (error.GetCode() != ErrorCode::kNone)
    {
        semaphore_post(storage_lock);
        return PersistentStorage::Status::PS_ERROR;
    }

    if (fdata.size() == 0)
    {
        cache = JsonDefault();

        semaphore_post(storage_lock);
        return PersistentStorage::Status::PS_SUCCESS;
    }

    try
    {
        cache = json::parse(fdata);
    } catch (json::parse_error const &)
    {
        semaphore_post(storage_lock);
        return PersistentStorage::Status::PS_ERROR;
    }

    if (cache.empty())
    {
        cache = JsonDefault();
    }

    // base validation
    if (!CacheStructValidation())
    {
        semaphore_post(storage_lock);
        return PersistentStorage::Status::PS_ERROR;
    }

    semaphore_post(storage_lock);
    return PersistentStorage::Status::PS_SUCCESS;
}

PersistentStorage::Status PersistentStorageJson::CacheToFile()
{
    if (file_name.empty())
    {
        // No persistence, nothing to do
        return PersistentStorage::Status::PS_SUCCESS;
    }

    if (semaphore_wait(storage_lock) != ot::os::sem::sem_status::SEM_SUCCESS)
    {
        return PersistentStorage::Status::PS_ERROR;
    }

    Error error = WriteFile(cache.dump(4), file_name);
    if (error.GetCode() != ErrorCode::kNone)
    {
        semaphore_post(storage_lock);
        return PersistentStorage::Status::PS_ERROR;
    }

    semaphore_post(storage_lock);
    return PersistentStorage::Status::PS_SUCCESS;
}

PersistentStorage::Status PersistentStorageJson::Open()
{
    if (file_name.empty())
    {
        // No persistence, use default contents
        cache = JsonDefault();
    }
    CacheFromFile();
    return CacheToFile();
}

PersistentStorage::Status PersistentStorageJson::Close()
{
    return CacheToFile();
}

PersistentStorage::Status PersistentStorageJson::Add(registrar const &val, registrar_id &ret_id)
{
    return AddOne<registrar, registrar_id>(val, ret_id, JSON_RGR_SEQ, JSON_RGR);
}

PersistentStorage::Status PersistentStorageJson::Add(domain const &val, domain_id &ret_id)
{
    return AddOne<domain, domain_id>(val, ret_id, JSON_DOM_SEQ, JSON_DOM);
}

PersistentStorage::Status PersistentStorageJson::Add(network const &val, network_id &ret_id)
{
    return AddOne<network, network_id>(val, ret_id, JSON_NWK_SEQ, JSON_NWK);
}

PersistentStorage::Status PersistentStorageJson::Add(border_router const &val, border_router_id &ret_id)
{
    return AddOne<border_router, border_router_id>(val, ret_id, JSON_BR_SEQ, JSON_BR);
}

PersistentStorage::Status PersistentStorageJson::Del(registrar_id const &id)
{
    return DelId<registrar, registrar_id>(id, JSON_RGR);
}

PersistentStorage::Status PersistentStorageJson::Del(domain_id const &id)
{
    return DelId<domain, domain_id>(id, JSON_DOM);
}

PersistentStorage::Status PersistentStorageJson::Del(network_id const &id)
{
    return DelId<network, network_id>(id, JSON_NWK);
}

PersistentStorage::Status PersistentStorageJson::Del(border_router_id const &id)
{
    return DelId<border_router, border_router_id>(id, JSON_BR);
}

PersistentStorage::Status PersistentStorageJson::Get(registrar_id const &id, registrar &ret_val)
{
    return GetId<registrar, registrar_id>(id, ret_val, JSON_RGR);
}

PersistentStorage::Status PersistentStorageJson::Get(domain_id const &id, domain &ret_val)
{
    return GetId<domain, domain_id>(id, ret_val, JSON_DOM);
}

PersistentStorage::Status PersistentStorageJson::Get(network_id const &id, network &ret_val)
{
    return GetId<network, network_id>(id, ret_val, JSON_NWK);
}

PersistentStorage::Status PersistentStorageJson::Get(border_router_id const &id, border_router &ret_val)
{
    return GetId<border_router, border_router_id>(id, ret_val, JSON_BR);
}

PersistentStorage::Status PersistentStorageJson::Update(registrar const &val)
{
    return UpdId<registrar>(val, JSON_RGR);
}

PersistentStorage::Status PersistentStorageJson::Update(domain const &val)
{
    return UpdId<domain>(val, JSON_DOM);
}

PersistentStorage::Status PersistentStorageJson::Update(network const &val)
{
    return UpdId<network>(val, JSON_NWK);
}

PersistentStorage::Status PersistentStorageJson::Update(border_router const &val)
{
    return UpdId<border_router>(val, JSON_BR);
}

PersistentStorage::Status PersistentStorageJson::Lookup(registrar const &val, std::vector<registrar> &ret)
{
    std::function<bool(registrar const &)> pred = [](registrar const &) { return true; };

    pred = [val](registrar const &el) {
        bool ret = (val.id.id == EMPTY_ID || (el.id.id == val.id.id)) &&
                   (val.addr.empty() || CaseInsensitiveEqual(val.addr, el.addr)) &&
                   (val.port == 0 || (val.port == el.port));

        if (ret && !val.domains.empty())
        {
            std::vector<std::string> el_tmp(el.domains);
            std::vector<std::string> val_tmp(val.domains);
            std::sort(std::begin(el_tmp), std::end(el_tmp));
            std::sort(std::begin(val_tmp), std::end(val_tmp));
            ret = std::includes(std::begin(el_tmp), std::end(el_tmp), std::begin(val_tmp), std::end(val_tmp));
        }
        return ret;
    };

    return LookupPred<registrar>(pred, ret, JSON_RGR);
}

PersistentStorage::Status PersistentStorageJson::Lookup(domain const &val, std::vector<domain> &ret)
{
    std::function<bool(domain const &)> pred = [](domain const &) { return true; };

    pred = [val](domain const &el) {
        bool ret = (val.id.id == EMPTY_ID || (el.id.id == val.id.id)) && (val.name.empty() || (val.name == el.name));
        return ret;
    };

    return LookupPred<domain>(pred, ret, JSON_DOM);
}

PersistentStorage::Status PersistentStorageJson::Lookup(network const &val, std::vector<network> &ret)
{
    std::function<bool(network const &)> pred = [](network const &) { return true; };

    pred = [val](network const &el) {
        bool ret = (val.ccm < 0 || val.ccm == el.ccm) && (val.id.id == EMPTY_ID || (el.id.id == val.id.id)) &&
                   (val.dom_id.id == EMPTY_ID || (el.dom_id.id == val.dom_id.id)) &&
                   (val.name.empty() || (val.name == el.name)) && (val.xpan.value == 0 || val.xpan == el.xpan) &&
                   (val.pan.empty() || CaseInsensitiveEqual(val.pan, el.pan)) &&
                   (val.mlp.empty() || CaseInsensitiveEqual(val.mlp, el.mlp)) &&
                   (val.channel == 0 || (val.channel == el.channel));

        return ret;
    };

    return LookupPred<network>(pred, ret, JSON_NWK);
}

PersistentStorage::Status PersistentStorageJson::Lookup(border_router const &val, std::vector<border_router> &ret)
{
    std::function<bool(border_router const &)> pred = [](border_router const &) { return true; };

    pred = [val](border_router const &el) {
        bool ret =
            (val.id.id == EMPTY_ID || (el.id.id == val.id.id)) &&
            (val.nwk_id.id == EMPTY_ID || (el.nwk_id.id == val.nwk_id.id)) &&
            ((val.agent.mPresentFlags & BorderAgent::kAddrBit) == 0 ||
             ((el.agent.mPresentFlags & BorderAgent::kAddrBit) != 0 &&
              CaseInsensitiveEqual(el.agent.mAddr, val.agent.mAddr))) &&
            ((val.agent.mPresentFlags & BorderAgent::kPortBit) == 0 ||
             ((el.agent.mPresentFlags & BorderAgent::kPortBit) != 0 && el.agent.mPort == val.agent.mPort)) &&
            ((val.agent.mPresentFlags & BorderAgent::kThreadVersionBit) == 0 ||
             ((el.agent.mPresentFlags & BorderAgent::kThreadVersionBit) != 0 &&
              val.agent.mThreadVersion == el.agent.mThreadVersion)) &&
            ((val.agent.mPresentFlags & BorderAgent::kStateBit) == 0 ||
             ((el.agent.mPresentFlags & BorderAgent::kStateBit) != 0 && el.agent.mState == val.agent.mState)) &&
            ((val.agent.mPresentFlags & BorderAgent::kVendorNameBit) == 0 ||
             ((el.agent.mPresentFlags & BorderAgent::kVendorNameBit) != 0 &&
              CaseInsensitiveEqual(el.agent.mVendorName, val.agent.mVendorName))) &&
            ((val.agent.mPresentFlags & BorderAgent::kModelNameBit) == 0 ||
             ((el.agent.mPresentFlags & BorderAgent::kModelNameBit) != 0 &&
              CaseInsensitiveEqual(el.agent.mModelName, val.agent.mModelName))) &&
            ((val.agent.mPresentFlags & BorderAgent::kActiveTimestampBit) == 0 ||
             ((el.agent.mPresentFlags & BorderAgent::kActiveTimestampBit) != 0 &&
              el.agent.mActiveTimestamp.Encode() == val.agent.mActiveTimestamp.Encode())) &&
            ((val.agent.mPresentFlags & BorderAgent::kPartitionIdBit) == 0 ||
             ((el.agent.mPresentFlags & BorderAgent::kPartitionIdBit) != 0 &&
              el.agent.mPartitionId == val.agent.mPartitionId)) &&
            ((val.agent.mPresentFlags & BorderAgent::kVendorDataBit) == 0 ||
             ((el.agent.mPresentFlags & BorderAgent::kVendorDataBit) != 0 &&
              el.agent.mVendorData == val.agent.mVendorData)) &&
            ((val.agent.mPresentFlags & BorderAgent::kVendorOuiBit) == 0 ||
             ((el.agent.mPresentFlags & BorderAgent::kVendorOuiBit) != 0 &&
              el.agent.mVendorOui == val.agent.mVendorOui)) &&
            ((val.agent.mPresentFlags & BorderAgent::kBbrSeqNumberBit) == 0 ||
             ((el.agent.mPresentFlags & BorderAgent::kBbrSeqNumberBit) != 0 &&
              el.agent.mBbrSeqNumber == val.agent.mBbrSeqNumber)) &&
            ((val.agent.mPresentFlags & BorderAgent::kBbrPortBit) == 0 ||
             ((el.agent.mPresentFlags & BorderAgent::kBbrPortBit) != 0 && el.agent.mBbrPort == val.agent.mBbrPort));
        return ret;
    };

    return LookupPred<border_router>(pred, ret, JSON_BR);
}

PersistentStorage::Status PersistentStorageJson::LookupAny(registrar const &val, std::vector<registrar> &ret)
{
    std::function<bool(registrar const &)> pred = [](registrar const &) { return true; };

    pred = [val](registrar const &el) {
        bool ret = (val.id.id == EMPTY_ID || (el.id.id == val.id.id)) ||
                   (val.addr.empty() || CaseInsensitiveEqual(val.addr, el.addr)) ||
                   (val.port == 0 || (val.port == el.port));

        if (!val.domains.empty())
        {
            std::vector<std::string> el_tmp(el.domains);
            std::vector<std::string> val_tmp(val.domains);
            std::sort(std::begin(el_tmp), std::end(el_tmp));
            std::sort(std::begin(val_tmp), std::end(val_tmp));
            ret = ret || std::includes(std::begin(el_tmp), std::end(el_tmp), std::begin(val_tmp), std::end(val_tmp));
        }
        return ret;
    };

    return LookupPred<registrar>(pred, ret, JSON_RGR);
}

PersistentStorage::Status PersistentStorageJson::LookupAny(domain const &val, std::vector<domain> &ret)
{
    std::function<bool(domain const &)> pred = [](domain const &) { return true; };

    pred = [val](domain const &el) {
        bool ret = (val.id.id == EMPTY_ID || (el.id.id == val.id.id)) || (val.name.empty() || (val.name == el.name));
        return ret;
    };

    return LookupPred<domain>(pred, ret, JSON_DOM);
}

PersistentStorage::Status PersistentStorageJson::LookupAny(network const &val, std::vector<network> &ret)
{
    std::function<bool(network const &)> pred = [](network const &) { return true; };

    pred = [val](network const &el) {
        bool ret = (val.ccm < 0 || val.ccm == el.ccm) || (val.id.id == EMPTY_ID || (el.id.id == val.id.id)) ||
                   (val.dom_id.id == EMPTY_ID || (el.dom_id.id == val.dom_id.id)) ||
                   (val.name.empty() || (val.name == el.name)) || (val.xpan.value == 0 || val.xpan == el.xpan) ||
                   (val.pan.empty() || CaseInsensitiveEqual(val.pan, el.pan)) ||
                   (val.mlp.empty() || CaseInsensitiveEqual(val.mlp, el.mlp)) ||
                   (val.channel == 0 || (val.channel == el.channel));

        return ret;
    };

    return LookupPred<network>(pred, ret, JSON_NWK);
}

PersistentStorage::Status PersistentStorageJson::LookupAny(border_router const &val, std::vector<border_router> &ret)
{
    std::function<bool(border_router const &)> pred = [](border_router const &) { return true; };

    pred = [val](border_router const &el) {
        bool ret =
            (val.id.id == EMPTY_ID || (el.id.id == val.id.id)) ||
            (val.nwk_id.id == EMPTY_ID || (el.nwk_id.id == val.nwk_id.id)) ||
            ((val.agent.mPresentFlags & BorderAgent::kAddrBit) == 0 ||
             ((el.agent.mPresentFlags & BorderAgent::kAddrBit) != 0 ||
              CaseInsensitiveEqual(el.agent.mAddr, val.agent.mAddr))) ||
            ((val.agent.mPresentFlags & BorderAgent::kPortBit) == 0 ||
             ((el.agent.mPresentFlags & BorderAgent::kPortBit) != 0 || el.agent.mPort == val.agent.mPort)) ||
            ((val.agent.mPresentFlags & BorderAgent::kThreadVersionBit) == 0 ||
             ((el.agent.mPresentFlags & BorderAgent::kThreadVersionBit) != 0 ||
              val.agent.mThreadVersion == el.agent.mThreadVersion)) ||
            ((val.agent.mPresentFlags & BorderAgent::kStateBit) == 0 ||
             ((el.agent.mPresentFlags & BorderAgent::kStateBit) != 0 || el.agent.mState == val.agent.mState)) ||
            ((val.agent.mPresentFlags & BorderAgent::kVendorNameBit) == 0 ||
             ((el.agent.mPresentFlags & BorderAgent::kVendorNameBit) != 0 ||
              CaseInsensitiveEqual(el.agent.mVendorName, val.agent.mVendorName))) ||
            ((val.agent.mPresentFlags & BorderAgent::kModelNameBit) == 0 ||
             ((el.agent.mPresentFlags & BorderAgent::kModelNameBit) != 0 ||
              CaseInsensitiveEqual(el.agent.mModelName, val.agent.mModelName))) ||
            ((val.agent.mPresentFlags & BorderAgent::kActiveTimestampBit) == 0 ||
             ((el.agent.mPresentFlags & BorderAgent::kActiveTimestampBit) != 0 ||
              el.agent.mActiveTimestamp.Encode() == val.agent.mActiveTimestamp.Encode())) ||
            ((val.agent.mPresentFlags & BorderAgent::kPartitionIdBit) == 0 ||
             ((el.agent.mPresentFlags & BorderAgent::kPartitionIdBit) != 0 ||
              el.agent.mPartitionId == val.agent.mPartitionId)) ||
            ((val.agent.mPresentFlags & BorderAgent::kVendorDataBit) == 0 ||
             ((el.agent.mPresentFlags & BorderAgent::kVendorDataBit) != 0 ||
              el.agent.mVendorData == val.agent.mVendorData)) ||
            ((val.agent.mPresentFlags & BorderAgent::kVendorOuiBit) == 0 ||
             ((el.agent.mPresentFlags & BorderAgent::kVendorOuiBit) != 0 ||
              el.agent.mVendorOui == val.agent.mVendorOui)) ||
            ((val.agent.mPresentFlags & BorderAgent::kBbrSeqNumberBit) == 0 ||
             ((el.agent.mPresentFlags & BorderAgent::kBbrSeqNumberBit) != 0 ||
              el.agent.mBbrSeqNumber == val.agent.mBbrSeqNumber)) ||
            ((val.agent.mPresentFlags & BorderAgent::kBbrPortBit) == 0 ||
             ((el.agent.mPresentFlags & BorderAgent::kBbrPortBit) != 0 || el.agent.mBbrPort == val.agent.mBbrPort));
        return ret;
    };

    return LookupPred<border_router>(pred, ret, JSON_BR);
}

PersistentStorage::Status PersistentStorageJson::CurrentNetworkSet(const network_id &nwk_id)
{
    if (CacheFromFile() != PersistentStorage::Status::PS_SUCCESS)
    {
        return PersistentStorage::Status::PS_ERROR;
    }

    cache[JSON_CURR_NWK] = nwk_id;

    return CacheToFile();
}

PersistentStorage::Status PersistentStorageJson::CurrentNetworkGet(network_id &nwk_id)
{
    if (!cache.contains(JSON_CURR_NWK))
    {
        nwk_id               = network_id{};
        cache[JSON_CURR_NWK] = nwk_id;
    }
    else
    {
        nwk_id = cache[JSON_CURR_NWK];
    }
    return PersistentStorage::Status::PS_SUCCESS;
}

} // namespace persistent_storage
} // namespace commissioner
} // namespace ot
