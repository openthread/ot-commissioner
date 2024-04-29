/*
 *    Copyright (c) 2020, The OpenThread Commissioner Authors.
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
 *   The file implements JSON-based Thread networks/domains registry.
 */

#include <cassert>
#include <string>

#include "app/border_agent.hpp"
#include "app/ps/persistent_storage.hpp"
#include "app/ps/persistent_storage_json.hpp"
#include "app/ps/registry.hpp"
#include "app/ps/registry_entries.hpp"
#include "commissioner/error.hpp"
#include "commissioner/network_data.hpp"
#include "common/error_macros.hpp"
#include "common/utils.hpp"

namespace ot {
namespace commissioner {
namespace persistent_storage {

namespace {
Registry::Status SuccessStatus(PersistentStorage::Status aStatus)
{
    if (aStatus == PersistentStorage::Status::kSuccess)
    {
        return Registry::Status::kSuccess;
    }
    return Registry::Status::kError;
}

Registry::Status MapStatus(PersistentStorage::Status aStatus)
{
    if (aStatus == PersistentStorage::Status::kError)
    {
        return Registry::Status::kError;
    }
    if (aStatus == PersistentStorage::Status::kSuccess)
    {
        return Registry::Status::kSuccess;
    }
    if (aStatus == PersistentStorage::Status::kNotFound)
    {
        return Registry::Status::kNotFound;
    }
    return Registry::Status::kError;
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
        return Registry::Status::kError;
    }

    return SuccessStatus(mStorage->Open());
}

Registry::Status Registry::Close()
{
    if (mStorage == nullptr)
    {
        return Registry::Status::kError;
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
        if (status == Registry::Status::kError || domains.size() > 1)
        {
            return Registry::Status::kError;
        }

        if (status == Registry::Status::kNotFound)
        {
            DomainId domainId{EMPTY_ID};
            status = MapStatus(mStorage->Add(dom, domainId));
            if (status != Registry::Status::kSuccess)
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
            if (status == Registry::Status::kError || nwks.size() > 1)
            {
                throw status;
            }

            if (status == Registry::Status::kNotFound)
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

                // Provisionally set network's CCM flag considering
                // advertised Connection Mode.
                //
                // Please note that actually see if CCM enabled or not
                // is possible only by respective security policy flag
                // from Active Operational Dataset. Later, if and when
                // successfully petitioned to the network, the flag
                // will be updated by real network dataset.
                nwk.mCcm = ((aValue.mState.mConnectionMode == 4) ? 1 : 0);
                status   = MapStatus(mStorage->Add(nwk, networkId));
                if (status != Registry::Status::kSuccess)
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
                    if (status != Registry::Status::kSuccess)
                    {
                        throw status;
                    }
                }
            }
        }

        BorderRouter br{BorderRouterId{EMPTY_ID}, nwk.mId, aValue};
        try
        {
            if (br.mNetworkId.mId == EMPTY_ID)
            {
                throw Registry::Status::kError;
            }

            // Lookup BorderRouter by address and port to decide to add() or update()
            // Assuming address and port are set (it should be so).
            BorderRouter lookupBr{};
            lookupBr.mAgent.mAddr          = aValue.mAddr;
            lookupBr.mAgent.mExtendedPanId = aValue.mExtendedPanId;
            lookupBr.mAgent.mPresentFlags  = BorderAgent::kAddrBit | BorderAgent::kExtendedPanIdBit;
            std::vector<BorderRouter> routers;
            status = MapStatus(mStorage->Lookup(lookupBr, routers));
            if (status == Registry::Status::kSuccess && routers.size() == 1)
            {
                br.mId.mId = routers[0].mId.mId;
                status     = MapStatus(mStorage->Update(br));
            }
            else if (status == Registry::Status::kNotFound)
            {
                status = MapStatus(mStorage->Add(br, br.mId));
            }

            if (status != Registry::Status::kSuccess)
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

Registry::Status Registry::GetBorderRoutersInNetwork(uint64_t aXpan, BorderRouterArray &aRet)
{
    Network          nwk;
    BorderRouter     pred;
    Registry::Status status = GetNetworkByXpan(aXpan, nwk);

    VerifyOrExit(status == Registry::Status::kSuccess);
    pred.mNetworkId = nwk.mId;
    status          = MapStatus(mStorage->Lookup(pred, aRet));
exit:
    return status;
}

Registry::Status Registry::GetNetworkXpansInDomain(const std::string &aDomainName, std::vector<uint64_t> &aRet)
{
    NetworkArray     networks;
    Registry::Status status = GetNetworksInDomain(aDomainName, networks);

    if (status == Registry::Status::kSuccess)
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
        if (status == Registry::Status::kSuccess)
        {
            status = MapStatus(mStorage->Get(curNwk.mDomainId, curDom));
        }
        VerifyOrExit(status == Registry::Status::kSuccess);
        domains.push_back(curDom);
    }
    else
    {
        Domain dom{DomainId{EMPTY_ID}, aDomainName};
        VerifyOrExit((status = MapStatus(mStorage->Lookup(dom, domains))) == Registry::Status::kSuccess);
    }
    VerifyOrExit(domains.size() < 2, status = Registry::Status::kAmbiguity);
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
            if (status == Registry::Status::kSuccess)
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
            if (status == Registry::Status::kSuccess)
            {
                if (result.size() == 1)
                {
                    dom = result.front();
                }
                else if (result.size() > 1)
                {
                    aUnresolved.push_back(alias);
                    ExitNow(status = Registry::Status::kAmbiguity);
                }
            }
        }
        if (status == Registry::Status::kSuccess)
        {
            domains.push_back(dom);
        }
        else
        {
            aUnresolved.push_back(alias);
        }
    }
    aRet.insert(aRet.end(), domains.begin(), domains.end());
    status = domains.size() > 0 ? Registry::Status::kSuccess : Registry::Status::kNotFound;
exit:
    return status;
}

Registry::Status Registry::GetAllNetworks(NetworkArray &aRet)
{
    Registry::Status status;

    status = MapStatus(mStorage->Lookup(Network{}, aRet));

    return status;
}

Registry::Status Registry::GetNetworkXpansByAliases(const StringArray     &aAliases,
                                                    std::vector<uint64_t> &aRet,
                                                    StringArray           &aUnresolved)
{
    NetworkArray     networks;
    Registry::Status status = GetNetworksByAliases(aAliases, networks, aUnresolved);

    if (status == Registry::Status::kSuccess)
    {
        for (const auto &nwk : networks)
        {
            aRet.push_back(nwk.mXpan);
        }
    }
    return status;
}

Registry::Status Registry::GetNetworksByAliases(const StringArray &aAliases,
                                                NetworkArray      &aRet,
                                                StringArray       &aUnresolved)
{
    Registry::Status status;
    NetworkArray     networks;

    if (aAliases.size() == 0)
        return Registry::Status::kError;

    for (const auto &alias : aAliases)
    {
        if (alias == ALIAS_ALL || alias == ALIAS_OTHER)
        {
            VerifyOrExit(aAliases.size() == 1,
                         status = Registry::Status::kError); // Interpreter must have taken care of this
            VerifyOrExit((status = GetAllNetworks(networks)) == Registry::Status::kSuccess);
            if (alias == ALIAS_OTHER)
            {
                Network nwkThis;
                VerifyOrExit((status = GetCurrentNetwork(nwkThis)) == Registry::Status::kSuccess);
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
            if (status == Registry::Status::kSuccess && nwkThis.mId.mId != EMPTY_ID)
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
            Network  nwk;
            uint64_t xpid;
            uint16_t pid;

            status = Registry::Status::kNotFound;
            if (utils::ParseInteger(xpid, alias) == ERROR_NONE)
            {
                status = GetNetworkByXpan(xpid, nwk);
            }

            if (status != Registry::Status::kSuccess)
            {
                status = GetNetworkByName(alias, nwk);
            }

            if (status != Registry::Status::kSuccess)
            {
                bool        hasHexPrefix = utils::ToLower(alias.substr(0, 2)) != "0x";
                std::string aliasAsPanId = std::string(hasHexPrefix ? "0x" : "") + alias;

                if (utils::ParseInteger(pid, aliasAsPanId) == ERROR_NONE)
                {
                    status = GetNetworkByPan(aliasAsPanId, nwk);
                }
            }

            if (status == Registry::Status::kSuccess)
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
    if (networks.size() > 0)
    {
        status = Registry::Status::kSuccess;
    }
exit:
    return status;
}

Registry::Status Registry::ForgetCurrentNetwork()
{
    return SetCurrentNetwork(NetworkId{});
}

Registry::Status Registry::SetCurrentNetwork(uint64_t aXpan)
{
    Network          nwk;
    Registry::Status status;

    VerifyOrExit((status = GetNetworkByXpan(aXpan, nwk)) == Registry::Status::kSuccess);
    status = SetCurrentNetwork(nwk.mId);
exit:
    return status;
}

Registry::Status Registry::SetCurrentNetwork(const NetworkId &aNetworkId)
{
    assert(mStorage != nullptr);

    return MapStatus(mStorage->SetCurrentNetwork(aNetworkId));
}

Registry::Status Registry::SetCurrentNetwork(const BorderRouter &aBr)
{
    assert(mStorage != nullptr);

    return MapStatus(mStorage->SetCurrentNetwork(aBr.mNetworkId));
}

Registry::Status Registry::GetCurrentNetwork(Network &aRet)
{
    assert(mStorage != nullptr);
    NetworkId networkId;
    if (MapStatus(mStorage->GetCurrentNetwork(networkId)) != Registry::Status::kSuccess)
    {
        return Registry::Status::kError;
    }

    return (networkId.mId == EMPTY_ID) ? (aRet = Network{}, Registry::Status::kSuccess)
                                       : MapStatus(mStorage->Get(networkId, aRet));
}

Registry::Status Registry::GetCurrentNetworkXpan(uint64_t &aRet)
{
    Registry::Status status;
    Network          nwk;
    VerifyOrExit(Registry::Status::kSuccess == (status = GetCurrentNetwork(nwk)));
    aRet = nwk.mXpan;
exit:
    return status;
}

Registry::Status Registry::LookupOne(const Network &aPred, Network &aRet)
{
    Registry::Status status;
    NetworkArray     networks;
    VerifyOrExit((status = MapStatus(mStorage->Lookup(aPred, networks))) == Registry::Status::kSuccess);
    VerifyOrExit(networks.size() == 1, status = Registry::Status::kAmbiguity);
    aRet = networks[0];
exit:
    return status;
}

Registry::Status Registry::GetNetworkByXpan(uint64_t aXpan, Network &aRet)
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
    Network  nwk{};
    uint16_t panId = 0;

    if (utils::ParseInteger(panId, aPan).GetCode() != ErrorCode::kNone)
    {
        return Registry::Status::kError;
    }
    nwk.mPan = panId;
    return LookupOne(nwk, aRet);
}

Registry::Status Registry::GetDomainNameByXpan(uint64_t aXpan, std::string &aName)
{
    Registry::Status status;
    Network          nwk;
    Domain           dom;

    VerifyOrExit(Registry::Status::kSuccess == (status = GetNetworkByXpan(aXpan, nwk)));
    VerifyOrExit(Registry::Status::kSuccess == (status = MapStatus(mStorage->Get(nwk.mDomainId, dom))));
    aName = dom.mName;
exit:
    return status;
}

Registry::Status Registry::DeleteBorderRouterById(const BorderRouterId &aRouterId)
{
    Registry::Status          status;
    BorderRouter              br;
    Network                   current;
    std::vector<BorderRouter> routers;
    BorderRouter              pred;

    VerifyOrExit((status = MapStatus(mStorage->Get(aRouterId, br))) == Registry::Status::kSuccess);

    status = GetCurrentNetwork(current);
    if (status == Registry::Status::kSuccess && (current.mId.mId != EMPTY_ID))
    {
        Network brNetwork;
        // Check we don't delete the last border router in the current network
        VerifyOrExit((status = MapStatus(mStorage->Get(aRouterId, br))) == Registry::Status::kSuccess);
        VerifyOrExit((status = MapStatus(mStorage->Get(br.mNetworkId, brNetwork))) == Registry::Status::kSuccess);
        if (brNetwork.mXpan == current.mXpan)
        {
            pred.mNetworkId = brNetwork.mId;
            VerifyOrExit((status = MapStatus(mStorage->Lookup(pred, routers))) == Registry::Status::kSuccess);
            VerifyOrExit(routers.size() > 1, status = Registry::Status::kRestricted);
        }
    }

    status = MapStatus(mStorage->Del(aRouterId));

    if (br.mNetworkId.mId != EMPTY_ID)
    {
        // Was it the last in the network?
        routers.clear();
        pred.mNetworkId = br.mNetworkId;
        status          = MapStatus(mStorage->Lookup(pred, routers));
        if (status == Registry::Status::kNotFound)
        {
            Network nwk;
            VerifyOrExit((status = MapStatus(mStorage->Get(br.mNetworkId, nwk))) == Registry::Status::kSuccess);
            VerifyOrExit((status = MapStatus(mStorage->Del(br.mNetworkId))) == Registry::Status::kSuccess);

            VerifyOrExit((status = DropDomainIfEmpty(nwk.mDomainId)) == Registry::Status::kSuccess);
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
            return Registry::Status::kRestricted;
        }
        if (alias == ALIAS_ALL || (alias == ALIAS_OTHER && aAliases.size() > 1))
        {
            return Registry::Status::kDataInvalid;
        }
    }

    VerifyOrExit((status = GetNetworksByAliases(aAliases, nwks, aUnresolved)) == Registry::Status::kSuccess);

    // Processing explicit network aAliases
    if (aAliases[0] != ALIAS_OTHER)
    {
        VerifyOrExit((status = GetCurrentNetwork(current)) == Registry::Status::kSuccess);
    }

    if (current.mId.mId != EMPTY_ID)
    {
        auto found =
            std::find_if(nwks.begin(), nwks.end(), [&](const Network &a) { return a.mId.mId == current.mId.mId; });
        VerifyOrExit(found == nwks.end(), status = Registry::Status::kRestricted);
    }

    for (const auto &nwk : nwks)
    {
        BorderRouterArray brs;
        BorderRouter      br;

        br.mNetworkId = nwk.mId;
        VerifyOrExit((status = MapStatus(mStorage->Lookup(br, brs))) == Registry::Status::kSuccess);
        for (auto &&br : brs)
        {
            VerifyOrExit((status = DeleteBorderRouterById(br.mId)) == Registry::Status::kSuccess);
        }

        VerifyOrExit((status = DropDomainIfEmpty(nwk.mDomainId)) == Registry::Status::kSuccess);
    }
exit:
    return status;
}

Registry::Status Registry::DropDomainIfEmpty(const DomainId &aDomainId)
{
    Registry::Status status = Registry::Status::kSuccess;

    if (aDomainId.mId != EMPTY_ID)
    {
        Network              npred;
        std::vector<Network> nwks;

        npred.mDomainId = aDomainId;
        status          = MapStatus(mStorage->Lookup(npred, nwks));
        VerifyOrExit(status == Registry::Status::kSuccess || status == Registry::Status::kNotFound);
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
    Domain                dom;
    DomainArray           doms;
    Registry::Status      status;
    Network               current;
    std::vector<uint64_t> xpans;
    StringArray           aAliases;
    StringArray           aUnresolved;

    dom.mName = aDomainName;
    VerifyOrExit((status = MapStatus(mStorage->Lookup(dom, doms))) == Registry::Status::kSuccess);
    VerifyOrExit(doms.size() == 1, status = Registry::Status::kError);

    VerifyOrExit((status = GetCurrentNetwork(current)) == Registry::Status::kSuccess);
    if (current.mDomainId.mId != EMPTY_ID)
    {
        Domain currentDomain;
        VerifyOrExit((status = MapStatus(mStorage->Get(current.mDomainId, currentDomain))) ==
                     Registry::Status::kSuccess);
        VerifyOrExit(currentDomain.mName != aDomainName, status = Registry::Status::kRestricted);
    }

    VerifyOrExit((status = GetNetworkXpansInDomain(aDomainName, xpans)) == Registry::Status::kSuccess);
    VerifyOrExit(!xpans.empty(), status = MapStatus(mStorage->Del(doms[0].mId)));

    for (auto &&xpan : xpans)
    {
        aAliases.push_back(utils::Hex(xpan));
    }
    VerifyOrExit((status = DeleteBorderRoutersInNetworks(aAliases, aUnresolved)) == Registry::Status::kSuccess);
    VerifyOrExit(aUnresolved.empty(), status = Registry::Status::kAmbiguity);
    // Domain will be deleted
exit:
    return status;
}

Registry::Status Registry::Update(const Network &aNetwork)
{
    return MapStatus(mStorage->Update(aNetwork));
}

} // namespace persistent_storage
} // namespace commissioner
} // namespace ot
