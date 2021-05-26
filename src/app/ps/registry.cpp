#include "registry.hpp"
#include "persistent_storage_json.hpp"
#include "common/error_macros.hpp"
#include "common/utils.hpp"

#include <cassert>
#include <sstream>

namespace ot {
namespace commissioner {
namespace persistent_storage {

namespace {
Registry::Status SuccessStatus(PersistentStorage::Status ps_st)
{
    if (ps_st == PersistentStorage::Status::PS_SUCCESS)
    {
        return Registry::Status::REG_SUCCESS;
    }
    return Registry::Status::REG_ERROR;
}

Registry::Status MapStatus(PersistentStorage::Status ps_st)
{
    if (ps_st == PersistentStorage::Status::PS_ERROR)
    {
        return Registry::Status::REG_ERROR;
    }
    if (ps_st == PersistentStorage::Status::PS_SUCCESS)
    {
        return Registry::Status::REG_SUCCESS;
    }
    if (ps_st == PersistentStorage::Status::PS_NOT_FOUND)
    {
        return Registry::Status::REG_NOT_FOUND;
    }
    return Registry::Status::REG_ERROR;
}

const std::string ALIAS_THIS{"this"};
const std::string ALIAS_ALL{"all"};
const std::string ALIAS_OTHER{"other"};

} // namespace

Registry *CreateRegistry(const std::string &aFile)
{
    return new Registry(aFile);
}

Registry::Registry(PersistentStorage *strg)
    : manage_storage(false)
    , storage(strg)
{
    assert(storage != nullptr);
}

Registry::Registry(std::string const &name)
    : manage_storage(true)
{
    storage = new PersistentStorageJson(name);
}

Registry::~Registry()
{
    Close();

    if (manage_storage)
    {
        delete storage;
    }
    storage = nullptr;
}

Registry::Status Registry::Open()
{
    if (storage == nullptr)
    {
        return Registry::Status::REG_ERROR;
    }

    return SuccessStatus(storage->Open());
}

Registry::Status Registry::Close()
{
    if (storage == nullptr)
    {
        return Registry::Status::REG_ERROR;
    }
    return SuccessStatus(storage->Close());
}

Registry::Status Registry::Add(BorderAgent const &val)
{
    domain           dom{};
    bool             domain_created = false;
    Registry::Status status;

    if ((val.mPresentFlags & BorderAgent::kDomainNameBit) != 0)
    {
        dom.name = val.mDomainName;
        std::vector<domain> domains;
        status = MapStatus(storage->Lookup(dom, domains));
        if (status == Registry::Status::REG_ERROR || domains.size() > 1)
        {
            return Registry::Status::REG_ERROR;
        }
        else if (status == Registry::Status::REG_NOT_FOUND)
        {
            domain_id dom_id{EMPTY_ID};
            status = MapStatus(storage->Add(dom, dom_id));
            if (status != Registry::Status::REG_SUCCESS)
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
            status = MapStatus(storage->Lookup(nwk, nwks));
            if (status == Registry::Status::REG_ERROR || nwks.size() > 1)
            {
                throw status;
            }
            else if (status == Registry::Status::REG_NOT_FOUND)
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
                status     = MapStatus(storage->Add(nwk, nwk_id));
                if (status != Registry::Status::REG_SUCCESS)
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
                    status        = MapStatus(storage->Update(nwk));
                    if (status != Registry::Status::REG_SUCCESS)
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
                throw Registry::Status::REG_ERROR;
            }

            // Lookup border_router by address and port to decide to add() or update()
            // Assuming address and port are set (it should be so).
            border_router lookup_br{};
            lookup_br.agent.mAddr          = val.mAddr;
            lookup_br.agent.mExtendedPanId = val.mExtendedPanId;
            lookup_br.agent.mPresentFlags  = BorderAgent::kAddrBit | BorderAgent::kExtendedPanIdBit;
            std::vector<border_router> routers;
            status = MapStatus(storage->Lookup(lookup_br, routers));
            if (status == Registry::Status::REG_SUCCESS && routers.size() == 1)
            {
                br.id.id = routers[0].id.id;
                status   = MapStatus(storage->Update(br));
            }
            else if (status == Registry::Status::REG_NOT_FOUND)
            {
                status = MapStatus(storage->Add(br, br.id));
            }

            if (status != Registry::Status::REG_SUCCESS)
            {
                throw status;
            }
        } catch (Registry::Status thrown_status)
        {
            if (network_created)
            {
                MapStatus(storage->Del(nwk.id));
            }
            throw;
        }
    } catch (Registry::Status thrown_status)
    {
        if (domain_created)
        {
            MapStatus(storage->Del(dom.id));
        }
        status = thrown_status;
    }
    return status;
}

Registry::Status Registry::GetAllBorderRouters(BorderRouterArray &ret)
{
    return MapStatus(storage->Lookup(border_router{}, ret));
}

Registry::Status Registry::GetBorderRouter(const border_router_id rawid, border_router &br)
{
    return MapStatus(storage->Get(rawid, br));
}

Registry::Status Registry::GetBorderRoutersInNetwork(const xpan_id xpan, BorderRouterArray &ret)
{
    network          nwk;
    border_router    pred;
    Registry::Status status = GetNetworkByXpan(xpan, nwk);

    VerifyOrExit(status == Registry::Status::REG_SUCCESS);
    pred.nwk_id = nwk.id;
    status      = MapStatus(storage->Lookup(pred, ret));
exit:
    return status;
}

Registry::Status Registry::GetNetworkXpansInDomain(const std::string &dom_name, XpanIdArray &ret)
{
    NetworkArray     networks;
    Registry::Status status = GetNetworksInDomain(dom_name, networks);

    if (status == Registry::Status::REG_SUCCESS)
    {
        for (auto nwk : networks)
        {
            ret.push_back(nwk.xpan);
        }
    }
    return status;
}

Registry::Status Registry::GetNetworksInDomain(const std::string &dom_name, NetworkArray &ret)
{
    Registry::Status    status;
    std::vector<domain> domains;
    network             nwk{};

    if (dom_name == ALIAS_THIS)
    {
        domain  curDom;
        network curNwk;
        status = GetCurrentNetwork(curNwk);
        if (status == Registry::Status::REG_SUCCESS)
        {
            status = MapStatus(storage->Get(curNwk.dom_id, curDom));
        }
        VerifyOrExit(status == Registry::Status::REG_SUCCESS);
        domains.push_back(curDom);
    }
    else
    {
        domain dom{EMPTY_ID, dom_name};
        VerifyOrExit((status = MapStatus(storage->Lookup(dom, domains))) == Registry::Status::REG_SUCCESS);
    }
    VerifyOrExit(domains.size() < 2, status = Registry::Status::REG_AMBIGUITY);
    nwk.dom_id = domains[0].id;
    status     = MapStatus(storage->Lookup(nwk, ret));
exit:
    return status;
}

Registry::Status Registry::GetAllDomains(DomainArray &ret)
{
    Registry::Status status;

    status = MapStatus(storage->Lookup(domain{}, ret));

    return status;
}

Registry::Status Registry::GetDomainsByAliases(const StringArray &aliases, DomainArray &ret, StringArray &unresolved)
{
    Registry::Status status;
    DomainArray      domains;

    for (auto alias : aliases)
    {
        domain dom;

        if (alias == ALIAS_THIS)
        {
            network nwk;
            status = GetCurrentNetwork(nwk);
            if (status == Registry::Status::REG_SUCCESS)
            {
                status = MapStatus(storage->Get(nwk.dom_id, dom));
            }
        }
        else
        {
            DomainArray result;
            domain      pred;
            pred.name = alias;
            status    = MapStatus(storage->Lookup(pred, result));
            if (status == Registry::Status::REG_SUCCESS)
            {
                if (result.size() == 1)
                {
                    dom = result.front();
                }
                else if (result.size() > 1)
                {
                    unresolved.push_back(alias);
                    ExitNow(status = Registry::Status::REG_AMBIGUITY);
                }
            }
        }
        if (status == Registry::Status::REG_SUCCESS)
        {
            domains.push_back(dom);
        }
        else
        {
            unresolved.push_back(alias);
        }
    }
    ret.insert(ret.end(), domains.begin(), domains.end());
    status = domains.size() > 0 ? Registry::Status::REG_SUCCESS : Registry::Status::REG_NOT_FOUND;
exit:
    return status;
}

Registry::Status Registry::GetAllNetworks(NetworkArray &ret)
{
    Registry::Status status;

    status = MapStatus(storage->Lookup(network{}, ret));

    return status;
}

Registry::Status Registry::GetNetworkXpansByAliases(const StringArray &aliases,
                                                    XpanIdArray &      ret,
                                                    StringArray &      unresolved)
{
    NetworkArray     networks;
    Registry::Status status = GetNetworksByAliases(aliases, networks, unresolved);

    if (status == Registry::Status::REG_SUCCESS)
    {
        for (auto nwk : networks)
        {
            ret.push_back(nwk.xpan);
        }
    }
    return status;
}

Registry::Status Registry::GetNetworksByAliases(const StringArray &aliases, NetworkArray &ret, StringArray &unresolved)
{
    Registry::Status status;
    NetworkArray     networks;

    if (aliases.size() == 0)
        return Registry::Status::REG_ERROR;

    for (auto alias : aliases)
    {
        if (alias == ALIAS_ALL || alias == ALIAS_OTHER)
        {
            VerifyOrExit(aliases.size() == 1,
                         status = Registry::Status::REG_ERROR); // Interpreter must have taken care of this
            VerifyOrExit((status = GetAllNetworks(networks)) == Registry::Status::REG_SUCCESS);
            if (alias == ALIAS_OTHER)
            {
                network nwk_this;
                VerifyOrExit((status = GetCurrentNetwork(nwk_this)) == Registry::Status::REG_SUCCESS);
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
            status = GetCurrentNetwork(nwk_this);
            if (status == Registry::Status::REG_SUCCESS && nwk_this.id.id != EMPTY_ID)
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
            xpan_id xpid;

            status = (xpid.from_hex(alias) == ERROR_NONE) ? Registry::Status::REG_SUCCESS : Registry::Status::REG_ERROR;

            if (status == Registry::Status::REG_SUCCESS)
            {
                status = GetNetworkByXpan(xpid, nwk);
            }
            if (status != Registry::Status::REG_SUCCESS)
            {
                status = GetNetworkByName(alias, nwk);
                if (status != Registry::Status::REG_SUCCESS)
                {
                    status = GetNetworkByPan(alias, nwk);
                }
            }
            if (status == Registry::Status::REG_SUCCESS)
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
    status = networks.size() > 0 ? Registry::Status::REG_SUCCESS : Registry::Status::REG_NOT_FOUND;
exit:
    return status;
}

Registry::Status Registry::ForgetCurrentNetwork()
{
    return SetCurrentNetwork(network_id{});
}

Registry::Status Registry::SetCurrentNetwork(const xpan_id xpan)
{
    network          nwk;
    Registry::Status status;

    VerifyOrExit((status = GetNetworkByXpan(xpan, nwk)) == Registry::Status::REG_SUCCESS);
    status = SetCurrentNetwork(nwk.id);
exit:
    return status;
}

Registry::Status Registry::SetCurrentNetwork(const network_id &nwk_id)
{
    assert(storage != nullptr);

    return MapStatus(storage->CurrentNetworkSet(nwk_id));
}

Registry::Status Registry::SetCurrentNetwork(const border_router &br)
{
    assert(storage != nullptr);

    return MapStatus(storage->CurrentNetworkSet(br.nwk_id));
}

Registry::Status Registry::GetCurrentNetwork(network &ret)
{
    assert(storage != nullptr);
    network_id nwk_id;
    if (MapStatus(storage->CurrentNetworkGet(nwk_id)) != Registry::Status::REG_SUCCESS)
    {
        return Registry::Status::REG_ERROR;
    }

    return (nwk_id.id == EMPTY_ID) ? (ret = network{}, Registry::Status::REG_SUCCESS)
                                   : MapStatus(storage->Get(nwk_id, ret));
}

Registry::Status Registry::GetCurrentNetworkXpan(uint64_t &ret)
{
    Registry::Status status;
    network          nwk;
    VerifyOrExit(Registry::Status::REG_SUCCESS == (status = GetCurrentNetwork(nwk)));
    ret = nwk.xpan;
exit:
    return status;
}

Registry::Status Registry::LookupOne(const network &pred, network &ret)
{
    Registry::Status status;
    NetworkArray     networks;
    VerifyOrExit((status = MapStatus(storage->Lookup(pred, networks))) == Registry::Status::REG_SUCCESS);
    VerifyOrExit(networks.size() == 1, status = Registry::Status::REG_AMBIGUITY);
    ret = networks[0];
exit:
    return status;
}

Registry::Status Registry::GetNetworkByXpan(const xpan_id xpan, network &ret)
{
    network nwk{};
    nwk.xpan = xpan;
    return LookupOne(nwk, ret);
}

Registry::Status Registry::GetNetworkByName(const std::string &name, network &ret)
{
    network nwk{};
    nwk.name = name;
    return LookupOne(nwk, ret);
}

Registry::Status Registry::GetNetworkByPan(const std::string &pan, network &ret)
{
    network nwk{};
    nwk.pan = pan;
    return LookupOne(nwk, ret);
}

Registry::Status Registry::GetDomainNameByXpan(const xpan_id xpan, std::string &name)
{
    Registry::Status status;
    network          nwk;
    domain           dom;

    VerifyOrExit(Registry::Status::REG_SUCCESS == (status = GetNetworkByXpan(xpan, nwk)));
    VerifyOrExit(Registry::Status::REG_SUCCESS == (status = MapStatus(storage->Get(nwk.dom_id, dom))));
    name = dom.name;
exit:
    return status;
}

Registry::Status Registry::DeleteBorderRouterById(const border_router_id router_id)
{
    Registry::Status           status;
    border_router              br;
    network                    current;
    std::vector<border_router> routers;
    border_router              pred;

    VerifyOrExit((status = MapStatus(storage->Get(router_id, br))) == Registry::Status::REG_SUCCESS);

    status = GetCurrentNetwork(current);
    if (status == Registry::Status::REG_SUCCESS && (current.id.id != EMPTY_ID))
    {
        network brNetwork;
        // Check we don't delete the last border router in the current network
        VerifyOrExit((status = MapStatus(storage->Get(router_id, br))) == Registry::Status::REG_SUCCESS);
        VerifyOrExit((status = MapStatus(storage->Get(br.nwk_id, brNetwork))) == Registry::Status::REG_SUCCESS);
        if (brNetwork.xpan == current.xpan)
        {
            pred.nwk_id = brNetwork.id;
            VerifyOrExit((status = MapStatus(storage->Lookup(pred, routers))) == Registry::Status::REG_SUCCESS);
            if (routers.size() <= 1)
            {
                status = Registry::Status::REG_RESTRICTED;
                goto exit;
            }
        }
    }

    status = MapStatus(storage->Del(router_id));

    if (br.nwk_id.id != EMPTY_ID)
    {
        // Was it the last in the network?
        routers.clear();
        pred.nwk_id = br.nwk_id;
        status      = MapStatus(storage->Lookup(pred, routers));
        if (status == Registry::Status::REG_NOT_FOUND)
        {
            network nwk;
            VerifyOrExit((status = MapStatus(storage->Get(br.nwk_id, nwk))) == Registry::Status::REG_SUCCESS);
            VerifyOrExit((status = MapStatus(storage->Del(br.nwk_id))) == Registry::Status::REG_SUCCESS);

            VerifyOrExit((status = DropDomainIfEmpty(nwk.dom_id)) == Registry::Status::REG_SUCCESS);
        }
    }
exit:
    return status;
}

Registry::Status Registry::DeleteBorderRoutersInNetworks(const StringArray &aliases, StringArray &unresolved)
{
    Registry::Status status;
    NetworkArray     nwks;
    network          current;

    // Check aliases acceptable
    for (auto alias : aliases)
    {
        if (alias == ALIAS_THIS)
        {
            return Registry::Status::REG_RESTRICTED;
        }
        if (alias == ALIAS_ALL || (alias == ALIAS_OTHER && aliases.size() > 1))
        {
            return Registry::Status::REG_DATA_INVALID;
        }
    }

    VerifyOrExit((status = GetNetworksByAliases(aliases, nwks, unresolved)) == Registry::Status::REG_SUCCESS);

    // Processing explicit network aliases
    if (aliases[0] != ALIAS_OTHER)
    {
        VerifyOrExit((status = GetCurrentNetwork(current)) == Registry::Status::REG_SUCCESS);
    }

    if (current.id.id != EMPTY_ID)
    {
        auto found = std::find_if(nwks.begin(), nwks.end(), [&](const network &a) { return a.id.id == current.id.id; });
        if (found != nwks.end())
        {
            status = Registry::Status::REG_RESTRICTED;
            goto exit;
        }
    }

    for (auto nwk : nwks)
    {
        BorderRouterArray brs;
        border_router     br;

        br.nwk_id = nwk.id;
        VerifyOrExit((status = MapStatus(storage->Lookup(br, brs))) == Registry::Status::REG_SUCCESS);
        for (auto &&br : brs)
        {
            VerifyOrExit((status = DeleteBorderRouterById(br.id)) == Registry::Status::REG_SUCCESS);
        }

        VerifyOrExit((status = DropDomainIfEmpty(nwk.dom_id)) == Registry::Status::REG_SUCCESS);
    }
exit:
    return status;
}

Registry::Status Registry::DropDomainIfEmpty(const domain_id &dom_id)
{
    Registry::Status status = Registry::Status::REG_SUCCESS;

    if (dom_id.id != EMPTY_ID)
    {
        network              npred;
        std::vector<network> nwks;

        npred.dom_id = dom_id;
        status       = MapStatus(storage->Lookup(npred, nwks));
        VerifyOrExit(status == Registry::Status::REG_SUCCESS || status == Registry::Status::REG_NOT_FOUND);
        if (nwks.size() == 0)
        {
            // Drop empty domain
            status = MapStatus(storage->Del(dom_id));
        }
    }
exit:
    return status;
}

Registry::Status Registry::DeleteBorderRoutersInDomain(const std::string &domain_name)
{
    domain           dom;
    DomainArray      doms;
    Registry::Status status;
    network          current;
    XpanIdArray      xpans;
    StringArray      aliases;
    StringArray      unresolved;

    dom.name = domain_name;
    VerifyOrExit((status = MapStatus(storage->Lookup(dom, doms))) == Registry::Status::REG_SUCCESS);
    if (doms.size() != 1)
    {
        status = Registry::Status::REG_ERROR;
        goto exit;
    }

    VerifyOrExit((status = GetCurrentNetwork(current)) == Registry::Status::REG_SUCCESS);
    if (current.dom_id.id != EMPTY_ID)
    {
        domain currentDomain;
        VerifyOrExit((status = MapStatus(storage->Get(current.dom_id, currentDomain))) ==
                     Registry::Status::REG_SUCCESS);

        if (currentDomain.name == domain_name)
        {
            status = Registry::Status::REG_RESTRICTED;
            goto exit;
        }
    }

    VerifyOrExit((status = GetNetworkXpansInDomain(domain_name, xpans)) == Registry::Status::REG_SUCCESS);
    if (xpans.empty())
    {
        // Domain is already empty
        status = MapStatus(storage->Del(doms[0].id));
        goto exit;
    }

    for (auto &&xpan : xpans)
    {
        aliases.push_back(xpan_id(xpan).str());
    }
    VerifyOrExit((status = DeleteBorderRoutersInNetworks(aliases, unresolved)) == Registry::Status::REG_SUCCESS);
    if (!unresolved.empty())
    {
        status = Registry::Status::REG_AMBIGUITY;
        goto exit;
    }
    // Domain will be deleted
exit:
    return status;
}

Registry::Status Registry::Update(const network &nwk)
{
    return MapStatus(storage->Update(nwk));
}

} // namespace persistent_storage
} // namespace commissioner
} // namespace ot
