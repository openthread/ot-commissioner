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
Registry::Status SuccessStatus(PersistentStorage::Status aStatus)
{
    if (aStatus == PersistentStorage::Status::PS_SUCCESS)
    {
        return Registry::Status::REG_SUCCESS;
    }
    return Registry::Status::REG_ERROR;
}

Registry::Status MapStatus(PersistentStorage::Status aStatus)
{
    if (aStatus == PersistentStorage::Status::PS_ERROR)
    {
        return Registry::Status::REG_ERROR;
    }
    if (aStatus == PersistentStorage::Status::PS_SUCCESS)
    {
        return Registry::Status::REG_SUCCESS;
    }
    if (aStatus == PersistentStorage::Status::PS_NOT_FOUND)
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

Registry::Registry(PersistentStorage *aStorage)
    : mManageStorage(false)
    , mStorage(aStorage)
{
    assert(mStorage != nullptr);
}

Registry::Registry(std::string const &aName)
    : mManageStorage(true)
{
    mStorage = new PersistentStorageJson(aName);
}

Registry::~Registry()
{
    Close();

    if (mManageStorage)
    {
        delete mStorage;
    }
    mStorage = nullptr;
}

Registry::Status Registry::Open()
{
    if (mStorage == nullptr)
    {
        return Registry::Status::REG_ERROR;
    }

    return SuccessStatus(mStorage->Open());
}

Registry::Status Registry::Close()
{
    if (mStorage == nullptr)
    {
        return Registry::Status::REG_ERROR;
    }
    return SuccessStatus(mStorage->Close());
}

Registry::Status Registry::Add(BorderAgent const &aValue)
{
    Domain           dom{};
    bool             domainCreated = false;
    Registry::Status status;

    if ((aValue.mPresentFlags & BorderAgent::kDomainNameBit) != 0)
    {
        dom.mName = aValue.mDomainName;
        std::vector<Domain> domains;
        status = MapStatus(mStorage->Lookup(dom, domains));
        if (status == Registry::Status::REG_ERROR || domains.size() > 1)
        {
            return Registry::Status::REG_ERROR;
        }
        else if (status == Registry::Status::REG_NOT_FOUND)
        {
            DomainId domainId{EMPTY_ID};
            status = MapStatus(mStorage->Add(dom, domainId));
            if (status != Registry::Status::REG_SUCCESS)
            {
                return status;
            }
            dom.mId       = domainId;
            domainCreated = true;
        }
        else
        {
            dom = domains[0];
        }
    }

    Network nwk{};
    bool    networkCreated = false;

    try
    {
        if ((aValue.mPresentFlags & BorderAgent::kNetworkNameBit) != 0 ||
            (aValue.mPresentFlags & BorderAgent::kExtendedPanIdBit) != 0)
        {
            // If xpan present lookup with it only
            // Discussed: is different aName an error?
            // Decided: update network aName in the network entity
            if ((aValue.mPresentFlags & BorderAgent::kExtendedPanIdBit) != 0)
            {
                nwk.mXpan = aValue.mExtendedPanId;
            }
            else
            {
                nwk.mName = aValue.mNetworkName;
            }

            std::vector<Network> nwks;
            status = MapStatus(mStorage->Lookup(nwk, nwks));
            if (status == Registry::Status::REG_ERROR || nwks.size() > 1)
            {
                throw status;
            }
            else if (status == Registry::Status::REG_NOT_FOUND)
            {
                NetworkId networkId{EMPTY_ID};
                // It is possible we found the network by xpan
                if ((aValue.mPresentFlags & BorderAgent::kExtendedPanIdBit) != 0 &&
                    (aValue.mPresentFlags & BorderAgent::kNetworkNameBit) != 0)
                {
                    nwk.mName = aValue.mNetworkName;
                }
                nwk.mDomainId = dom.mId;
                nwk.mXpan     = aValue.mExtendedPanId;
                nwk.mCcm      = ((aValue.mState.mConnectionMode == 4) ? 1 : 0);
                status        = MapStatus(mStorage->Add(nwk, networkId));
                if (status != Registry::Status::REG_SUCCESS)
                {
                    throw status;
                }
                nwk.mId        = networkId;
                networkCreated = true;
            }
            else
            {
                nwk = nwks[0];
                if (nwk.mDomainId.mId != dom.mId.mId || nwk.mName != aValue.mNetworkName)
                {
                    nwk.mDomainId.mId = dom.mId.mId;
                    nwk.mName         = aValue.mNetworkName;
                    status            = MapStatus(mStorage->Update(nwk));
                    if (status != Registry::Status::REG_SUCCESS)
                    {
                        throw status;
                    }
                }
            }
        }

        BorderRouter br{EMPTY_ID, nwk.mId, aValue};
        try
        {
            if (br.mNetworkId.mId == EMPTY_ID)
            {
                throw Registry::Status::REG_ERROR;
            }

            // Lookup BorderRouter by address and port to decide to add() or update()
            // Assuming address and port are set (it should be so).
            BorderRouter lookupBr{};
            lookupBr.mAgent.mAddr          = aValue.mAddr;
            lookupBr.mAgent.mExtendedPanId = aValue.mExtendedPanId;
            lookupBr.mAgent.mPresentFlags  = BorderAgent::kAddrBit | BorderAgent::kExtendedPanIdBit;
            std::vector<BorderRouter> routers;
            status = MapStatus(mStorage->Lookup(lookupBr, routers));
            if (status == Registry::Status::REG_SUCCESS && routers.size() == 1)
            {
                br.mId.mId = routers[0].mId.mId;
                status     = MapStatus(mStorage->Update(br));
            }
            else if (status == Registry::Status::REG_NOT_FOUND)
            {
                status = MapStatus(mStorage->Add(br, br.mId));
            }

            if (status != Registry::Status::REG_SUCCESS)
            {
                throw status;
            }
        } catch (Registry::Status thrownStatus)
        {
            if (networkCreated)
            {
                MapStatus(mStorage->Del(nwk.mId));
            }
            throw;
        }
    } catch (Registry::Status thrownStatus)
    {
        if (domainCreated)
        {
            MapStatus(mStorage->Del(dom.mId));
        }
        status = thrownStatus;
    }
    return status;
}

Registry::Status Registry::GetAllBorderRouters(BorderRouterArray &aRet)
{
    return MapStatus(mStorage->Lookup(BorderRouter{}, aRet));
}

Registry::Status Registry::GetBorderRouter(const BorderRouterId aRawId, BorderRouter &br)
{
    return MapStatus(mStorage->Get(aRawId, br));
}

Registry::Status Registry::GetBorderRoutersInNetwork(const XpanId aXpan, BorderRouterArray &aRet)
{
    Network          nwk;
    BorderRouter     pred;
    Registry::Status status = GetNetworkByXpan(aXpan, nwk);

    VerifyOrExit(status == Registry::Status::REG_SUCCESS);
    pred.mNetworkId = nwk.mId;
    status          = MapStatus(mStorage->Lookup(pred, aRet));
exit:
    return status;
}

Registry::Status Registry::GetNetworkXpansInDomain(const std::string &aDomainName, XpanIdArray &aRet)
{
    NetworkArray     networks;
    Registry::Status status = GetNetworksInDomain(aDomainName, networks);

    if (status == Registry::Status::REG_SUCCESS)
    {
        for (const auto &nwk : networks)
        {
            aRet.push_back(nwk.mXpan);
        }
    }
    return status;
}

Registry::Status Registry::GetNetworksInDomain(const std::string &aDomainName, NetworkArray &aRet)
{
    Registry::Status    status;
    std::vector<Domain> domains;
    Network             nwk{};

    if (aDomainName == ALIAS_THIS)
    {
        Domain  curDom;
        Network curNwk;
        status = GetCurrentNetwork(curNwk);
        if (status == Registry::Status::REG_SUCCESS)
        {
            status = MapStatus(mStorage->Get(curNwk.mDomainId, curDom));
        }
        VerifyOrExit(status == Registry::Status::REG_SUCCESS);
        domains.push_back(curDom);
    }
    else
    {
        Domain dom{EMPTY_ID, aDomainName};
        VerifyOrExit((status = MapStatus(mStorage->Lookup(dom, domains))) == Registry::Status::REG_SUCCESS);
    }
    VerifyOrExit(domains.size() < 2, status = Registry::Status::REG_AMBIGUITY);
    nwk.mDomainId = domains[0].mId;
    status        = MapStatus(mStorage->Lookup(nwk, aRet));
exit:
    return status;
}

Registry::Status Registry::GetAllDomains(DomainArray &aRet)
{
    Registry::Status status;

    status = MapStatus(mStorage->Lookup(Domain{}, aRet));

    return status;
}

Registry::Status Registry::GetDomainsByAliases(const StringArray &aAliases, DomainArray &aRet, StringArray &aUnresolved)
{
    Registry::Status status;
    DomainArray      domains;

    for (const auto &alias : aAliases)
    {
        Domain dom;

        if (alias == ALIAS_THIS)
        {
            Network nwk;
            status = GetCurrentNetwork(nwk);
            if (status == Registry::Status::REG_SUCCESS)
            {
                status = MapStatus(mStorage->Get(nwk.mDomainId, dom));
            }
        }
        else
        {
            DomainArray result;
            Domain      pred;
            pred.mName = alias;
            status     = MapStatus(mStorage->Lookup(pred, result));
            if (status == Registry::Status::REG_SUCCESS)
            {
                if (result.size() == 1)
                {
                    dom = result.front();
                }
                else if (result.size() > 1)
                {
                    aUnresolved.push_back(alias);
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
            aUnresolved.push_back(alias);
        }
    }
    aRet.insert(aRet.end(), domains.begin(), domains.end());
    status = domains.size() > 0 ? Registry::Status::REG_SUCCESS : Registry::Status::REG_NOT_FOUND;
exit:
    return status;
}

Registry::Status Registry::GetAllNetworks(NetworkArray &aRet)
{
    Registry::Status status;

    status = MapStatus(mStorage->Lookup(Network{}, aRet));

    return status;
}

Registry::Status Registry::GetNetworkXpansByAliases(const StringArray &aAliases,
                                                    XpanIdArray &      aRet,
                                                    StringArray &      aUnresolved)
{
    NetworkArray     networks;
    Registry::Status status = GetNetworksByAliases(aAliases, networks, aUnresolved);

    if (status == Registry::Status::REG_SUCCESS)
    {
        for (const auto &nwk : networks)
        {
            aRet.push_back(nwk.mXpan);
        }
    }
    return status;
}

Registry::Status Registry::GetNetworksByAliases(const StringArray &aAliases,
                                                NetworkArray &     aRet,
                                                StringArray &      aUnresolved)
{
    Registry::Status status;
    NetworkArray     networks;

    if (aAliases.size() == 0)
        return Registry::Status::REG_ERROR;

    for (const auto &alias : aAliases)
    {
        if (alias == ALIAS_ALL || alias == ALIAS_OTHER)
        {
            VerifyOrExit(aAliases.size() == 1,
                         status = Registry::Status::REG_ERROR); // Interpreter must have taken care of this
            VerifyOrExit((status = GetAllNetworks(networks)) == Registry::Status::REG_SUCCESS);
            if (alias == ALIAS_OTHER)
            {
                Network nwkThis;
                VerifyOrExit((status = GetCurrentNetwork(nwkThis)) == Registry::Status::REG_SUCCESS);
                auto nwkIter = find_if(networks.begin(), networks.end(),
                                       [&nwkThis](const Network &el) { return nwkThis.mId.mId == el.mId.mId; });
                if (nwkIter != networks.end())
                {
                    networks.erase(nwkIter);
                }
            }
        }
        else if (alias == ALIAS_THIS)
        {
            // Get selected nwk xpan (no selected is an error)
            Network nwkThis;
            status = GetCurrentNetwork(nwkThis);
            if (status == Registry::Status::REG_SUCCESS && nwkThis.mId.mId != EMPTY_ID)
            {
                // Put nwk into networks
                networks.push_back(nwkThis);
            }
            else // failed 'this' must not break the translation flow
            {
                aUnresolved.push_back(alias);
            }
        }
        else
        {
            Network nwk;
            XpanId  xpid;

            status = (xpid.FromHex(alias) == ERROR_NONE) ? Registry::Status::REG_SUCCESS : Registry::Status::REG_ERROR;

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
            else // aUnresolved alias must not break processing
            {
                aUnresolved.push_back(alias);
            }
        }

        // Make results unique
        std::sort(networks.begin(), networks.end(),
                  [](Network const &a, Network const &b) { return a.mXpan < b.mXpan; });
        auto last = std::unique(networks.begin(), networks.end(),
                                [](Network const &a, Network const &b) { return a.mXpan == b.mXpan; });
        networks.erase(last, networks.end());
    }

    aRet.insert(aRet.end(), networks.begin(), networks.end());
    status = networks.size() > 0 ? Registry::Status::REG_SUCCESS : Registry::Status::REG_NOT_FOUND;
exit:
    return status;
}

Registry::Status Registry::ForgetCurrentNetwork()
{
    return SetCurrentNetwork(NetworkId{});
}

Registry::Status Registry::SetCurrentNetwork(const XpanId aXpan)
{
    Network          nwk;
    Registry::Status status;

    VerifyOrExit((status = GetNetworkByXpan(aXpan, nwk)) == Registry::Status::REG_SUCCESS);
    status = SetCurrentNetwork(nwk.mId);
exit:
    return status;
}

Registry::Status Registry::SetCurrentNetwork(const NetworkId &aNetworkId)
{
    assert(mStorage != nullptr);

    return MapStatus(mStorage->CurrentNetworkSet(aNetworkId));
}

Registry::Status Registry::SetCurrentNetwork(const BorderRouter &aBr)
{
    assert(mStorage != nullptr);

    return MapStatus(mStorage->CurrentNetworkSet(aBr.mNetworkId));
}

Registry::Status Registry::GetCurrentNetwork(Network &aRet)
{
    assert(mStorage != nullptr);
    NetworkId networkId;
    if (MapStatus(mStorage->CurrentNetworkGet(networkId)) != Registry::Status::REG_SUCCESS)
    {
        return Registry::Status::REG_ERROR;
    }

    return (networkId.mId == EMPTY_ID) ? (aRet = Network{}, Registry::Status::REG_SUCCESS)
                                       : MapStatus(mStorage->Get(networkId, aRet));
}

Registry::Status Registry::GetCurrentNetworkXpan(XpanId &aRet)
{
    Registry::Status status;
    Network          nwk;
    VerifyOrExit(Registry::Status::REG_SUCCESS == (status = GetCurrentNetwork(nwk)));
    aRet = nwk.mXpan;
exit:
    return status;
}

Registry::Status Registry::LookupOne(const Network &aPred, Network &aRet)
{
    Registry::Status status;
    NetworkArray     networks;
    VerifyOrExit((status = MapStatus(mStorage->Lookup(aPred, networks))) == Registry::Status::REG_SUCCESS);
    VerifyOrExit(networks.size() == 1, status = Registry::Status::REG_AMBIGUITY);
    aRet = networks[0];
exit:
    return status;
}

Registry::Status Registry::GetNetworkByXpan(const XpanId aXpan, Network &aRet)
{
    Network nwk{};
    nwk.mXpan = aXpan;
    return LookupOne(nwk, aRet);
}

Registry::Status Registry::GetNetworkByName(const std::string &aName, Network &aRet)
{
    Network nwk{};
    nwk.mName = aName;
    return LookupOne(nwk, aRet);
}

Registry::Status Registry::GetNetworkByPan(const std::string &aPan, Network &aRet)
{
    Network nwk{};
    nwk.mPan = aPan;
    return LookupOne(nwk, aRet);
}

Registry::Status Registry::GetDomainNameByXpan(const XpanId aXpan, std::string &aName)
{
    Registry::Status status;
    Network          nwk;
    Domain           dom;

    VerifyOrExit(Registry::Status::REG_SUCCESS == (status = GetNetworkByXpan(aXpan, nwk)));
    VerifyOrExit(Registry::Status::REG_SUCCESS == (status = MapStatus(mStorage->Get(nwk.mDomainId, dom))));
    aName = dom.mName;
exit:
    return status;
}

Registry::Status Registry::DeleteBorderRouterById(const BorderRouterId aRouterId)
{
    Registry::Status          status;
    BorderRouter              br;
    Network                   current;
    std::vector<BorderRouter> routers;
    BorderRouter              pred;

    VerifyOrExit((status = MapStatus(mStorage->Get(aRouterId, br))) == Registry::Status::REG_SUCCESS);

    status = GetCurrentNetwork(current);
    if (status == Registry::Status::REG_SUCCESS && (current.mId.mId != EMPTY_ID))
    {
        Network brNetwork;
        // Check we don't delete the last border router in the current network
        VerifyOrExit((status = MapStatus(mStorage->Get(aRouterId, br))) == Registry::Status::REG_SUCCESS);
        VerifyOrExit((status = MapStatus(mStorage->Get(br.mNetworkId, brNetwork))) == Registry::Status::REG_SUCCESS);
        if (brNetwork.mXpan == current.mXpan)
        {
            pred.mNetworkId = brNetwork.mId;
            VerifyOrExit((status = MapStatus(mStorage->Lookup(pred, routers))) == Registry::Status::REG_SUCCESS);
            if (routers.size() <= 1)
            {
                status = Registry::Status::REG_RESTRICTED;
                goto exit;
            }
        }
    }

    status = MapStatus(mStorage->Del(aRouterId));

    if (br.mNetworkId.mId != EMPTY_ID)
    {
        // Was it the last in the network?
        routers.clear();
        pred.mNetworkId = br.mNetworkId;
        status          = MapStatus(mStorage->Lookup(pred, routers));
        if (status == Registry::Status::REG_NOT_FOUND)
        {
            Network nwk;
            VerifyOrExit((status = MapStatus(mStorage->Get(br.mNetworkId, nwk))) == Registry::Status::REG_SUCCESS);
            VerifyOrExit((status = MapStatus(mStorage->Del(br.mNetworkId))) == Registry::Status::REG_SUCCESS);

            VerifyOrExit((status = DropDomainIfEmpty(nwk.mDomainId)) == Registry::Status::REG_SUCCESS);
        }
    }
exit:
    return status;
}

Registry::Status Registry::DeleteBorderRoutersInNetworks(const StringArray &aAliases, StringArray &aUnresolved)
{
    Registry::Status status;
    NetworkArray     nwks;
    Network          current;

    // Check aAliases acceptable
    for (const auto &alias : aAliases)
    {
        if (alias == ALIAS_THIS)
        {
            return Registry::Status::REG_RESTRICTED;
        }
        if (alias == ALIAS_ALL || (alias == ALIAS_OTHER && aAliases.size() > 1))
        {
            return Registry::Status::REG_DATA_INVALID;
        }
    }

    VerifyOrExit((status = GetNetworksByAliases(aAliases, nwks, aUnresolved)) == Registry::Status::REG_SUCCESS);

    // Processing explicit network aAliases
    if (aAliases[0] != ALIAS_OTHER)
    {
        VerifyOrExit((status = GetCurrentNetwork(current)) == Registry::Status::REG_SUCCESS);
    }

    if (current.mId.mId != EMPTY_ID)
    {
        auto found =
            std::find_if(nwks.begin(), nwks.end(), [&](const Network &a) { return a.mId.mId == current.mId.mId; });
        if (found != nwks.end())
        {
            status = Registry::Status::REG_RESTRICTED;
            goto exit;
        }
    }

    for (const auto &nwk : nwks)
    {
        BorderRouterArray brs;
        BorderRouter      br;

        br.mNetworkId = nwk.mId;
        VerifyOrExit((status = MapStatus(mStorage->Lookup(br, brs))) == Registry::Status::REG_SUCCESS);
        for (auto &&br : brs)
        {
            VerifyOrExit((status = DeleteBorderRouterById(br.mId)) == Registry::Status::REG_SUCCESS);
        }

        VerifyOrExit((status = DropDomainIfEmpty(nwk.mDomainId)) == Registry::Status::REG_SUCCESS);
    }
exit:
    return status;
}

Registry::Status Registry::DropDomainIfEmpty(const DomainId &aDomainId)
{
    Registry::Status status = Registry::Status::REG_SUCCESS;

    if (aDomainId.mId != EMPTY_ID)
    {
        Network              npred;
        std::vector<Network> nwks;

        npred.mDomainId = aDomainId;
        status          = MapStatus(mStorage->Lookup(npred, nwks));
        VerifyOrExit(status == Registry::Status::REG_SUCCESS || status == Registry::Status::REG_NOT_FOUND);
        if (nwks.size() == 0)
        {
            // Drop empty domain
            status = MapStatus(mStorage->Del(aDomainId));
        }
    }
exit:
    return status;
}

Registry::Status Registry::DeleteBorderRoutersInDomain(const std::string &aDomainName)
{
    Domain           dom;
    DomainArray      doms;
    Registry::Status status;
    Network          current;
    XpanIdArray      xpans;
    StringArray      aAliases;
    StringArray      aUnresolved;

    dom.mName = aDomainName;
    VerifyOrExit((status = MapStatus(mStorage->Lookup(dom, doms))) == Registry::Status::REG_SUCCESS);
    if (doms.size() != 1)
    {
        status = Registry::Status::REG_ERROR;
        goto exit;
    }

    VerifyOrExit((status = GetCurrentNetwork(current)) == Registry::Status::REG_SUCCESS);
    if (current.mDomainId.mId != EMPTY_ID)
    {
        Domain currentDomain;
        VerifyOrExit((status = MapStatus(mStorage->Get(current.mDomainId, currentDomain))) ==
                     Registry::Status::REG_SUCCESS);

        if (currentDomain.mName == aDomainName)
        {
            status = Registry::Status::REG_RESTRICTED;
            goto exit;
        }
    }

    VerifyOrExit((status = GetNetworkXpansInDomain(aDomainName, xpans)) == Registry::Status::REG_SUCCESS);
    if (xpans.empty())
    {
        // Domain is already empty
        status = MapStatus(mStorage->Del(doms[0].mId));
        goto exit;
    }

    for (auto &&xpan : xpans)
    {
        aAliases.push_back(XpanId(xpan).str());
    }
    VerifyOrExit((status = DeleteBorderRoutersInNetworks(aAliases, aUnresolved)) == Registry::Status::REG_SUCCESS);
    if (!aUnresolved.empty())
    {
        status = Registry::Status::REG_AMBIGUITY;
        goto exit;
    }
    // Domain will be deleted
exit:
    return status;
}

Registry::Status Registry::Update(const Network &nwk)
{
    return MapStatus(mStorage->Update(nwk));
}

} // namespace persistent_storage
} // namespace commissioner
} // namespace ot
