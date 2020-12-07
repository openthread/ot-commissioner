
#include "persistent_storage_json.hpp"
#include "file.hpp"
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

persistent_storage_json::persistent_storage_json(std::string const &fname)
    : file_name(fname)
    , cache()
    , storage_lock()
{
    semaphore_open("thrcomm_json_storage", storage_lock);
}

persistent_storage_json::~persistent_storage_json()
{
    close();
    cache.clear();
    semaphore_close(storage_lock);
}

json persistent_storage_json::json_default()
{
    return json{
        {JSON_RGR_SEQ, registrar_id(0)},       {JSON_DOM_SEQ, domain_id(0)},
        {JSON_NWK_SEQ, network_id(0)},         {JSON_BR_SEQ, border_router_id(0)},

        {JSON_RGR, std::vector<registrar>{}},  {JSON_DOM, std::vector<domain>{}},
        {JSON_NWK, std::vector<network>{}},    {JSON_BR, std::vector<border_router>{}},

        {JSON_CURR_NWK, network_id{EMPTY_ID}},
    };
}

bool persistent_storage_json::cache_struct_validation()
{
    json base = json_default();

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

ps_status persistent_storage_json::cache_from_file()
{
    if (semaphore_wait(storage_lock) != ot::os::sem::sem_status::SEM_SUCCESS)
    {
        return PS_ERROR;
    }

    std::string               fdata;
    ot::os::file::file_status fstat = ot::os::file::file_read(file_name, fdata);
    if (fstat != ot::os::file::file_status::FS_NOT_EXISTS && fstat != ot::os::file::file_status::FS_SUCCESS)
    {
        semaphore_post(storage_lock);
        return PS_ERROR;
    }

    if (fdata.size() == 0)
    {
        cache = json_default();

        semaphore_post(storage_lock);
        return PS_SUCCESS;
    }

    try
    {
        cache = json::parse(fdata);
    } catch (json::parse_error const &)
    {
        semaphore_post(storage_lock);
        return PS_ERROR;
    }

    if (cache.empty())
    {
        cache = json_default();
    }

    // base validation
    if (!cache_struct_validation())
    {
        semaphore_post(storage_lock);
        return PS_ERROR;
    }

    semaphore_post(storage_lock);
    return PS_SUCCESS;
}

ps_status persistent_storage_json::cache_to_file()
{
    if (semaphore_wait(storage_lock) != ot::os::sem::sem_status::SEM_SUCCESS)
    {
        return PS_ERROR;
    }

    ot::os::file::file_status fstat = ot::os::file::file_write(file_name, cache.dump(4));

    if (fstat != ot::os::file::file_status::FS_SUCCESS)
    {
        semaphore_post(storage_lock);
        return PS_ERROR;
    }

    semaphore_post(storage_lock);
    return PS_SUCCESS;
}

ps_status persistent_storage_json::open()
{
    cache_from_file();
    return cache_to_file();
}

ps_status persistent_storage_json::close()
{
    return cache_to_file();
}

ps_status persistent_storage_json::add(registrar const &val, registrar_id &ret_id)
{
    return add_one<registrar, registrar_id>(val, ret_id, JSON_RGR_SEQ, JSON_RGR);
}

ps_status persistent_storage_json::add(domain const &val, domain_id &ret_id)
{
    return add_one<domain, domain_id>(val, ret_id, JSON_DOM_SEQ, JSON_DOM);
}

ps_status persistent_storage_json::add(network const &val, network_id &ret_id)
{
    return add_one<network, network_id>(val, ret_id, JSON_NWK_SEQ, JSON_NWK);
}

ps_status persistent_storage_json::add(border_router const &val, border_router_id &ret_id)
{
    return add_one<border_router, border_router_id>(val, ret_id, JSON_BR_SEQ, JSON_BR);
}

ps_status persistent_storage_json::del(registrar_id const &id)
{
    return del_id<registrar, registrar_id>(id, JSON_RGR);
}

ps_status persistent_storage_json::del(domain_id const &id)
{
    return del_id<domain, domain_id>(id, JSON_DOM);
}

ps_status persistent_storage_json::del(network_id const &id)
{
    return del_id<network, network_id>(id, JSON_NWK);
}

ps_status persistent_storage_json::del(border_router_id const &id)
{
    return del_id<border_router, border_router_id>(id, JSON_BR);
}

ps_status persistent_storage_json::get(registrar_id const &id, registrar &ret_val)
{
    return get_id<registrar, registrar_id>(id, ret_val, JSON_RGR);
}

ps_status persistent_storage_json::get(domain_id const &id, domain &ret_val)
{
    return get_id<domain, domain_id>(id, ret_val, JSON_DOM);
}

ps_status persistent_storage_json::get(network_id const &id, network &ret_val)
{
    return get_id<network, network_id>(id, ret_val, JSON_NWK);
}

ps_status persistent_storage_json::get(border_router_id const &id, border_router &ret_val)
{
    return get_id<border_router, border_router_id>(id, ret_val, JSON_BR);
}

ps_status persistent_storage_json::update(registrar const &val)
{
    return upd_id<registrar>(val, JSON_RGR);
}

ps_status persistent_storage_json::update(domain const &val)
{
    return upd_id<domain>(val, JSON_DOM);
}

ps_status persistent_storage_json::update(network const &val)
{
    return upd_id<network>(val, JSON_NWK);
}

ps_status persistent_storage_json::update(border_router const &val)
{
    return upd_id<border_router>(val, JSON_BR);
}

ps_status persistent_storage_json::lookup(registrar const &val, std::vector<registrar> &ret)
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

    return lookup_pred<registrar>(pred, ret, JSON_RGR);
}

ps_status persistent_storage_json::lookup(domain const &val, std::vector<domain> &ret)
{
    std::function<bool(domain const &)> pred = [](domain const &) { return true; };

    pred = [val](domain const &el) {
        bool ret = (val.id.id == EMPTY_ID || (el.id.id == val.id.id)) && (val.name.empty() || (val.name == el.name));
        return ret;
    };

    return lookup_pred<domain>(pred, ret, JSON_DOM);
}

ps_status persistent_storage_json::lookup(network const &val, std::vector<network> &ret)
{
    std::function<bool(network const &)> pred = [](network const &) { return true; };

    pred = [val](network const &el) {
        bool ret = (val.ccm < 0 || val.ccm == el.ccm) && (val.id.id == EMPTY_ID || (el.id.id == val.id.id)) &&
                   (val.dom_id.id == EMPTY_ID || (el.dom_id.id == val.dom_id.id)) &&
                   (val.name.empty() || (val.name == el.name)) && (val.xpan == 0 || val.xpan == el.xpan) &&
                   (val.pan.empty() || CaseInsensitiveEqual(val.pan, el.pan)) &&
                   (val.mlp.empty() || CaseInsensitiveEqual(val.mlp, el.mlp)) &&
                   (val.channel == 0 || (val.channel == el.channel));

        return ret;
    };

    return lookup_pred<network>(pred, ret, JSON_NWK);
}

ps_status persistent_storage_json::lookup(border_router const &val, std::vector<border_router> &ret)
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

    return lookup_pred<border_router>(pred, ret, JSON_BR);
}

ps_status persistent_storage_json::lookup_any(registrar const &val, std::vector<registrar> &ret)
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

    return lookup_pred<registrar>(pred, ret, JSON_RGR);
}

ps_status persistent_storage_json::lookup(domain const &val, std::vector<domain> &ret)
{
    std::function<bool(domain const &)> pred = [](domain const &) { return true; };

    pred = [val](domain const &el) {
        bool ret = (val.id.id == EMPTY_ID || (el.id.id == val.id.id)) && (val.name.empty() || (val.name == el.name));
        return ret;
    };

    return lookup_pred<domain>(pred, ret, JSON_DOM);
}

ps_status persistent_storage_json::lookup(network const &val, std::vector<network> &ret)
{
    std::function<bool(network const &)> pred = [](network const &) { return true; };

    pred = [val](network const &el) {
        bool ret = (val.ccm < 0 || val.ccm == el.ccm) || (val.id.id == EMPTY_ID || (el.id.id == val.id.id)) ||
                   (val.dom_id.id == EMPTY_ID || (el.dom_id.id == val.dom_id.id)) ||
                   (val.name.empty() || (val.name == el.name)) || (val.xpan == 0 || val.xpan == el.xpan) ||
                   (val.pan.empty() || CaseInsensitiveEqual(val.pan, el.pan)) ||
                   (val.mlp.empty() || CaseInsensitiveEqual(val.mlp, el.mlp)) ||
                   (val.channel == 0 || (val.channel == el.channel));

        return ret;
    };

    return lookup_pred<network>(pred, ret, JSON_NWK);
}

ps_status persistent_storage_json::lookup_any(border_router const &val, std::vector<border_router> &ret)
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

    return lookup_pred<border_router>(pred, ret, JSON_BR);
}

ps_status persistent_storage_json::current_network_set(const network_id &nwk_id)
{
    if (cache_from_file() != PS_SUCCESS)
    {
        return PS_ERROR;
    }

    cache[JSON_CURR_NWK] = nwk_id;

    return cache_to_file();
}

ps_status persistent_storage_json::current_network_get(network_id &nwk_id)
{
    if (!cache.contains(JSON_CURR_NWK))
    {
        return PS_ERROR;
    }

    cache[JSON_CURR_NWK] = nwk_id;

    return cache_to_file();
}

ps_status persistent_storage_json::current_network_get(network_id &nwk_id)
{
    if (!cache.contains(JSON_CURR_NWK))
    {
        return PS_ERROR;
    }

    nwk_id = cache[JSON_CURR_NWK];

    return PS_SUCCESS;
}

} // namespace persistent_storage
} // namespace commissioner
} // namespace ot
