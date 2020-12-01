#include "registry.hpp"
#include "persistent_storage_json.hpp"

#include <cassert>
#include <sstream>

namespace ot::commissioner::persistent_storage {

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

registry_status registry::lookup(registrar const *val, std::vector<registrar> &ret)
{
    assert(storage != nullptr);

    return map_status(storage->lookup(val, ret));
}

registry_status registry::lookup(domain const *val, std::vector<domain> &ret)
{
    assert(storage != nullptr);

    return map_status(storage->lookup(val, ret));
}

registry_status registry::lookup(network const *val, std::vector<network> &ret)
{
    assert(storage != nullptr);

    return map_status(storage->lookup(val, ret));
}

registry_status registry::lookup(border_router const *val, std::vector<border_router> &ret)
{
    assert(storage != nullptr);

    return map_status(storage->lookup(val, ret));
}

registry_status registry::get(registrar_id const &id, registrar &ret_val)
{
    assert(storage != nullptr);

    return map_status(storage->get(id, ret_val));
}

registry_status registry::get(domain_id const &id, domain &ret_val)
{
    assert(storage != nullptr);

    return map_status(storage->get(id, ret_val));
}

registry_status registry::get(network_id const &id, network &ret_val)
{
    assert(storage != nullptr);

    return map_status(storage->get(id, ret_val));
}

registry_status registry::get(border_router_id const &id, border_router &ret_val)
{
    assert(storage != nullptr);

    return map_status(storage->get(id, ret_val));
}

registry_status registry::add(registrar const &val, registrar_id &ret_id)
{
    assert(storage != nullptr);

    return map_status(storage->add(val, ret_id));
}

registry_status registry::add(domain const &val, domain_id &ret_id)
{
    assert(storage != nullptr);

    return map_status(storage->add(val, ret_id));
}

registry_status registry::add(network const &val, network_id &ret_id)
{
    assert(storage != nullptr);

    return map_status(storage->add(val, ret_id));
}

registry_status registry::add(border_router const &val, border_router_id &ret_id)
{
    assert(storage != nullptr);

    return map_status(storage->add(val, ret_id));
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
        // TODO refine why lookup() functions are with the pointer?
        status = lookup(&dom, domains);
        if (status == REG_ERROR || domains.size() > 1)
        {
            return REG_ERROR;
        }
        else if (status == REG_NOT_FOUND)
        {
            domain_id dom_id{EMPTY_ID};
            status = add(dom, dom_id);
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
    if ((val.mPresentFlags & BorderAgent::kNetworkNameBit) != 0 ||
        (val.mPresentFlags & BorderAgent::kExtendedPanIdBit) != 0)
    {
        // If xpan present lookup with it only
        // TODO discuss: is different name an error? Not yet.
        if ((val.mPresentFlags & BorderAgent::kExtendedPanIdBit) != 0)
        {
            std::ostringstream tmp;
            // Assumption: no leading zeroes for the xpan
            tmp << std::hex << val.mExtendedPanId;
            nwk.xpan = tmp.str();
        }
        else
        {
            nwk.name = val.mNetworkName;
        }
        std::vector<network> nwks;
        // TODO replace lookup with lookup_any()?
        status = lookup(&nwk, nwks);
        if (status == REG_ERROR || nwks.size() > 1)
        {
            if (domain_created)
            {
                del(dom.id);
            }
            return status;
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
            status     = add(nwk, nwk_id);
            if (status != REG_SUCCESS)
            {
                if (domain_created)
                {
                    del(dom.id);
                }
                return status;
            }
            nwk.id          = nwk_id;
            network_created = true;
        }
        else
        {
            nwk = nwks[0];
            if (nwk.dom_id.id != dom.id.id)
            {
                nwk.dom_id.id = dom.id.id;
                status        = update(nwk);
                if (status != REG_SUCCESS)
                {
                    if (domain_created)
                    {
                        del(dom.id);
                    }
                    return status;
                }
            }
        }
    }

    border_router br{EMPTY_ID, nwk.id, val};
    if (br.nwk_id.id == EMPTY_ID)
    {
        // TODO remove nwk and/or dom if necessary
        return REG_ERROR;
    }

    // Lookup border_router by address and port to decide to add() or update()
    // Assuming address and port are set (it should be so).
    border_router lookup_br{};
    lookup_br.agent.mAddr         = val.mAddr;
    lookup_br.agent.mPort         = val.mPort;
    lookup_br.agent.mPresentFlags = BorderAgent::kAddrBit | BorderAgent::kPortBit;
    std::vector<border_router> routers;
    status = lookup(&lookup_br, routers);
    if (status == REG_SUCCESS && routers.size() == 1)
    {
        br.id.id = routers[0].id.id;
        status   = update(br);
    }
    else if (status == REG_NOT_FOUND)
    {
        status = add(br, br.id);
    }

    if (status != REG_SUCCESS && network_created)
    {
        del(br.nwk_id);
    }

    return status;
}

registry_status registry::del(registrar_id const &id)
{
    assert(storage != nullptr);

    return map_status(storage->del(id));
}

registry_status registry::del(domain_id const &id)
{
    assert(storage != nullptr);

    return map_status(storage->del(id));
}

registry_status registry::del(network_id const &id)
{
    assert(storage != nullptr);

    return map_status(storage->del(id));
}

registry_status registry::del(border_router_id const &id)
{
    assert(storage != nullptr);

    return map_status(storage->del(id));
}

registry_status registry::update(registrar const &val)
{
    assert(storage != nullptr);

    return map_status(storage->update(val));
}

registry_status registry::update(domain const &val)
{
    assert(storage != nullptr);

    return map_status(storage->update(val));
}

registry_status registry::update(network const &val)
{
    assert(storage != nullptr);

    return map_status(storage->update(val));
}

registry_status registry::update(border_router const &val)
{
    assert(storage != nullptr);

    return map_status(storage->update(val));
}

} // namespace ot::commissioner::persistent_storage
