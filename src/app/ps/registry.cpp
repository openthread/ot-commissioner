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

registry_status is_xpan_string(std::string candidate)
{
    if (candidate.empty() || candidate.length() > 16)
        return REG_ERROR;
    for (auto c : candidate)
    {
        if (!std::isxdigit(c))
        {
            return REG_ERROR;
        }
    }

    return REG_SUCCESS;
}

const std::string ALIAS_THIS{"this"};
const std::string ALIAS_ALL{"all"};
const std::string ALIAS_OTHER{"other"};

} // namespace

registry *CreateRegistry(const std::string &aFile)
{
    return new registry(aFile);
}

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
                nwk.xpan   = val.mExtendedPanId;
                nwk.ccm    = ((val.mState.mConnectionMode == 4) ? 1 : 0);
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

registry_status registry::get_all_border_routers(BorderRouterArray &ret)
{
    return map_status(storage->lookup(border_router{}, ret));
}

registry_status registry::get_border_router(const border_router_id rawid, border_router &br)
{
    return map_status(storage->get(rawid, br));
}

registry_status registry::get_border_routers_in_network(const xpan_id xpan, BorderRouterArray &ret)
{
    network         nwk;
    border_router   pred;
    registry_status status = get_network_by_xpan(xpan, nwk);

    VerifyOrExit(status == REG_SUCCESS);
    pred.nwk_id = nwk.id;
    status      = map_status(storage->lookup(pred, ret));
exit:
    return status;
}

registry_status registry::get_network_xpans_in_domain(const std::string &dom_name, XpanIdArray &ret)
{
    NetworkArray    networks;
    registry_status status = get_networks_in_domain(dom_name, networks);

    if (status == REG_SUCCESS)
    {
        for (auto nwk : networks)
        {
            ret.push_back(nwk.xpan);
        }
    }
    return status;
}

registry_status registry::get_networks_in_domain(const std::string &dom_name, NetworkArray &ret)
{
    registry_status     status;
    std::vector<domain> domains;
    network             nwk{};

    if (dom_name == ALIAS_THIS)
    {
        domain  curDom;
        network curNwk;
        status = get_current_network(curNwk);
        if (status == REG_SUCCESS)
        {
            status = map_status(storage->get(curNwk.dom_id, curDom));
        }
        VerifyOrExit(status == REG_SUCCESS);
        domains.push_back(curDom);
    }
    else
    {
        domain dom{EMPTY_ID, dom_name};
        VerifyOrExit((status = map_status(storage->lookup(dom, domains))) == REG_SUCCESS);
    }
    VerifyOrExit(domains.size() < 2, status = REG_AMBIGUITY);
    nwk.dom_id = domains[0].id;
    status     = map_status(storage->lookup(nwk, ret));
exit:
    return status;
}

registry_status registry::get_all_domains(DomainArray &ret)
{
    registry_status status;

    status = map_status(storage->lookup(domain{}, ret));

    return status;
}

registry_status registry::get_domains_by_aliases(const StringArray &aliases, DomainArray &ret, StringArray &unresolved)
{
    registry_status status;
    DomainArray     domains;

    for (auto alias : aliases)
    {
        domain dom;

        if (alias == ALIAS_THIS)
        {
            network nwk;
            status = get_current_network(nwk);
            if (status == REG_SUCCESS)
            {
                status = map_status(storage->get(nwk.dom_id, dom));
            }
        }
        else
        {
            DomainArray result;
            domain      pred;
            pred.name = alias;
            status    = map_status(storage->lookup(pred, result));
            if (status == REG_SUCCESS)
            {
                if (result.size() == 1)
                {
                    dom = result.front();
                }
                else if (result.size() > 1)
                {
                    unresolved.push_back(alias);
                    ExitNow(status = REG_AMBIGUITY);
                }
            }
        }
        if (status == REG_SUCCESS)
        {
            domains.push_back(dom);
        }
        else
        {
            unresolved.push_back(alias);
        }
    }
    ret.insert(ret.end(), domains.begin(), domains.end());
    status = domains.size() > 0 ? REG_SUCCESS : REG_NOT_FOUND;
exit:
    return status;
}

registry_status registry::get_all_networks(NetworkArray &ret)
{
    registry_status status;

    status = map_status(storage->lookup(network{}, ret));

    return status;
}

registry_status registry::get_network_xpans_by_aliases(const StringArray &aliases,
                                                       XpanIdArray &      ret,
                                                       StringArray &      unresolved)
{
    NetworkArray    networks;
    registry_status status = get_networks_by_aliases(aliases, networks, unresolved);

    if (status == REG_SUCCESS)
    {
        for (auto nwk : networks)
        {
            ret.push_back(nwk.xpan);
        }
    }
    return status;
}

registry_status registry::get_networks_by_aliases(const StringArray &aliases,
                                                  NetworkArray &     ret,
                                                  StringArray &      unresolved)
{
    registry_status status;
    NetworkArray    networks;

    if (aliases.size() == 0)
        return REG_ERROR;

    for (auto alias : aliases)
    {
        if (alias == ALIAS_ALL || alias == ALIAS_OTHER)
        {
            VerifyOrExit(aliases.size() == 1, status = REG_ERROR); // Interpreter must have taken care of this
            VerifyOrExit((status = get_all_networks(networks)) == REG_SUCCESS);
            if (alias == ALIAS_OTHER)
            {
                network nwk_this;
                VerifyOrExit((status = get_current_network(nwk_this)) == REG_SUCCESS);
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
            status = get_current_network(nwk_this);
            if (status == registry_status::REG_SUCCESS && nwk_this.id.id != EMPTY_ID)
            {
                // Put nwk into networks
                networks.push_back(nwk_this);
            }
            else // failed 'this' must not break the translation flow
            {
                unresolved.push_back(alias);
            }
        }
        else
        {
            network nwk;

            status = is_xpan_string(alias);
            if (status == REG_SUCCESS)
            {
                status = get_network_by_xpan(alias, nwk);
            }
            if (status != REG_SUCCESS)
            {
                status = get_network_by_name(alias, nwk);
                if (status != REG_SUCCESS)
                {
                    status = get_network_by_pan(alias, nwk);
                }
            }
            if (status == REG_SUCCESS)
            {
                networks.push_back(nwk);
            }
            else // unresolved alias must not break processing
            {
                unresolved.push_back(alias);
            }
        }

        // Make results unique
        std::sort(networks.begin(), networks.end(), [](network const &a, network const &b) { return a.xpan < b.xpan; });
        auto last = std::unique(networks.begin(), networks.end(),
                                [](network const &a, network const &b) { return a.xpan == b.xpan; });
        networks.erase(last, networks.end());
    }

    ret.insert(ret.end(), networks.begin(), networks.end());
    status = networks.size() > 0 ? REG_SUCCESS : REG_NOT_FOUND;
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

registry_status registry::set_current_network(const border_router &br)
{
    assert(storage != nullptr);

    return map_status(storage->current_network_set(br.nwk_id));
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
    network         nwk;
    domain          dom;

    VerifyOrExit(REG_SUCCESS == (status = get_network_by_xpan(xpan, nwk)));
    VerifyOrExit(REG_SUCCESS == (status = map_status(storage->get(nwk.dom_id, dom))));
    name = dom.name;
exit:
    return status;
}

registry_status registry::delete_border_router_by_id(const border_router_id router_id)
{
    registry_status            status;
    border_router              br;
    network                    current;
    std::vector<border_router> routers;
    border_router              pred;

    VerifyOrExit((status = map_status(storage->get(router_id, br))) == REG_SUCCESS);

    status = get_current_network(current);
    if (status == REG_SUCCESS && (current.id.id != EMPTY_ID))
    {
        network brNetwork;
        // Check we don't delete the last border router in the current network
        VerifyOrExit((status = map_status(storage->get(router_id, br))) == REG_SUCCESS);
        VerifyOrExit((status = map_status(storage->get(br.nwk_id, brNetwork))) == REG_SUCCESS);
        if (brNetwork.xpan == current.xpan)
        {
            pred.nwk_id = brNetwork.id;
            VerifyOrExit((status = map_status(storage->lookup(pred, routers))) == REG_SUCCESS);
            if (routers.size() <= 1)
            {
                status = REG_ERROR;
                goto exit;
            }
        }
    }

    status = map_status(storage->del(router_id));

    if (br.nwk_id.id != EMPTY_ID)
    {
        // Was it the last in the network?
        routers.clear();
        pred.nwk_id = br.nwk_id;
        status      = map_status(storage->lookup(pred, routers));
        if (status == REG_NOT_FOUND)
        {
            network nwk;
            VerifyOrExit((status = map_status(storage->get(br.nwk_id, nwk))) == REG_SUCCESS);
            VerifyOrExit((status = map_status(storage->del(br.nwk_id))) == REG_SUCCESS);

            VerifyOrExit((status = drop_domain_if_empty(nwk.dom_id)) == REG_SUCCESS);
        }
    }
exit:
    return status;
}

registry_status registry::delete_border_routers_in_networks(const StringArray &aliases, StringArray &unresolved)
{
    registry_status status;
    NetworkArray    nwks;
    network         current;

    // Check aliases acceptable
    for (auto alias : aliases)
    {
        if (alias == ALIAS_ALL || alias == ALIAS_THIS || (alias == ALIAS_OTHER && aliases.size() > 1))
        {
            return REG_DATA_INVALID;
        }
    }

    VerifyOrExit((status = get_networks_by_aliases(aliases, nwks, unresolved)) == REG_SUCCESS);

    // Processing explicit network aliases
    if (aliases[0] != ALIAS_OTHER)
    {
        VerifyOrExit((status = get_current_network(current)) == REG_SUCCESS);
    }

    if (current.id.id != EMPTY_ID)
    {
        auto found = std::find_if(nwks.begin(), nwks.end(), [&](const network &a) { return a.id.id == current.id.id; });
        if (found != nwks.end())
        {
            status = REG_DATA_INVALID;
            goto exit;
        }
    }

    for (auto nwk : nwks)
    {
        BorderRouterArray brs;
        border_router     br;

        br.nwk_id = nwk.id;
        VerifyOrExit((status = map_status(storage->lookup(br, brs))) == REG_SUCCESS);
        for (auto &&br : brs)
        {
            VerifyOrExit((status = delete_border_router_by_id(br.id)) == REG_SUCCESS);
        }

        VerifyOrExit((status = drop_domain_if_empty(nwk.dom_id)) == REG_SUCCESS);
    }
exit:
    return status;
}

registry_status registry::drop_domain_if_empty(const domain_id &dom_id)
{
    registry_status status = REG_SUCCESS;

    if (dom_id.id != EMPTY_ID)
    {
        network              npred;
        std::vector<network> nwks;

        npred.dom_id = dom_id;
        status       = map_status(storage->lookup(npred, nwks));
        VerifyOrExit(status == REG_SUCCESS || status == REG_NOT_FOUND);
        if (nwks.size() == 0)
        {
            // Drop empty domain
            status = map_status(storage->del(dom_id));
        }
    }
exit:
    return status;
}

registry_status registry::delete_border_routers_in_domain(const std::string &domain_name)
{
    domain          dom;
    DomainArray     doms;
    registry_status status;
    network         current;
    XpanIdArray     xpans;
    StringArray     aliases;
    StringArray     unresolved;

    dom.name = domain_name;
    VerifyOrExit((status = map_status(storage->lookup(dom, doms))) == REG_SUCCESS);
    if (doms.size() != 1)
    {
        status = REG_ERROR;
        goto exit;
    }

    VerifyOrExit((status = get_current_network(current)) == REG_SUCCESS);
    if (current.dom_id.id != EMPTY_ID)
    {
        domain currentDomain;
        VerifyOrExit((status = map_status(storage->get(current.dom_id, currentDomain))) == REG_SUCCESS);

        if (currentDomain.name == domain_name)
        {
            status = REG_DATA_INVALID;
            goto exit;
        }
    }

    VerifyOrExit((status = get_network_xpans_in_domain(domain_name, xpans)) == REG_SUCCESS);
    if (xpans.empty())
    {
        // Domain is already empty
        status = map_status(storage->del(doms[0].id));
        goto exit;
    }

    for (auto &&xpan : xpans)
    {
        // TODO [MP] Should better be of type xpan_it then uint64_t
        aliases.push_back(xpan_id(xpan).str());
    }
    VerifyOrExit((status = delete_border_routers_in_networks(aliases, unresolved)) == REG_SUCCESS);
    if (!unresolved.empty())
    {
        status = REG_AMBIGUITY;
        goto exit;
    }
    // Domain will be deleted
exit:
    return status;
}

registry_status registry::update(const network &nwk)
{
    return map_status(storage->update(nwk));
}

} // namespace persistent_storage
} // namespace commissioner
} // namespace ot
