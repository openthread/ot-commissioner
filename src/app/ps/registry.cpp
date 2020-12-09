#include "registry.hpp"
#include "persistent_storage_json.hpp"
#include "common/utils.hpp"

#include <cassert>
#include <sstream>

namespace ot {
namespace commissioner {
namespace persistent_storage {

namespace {
registry_status success_status(ps_status ps_st)
{
    if (ps_st == ps_status::PS_SUCCESS)
    {
        return REG_SUCCESS;
    }
    return REG_ERROR;
}

registry_status map_status(ps_status ps_st)
{
    if (ps_st == ps_status::PS_ERROR)
    {
        return REG_ERROR;
    }
    if (ps_st == ps_status::PS_SUCCESS)
    {
        return REG_SUCCESS;
    }
    if (ps_st == ps_status::PS_NOT_FOUND)
    {
        return REG_NOT_FOUND;
    }
    return REG_ERROR;
}

const std::string ALIAS_THIS{"this"};
const std::string ALIAS_ALL{"all"};
const std::string ALIAS_OTHERS{"others"};

} // namespace

registry::registry(persistent_storage *strg)
    : manage_storage(false)
    , storage(strg)
{
    assert(storage != nullptr);
}

registry::registry(std::string const &name)
    : manage_storage(true)
{
    storage = new persistent_storage_json(name);
}

registry::~registry()
{
    close();

    if (manage_storage)
    {
        delete storage;
    }
    storage = nullptr;
}

registry_status registry::open()
{
    if (storage == nullptr)
    {
        return REG_ERROR;
    }

    return success_status(storage->open());
}

registry_status registry::close()
{
    if (storage == nullptr)
    {
        return REG_ERROR;
    }
    return success_status(storage->close());
}

registry_status registry::add(BorderAgent const &val)
{
    domain          dom{};
    bool            domain_created = false;
    registry_status status;

    if ((val.mPresentFlags & BorderAgent::kDomainNameBit) != 0)
    {
        dom.name = val.mDomainName;
        std::vector<domain> domains;
        status = map_status(storage->lookup(dom, domains));
        if (status == REG_ERROR || domains.size() > 1)
        {
            return REG_ERROR;
        }
        else if (status == REG_NOT_FOUND)
        {
            domain_id dom_id{EMPTY_ID};
            status = map_status(storage->add(dom, dom_id));
            if (status != REG_SUCCESS)
            {
                return status;
            }
            dom.id         = dom_id;
            domain_created = true;
        }
        else
        {
            dom = domains[0];
        }
    }

    network nwk{};
    bool    network_created = false;

    try
    {
        if ((val.mPresentFlags & BorderAgent::kNetworkNameBit) != 0 ||
            (val.mPresentFlags & BorderAgent::kExtendedPanIdBit) != 0)
        {
            // If xpan present lookup with it only
            // Discussed: is different name an error?
            // Decided: update network name in the network entity
            if ((val.mPresentFlags & BorderAgent::kExtendedPanIdBit) != 0)
            {
                nwk.xpan = val.mExtendedPanId;
            }
            else
            {
                nwk.name = val.mNetworkName;
            }

            std::vector<network> nwks;
            status = map_status(storage->lookup(nwk, nwks));
            if (status == REG_ERROR || nwks.size() > 1)
            {
                throw status;
            }
            else if (status == REG_NOT_FOUND)
            {
                network_id nwk_id{EMPTY_ID};
                // It is possible we found the network by xpan
                if ((val.mPresentFlags & BorderAgent::kExtendedPanIdBit) != 0 &&
                    (val.mPresentFlags & BorderAgent::kNetworkNameBit) != 0)
                {
                    nwk.name = val.mNetworkName;
                }
                nwk.dom_id = dom.id;
                status     = map_status(storage->add(nwk, nwk_id));
                if (status != REG_SUCCESS)
                {
                    throw status;
                }
                nwk.id          = nwk_id;
                network_created = true;
            }
            else
            {
                nwk = nwks[0];
                if (nwk.dom_id.id != dom.id.id || nwk.name != val.mNetworkName)
                {
                    nwk.dom_id.id = dom.id.id;
                    nwk.name      = val.mNetworkName;
                    status        = map_status(storage->update(nwk));
                    if (status != REG_SUCCESS)
                    {
                        throw status;
                    }
                }
            }
        }

        border_router br{EMPTY_ID, nwk.id, val};
        try
        {
            if (br.nwk_id.id == EMPTY_ID)
            {
                throw REG_ERROR;
            }

            // TODO [MP] Fix: BorderAgent key is xpan + addr
            // Lookup border_router by address and port to decide to add() or update()
            // Assuming address and port are set (it should be so).
            border_router lookup_br{};
            lookup_br.agent.mAddr         = val.mAddr;
            lookup_br.agent.mPort         = val.mPort;
            lookup_br.agent.mPresentFlags = BorderAgent::kAddrBit | BorderAgent::kPortBit;
            std::vector<border_router> routers;
            status = map_status(storage->lookup(lookup_br, routers));
            if (status == REG_SUCCESS && routers.size() == 1)
            {
                br.id.id = routers[0].id.id;
                status   = map_status(storage->update(br));
            }
            else if (status == REG_NOT_FOUND)
            {
                status = map_status(storage->add(br, br.id));
            }

            if (status != REG_SUCCESS)
            {
                throw status;
            }
        } catch (registry_status thrown_status)
        {
            if (network_created)
            {
                map_status(storage->del(nwk.id));
            }
            throw;
        }
    } catch (registry_status thrown_status)
    {
        if (domain_created)
        {
            map_status(storage->del(dom.id));
        }
        status = thrown_status;
    }
    return status;
}

registry_status registry::get_networks_in_domain(const std::string &dom_name, NetworkArray &ret)
{
    registry_status     status;
    std::vector<domain> domains;
    domain              dom{EMPTY_ID, dom_name};
    network             nwk{};

    VerifyOrExit((status = map_status(storage->lookup(dom, domains))) == REG_SUCCESS);
    VerifyOrExit(domains.size() < 2, status = REG_ERROR);

    nwk.dom_id = domains[0].id;
    status     = map_status(storage->lookup(nwk, ret));

exit:
    return status;
}

registry_status registry::get_all_networks(NetworkArray &ret)
{
    registry_status status;

    status = map_status(storage->lookup(network{}, ret));

    return status;
}

registry_status registry::get_networks_by_aliases(const StringArray &aliases, NetworkArray &ret)
{
    registry_status status;

    NetworkArray networks;
    for (auto alias : aliases)
    {
        if (alias == ALIAS_ALL || alias == ALIAS_OTHERS)
        {
            VerifyOrExit((status = get_all_networks(networks)) == REG_SUCCESS);
            if (alias == ALIAS_OTHERS)
            {
                network nwk_this;
                VerifyOrExit((status = get_current_network(nwk_this)));
                auto nwk_iter = find_if(networks.begin(), networks.end(),
                                        [&nwk_this](const network &el) { return nwk_this.id.id == el.id.id; });
                if (nwk_iter != networks.end())
                {
                    networks.erase(nwk_iter);
                }
            }
        }
        else if (alias == ALIAS_THIS)
        {
            // Get selected nwk xpan (no selected is an error)
            network nwk_this;
            VerifyOrExit((status = get_current_network(nwk_this)) == registry_status::REG_SUCCESS);

            // Put nwk into networks
            networks.push_back(nwk_this);
        }
        else
        {
            network nwk;
            status = get_network_by_xpan(alias, nwk);
            if (status != REG_SUCCESS)
            {
                status = get_network_by_name(alias, nwk);
                if (status != REG_SUCCESS)
                {
                    VerifyOrExit((status = get_network_by_pan(alias, nwk)) == REG_SUCCESS);
                }
            }
            networks.push_back(nwk);
        }

        // Make results unique
        std::sort(networks.begin(), networks.end(), [](network const &a, network const &b) { return a.name < b.name; });
        std::unique(networks.begin(), networks.end(), [](network const &a, network const &b) {
            return ::ot::commissioner::utils::CaseInsensitiveEqual(a.name, b.name);
        });
    }

    ret.insert(ret.end(), networks.begin(), networks.end());

exit:
    return status;
}

registry_status registry::forget_current_network()
{
    return set_current_network(network_id{});
}

registry_status registry::set_current_network(const xpan_id xpan)
{
    network         nwk;
    registry_status status;

    VerifyOrExit((status = get_network_by_xpan(xpan, nwk)) == REG_SUCCESS);
    status = set_current_network(nwk.id);
exit:
    return status;
}

registry_status registry::set_current_network(const network_id &nwk_id)
{
    assert(storage != nullptr);

    return map_status(storage->current_network_set(nwk_id));
}

registry_status registry::get_current_network(network &ret)
{
    assert(storage != nullptr);
    network_id nwk_id;
    if (map_status(storage->current_network_get(nwk_id)) != registry_status::REG_SUCCESS)
    {
        return REG_ERROR;
    }

    return (nwk_id.id == EMPTY_ID) ? (ret = network{}, REG_SUCCESS) : map_status(storage->get(nwk_id, ret));
}

registry_status registry::get_current_network_xpan(uint64_t &ret)
{
    registry_status status;
    network         nwk;
    VerifyOrExit(REG_SUCCESS == (status = get_current_network(nwk)));
    ret = nwk.xpan;
exit:
    return status;
}

registry_status registry::lookup_one(const network &pred, network &ret)
{
    registry_status status;
    NetworkArray    networks;
    VerifyOrExit((status = map_status(storage->lookup(pred, networks))) == REG_SUCCESS);
    VerifyOrExit(networks.size() == 1, status = REG_AMBIGUITY);
    ret = networks[0];
exit:
    return status;
}

registry_status registry::get_network_by_xpan(const xpan_id xpan, network &ret)
{
    network nwk{};
    nwk.xpan = xpan;
    return lookup_one(nwk, ret);
}

registry_status registry::get_network_by_name(const std::string &name, network &ret)
{
    network nwk{};
    nwk.name = name;
    return lookup_one(nwk, ret);
}

registry_status registry::get_network_by_pan(const std::string &pan, network &ret)
{
    network nwk{};
    nwk.pan = pan;
    return lookup_one(nwk, ret);
}

registry_status registry::get_domain_name_by_xpan(const xpan_id xpan, std::string &name)
{
    registry_status status;
    network         pred;
    network         nwk;
    domain          dom;
    DomainArray     domains;

    pred.xpan = xpan;
    VerifyOrExit(REG_SUCCESS == (status = lookup_one(pred, nwk)));
    dom.id = nwk.dom_id;
    VerifyOrExit(REG_SUCCESS == (status = map_status(storage->lookup(dom, domains))));
    VerifyOrExit(domains.size() == 1, status = REG_AMBIGUITY);
    name = domains[0].name;
exit:
    return status;
}

} // namespace persistent_storage
} // namespace commissioner
} // namespace ot
