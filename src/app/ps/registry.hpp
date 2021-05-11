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
 *   The file defines registry interface.
 */

#ifndef _REGISTRY_HPP_
#define _REGISTRY_HPP_

#include "persistent_storage.hpp"
#include "registry_entries.hpp"

#include <map>
#include <vector>

#include "app/border_agent.hpp"

namespace ot {
namespace commissioner {
namespace persistent_storage {

typedef std::vector<std::string> StringArray;

/**
 * Registry implementation
 */
class Registry
{
public:
    using xpan_id = ot::commissioner::xpan_id;

    /**
     * Registry operation status
     */
    enum class Status : uint8_t
    {
        REG_SUCCESS,   /**< operation succeeded */
        REG_NOT_FOUND, /**< requested data not found */
        /**
         * param combination invalid or new data conflicts with registry
         * content
         */
        REG_DATA_INVALID,
        REG_AMBIGUITY, /**< lookup result was ambiguous */
        REG_ERROR,     /**< operation failed */
        REG_RESTRICTED /**< operation restricted */
    };

    /**
     * Registry constructor with provided persistent_storage
     * @see persistent_storage
     *
     * @param[in] strg persistent storage to be used
     */
    Registry(PersistentStorage *strg);

    /**
     * Registry constructor with default PersistentStorage impl
     * @see PersistentStorage_json
     *
     * @param[in] name PersistentStorage_json file name
     */
    Registry(std::string const &name);

    /**
     * Registry destructor
     */
    ~Registry();

    /**
     * Opens and prepares registry for work
     */
    Status Open();

    /**
     * Closes registry
     */
    Status Close();

    /**
     *  Adds necessary values to the registry.
     *
     * Will create @ref border_router entity for the argument and put it into
     * registry. Will @ref lookup() @ref network
     * and @ref domain entities and even create and add the if necessary.
     *
     * @param[in] val value to be added
     * @return @ref Status
     * @see registry_entries.hpp
     */
    Status Add(BorderAgent const &val);

    /**
     * Get list of all border routers
     * @param[out] ret vector of all border routers
     */
    Status GetAllBorderRouters(BorderRouterArray &ret);

    /**
     * Get border router record by raw id.
     *
     * It is expected that user becomes aware of the border router raw id from
     * 'br list' results.
     */
    Status GetBorderRouter(const border_router_id rawid, border_router &br);

    /**
     * Get border routers belonging to the specified network.
     *
     * @param[in] xpan network's XPAN ID
     * @param[out] ret resultant array of @ref border_router records
     */
    Status GetBorderRoutersInNetwork(const xpan_id xpan, BorderRouterArray &ret);

    /**
     * Get networks of the domain
     *
     * @param[in] dom_name domain name
     * @param[out] ret vector of networks belonging to the domain
     * @note Network records will be appended to the end of the output network vector
     */
    Status GetNetworksInDomain(const std::string &dom_name, NetworkArray &ret);

    /**
     * Get network XPAN IDs of the domain
     *
     * @param[in] dom_name domain name
     * @param[out] ret vector of network XPAN IDs belonging to the domain
     * @note Network XPAN IDs will be appended to the end of the output vector
     */
    Status GetNetworkXpansInDomain(const std::string &dom_name, XpanIdArray &ret);

    /**
     * Get list of all domains
     * @param[out] ret vector of all domains
     */
    Status GetAllDomains(DomainArray &ret);

    /**
     * Get list of domains by list of aliases.
     * @param[out] ret vector of all domains
     * @param[out] unresolved list of aliases failed to resolve
     */
    Status GetDomainsByAliases(const StringArray &aliases, DomainArray &ret, StringArray &unresolved);

    /**
     * Get list of all networks
     * @param[out] ret vector of all networks
     */
    Status GetAllNetworks(NetworkArray &ret);

    /**
     * Get list of networks by alias
     *
     * @param[in] alieses list of network aliases
     * @param[out] ret list of networks
     * @param[out] unresolved list of aliases failed to resolve
     */
    Status GetNetworksByAliases(const StringArray &aliases, NetworkArray &ret, StringArray &unresolved);

    /**
     * Get list of network XPAN IDs by alias
     *
     * @param[in] alieses list of network aliases
     * @param[out] ret list of network XPAN IDs
     * @param[out] unresolved list of aliases failed to resolve
     */
    Status GetNetworkXpansByAliases(const StringArray &aliases, XpanIdArray &ret, StringArray &unresolved);

    /**
     * Set current network.
     */
    Status SetCurrentNetwork(const xpan_id xpan);

    /**
     * Set current network by border router specified.
     */
    Status SetCurrentNetwork(const border_router &br);

    /**
     * Forget current network
     */
    Status ForgetCurrentNetwork();
    /**
     * Get current network
     *
     * @param [out] ret current network data
     */
    Status GetCurrentNetwork(network &ret);

    /**
     * Get current network XPAN ID
     *
     * @param [out] ret current network XPAN ID
     */
    Status GetCurrentNetworkXpan(uint64_t &ret);

    /**
     * Get network with specified extended PAN id
     *
     * @param[in] xpan extended PAN id to lookup
     * @param[out] ret resulting network record
     * @return
     * @li @ref REG_SUCCESS if precisely one network found (resulting network updated)
     * @li @ref REG_NOT_FOUND if no network was found
     * @li @ref REG_DATA_INVALID is more than one network was found
     * @li @ref REG_ERROR on other errors
     */
    Status GetNetworkByXpan(const xpan_id xpan, network &ret);

    /**
     * Get network with specified name
     *
     * @param[in] name network name
     * @param[out] ret resulting network record
     * @return
     * @li @ref REG_SUCCESS if precisely one network found (resulting network updated)
     * @li @ref REG_NOT_FOUND if no network was found
     * @li @ref REG_AMBUGUITY is more than one network was found
     * @li @ref REG_ERROR on other errors
     */
    Status GetNetworkByName(const std::string &name, network &ret);

    /**
     * Get network with specified PAN id
     *
     * @param[in] pan PAN id to lookup
     * @param[out] ret resulting network record
     * @return
     * @li @ref REG_SUCCESS if precisely one network found (resulting network updated)
     * @li @ref REG_NOT_FOUND if no network was found
     * @li @ref REG_DATA_INVALID is more than one network was found
     * @li @ref REG_ERROR on other errors
     */
    Status GetNetworkByPan(const std::string &pan, network &ret);

    /**
     * Get domain name for the network identified by XPAN ID
     *
     * @param[in] xpan XPAN ID to lookup by
     * @param[out] name resulting domain name
     * @return
     * @li @ref RET_SUCCESS if name is found
     * @li @ref REG_NOT_FOUND if no network was found
     * @li @ref REG_AMBUGUITY is more than one network was found
     * @li @ref REG_ERROR on other errors
     */
    Status GetDomainNameByXpan(const xpan_id xpan, std::string &name);

    /**
     * Remove border router record.
     *
     * Prevents removing the last router in the selected network.
     *
     * @param [in] router_id ID of the border router record to delete
     * @return
     * @li @ref REG_SUCCESS if successfully deleted record
     */
    Status DeleteBorderRouterById(const border_router_id router_id);

    /**
     * Remove border router records corresponding to the network aliases list along with the corresponding networks.
     *
     * Fails if alias corresponds to the selected network or contains it.
     *
     * @param[in] aliases list of network aliases
     * @return
     * @li @ref REG_SUCCESS if all networks were deleted with border routers belonging to them.
     */
    Status DeleteBorderRoutersInNetworks(const StringArray &aliases, StringArray &unresolved);

    Status DeleteBorderRoutersInDomain(const std::string &domain_name);

    /**
     * Update existing network record.
     *
     * @param[in] network record with identifier set.
     *
     * @return
     * @li @ref REG_SUCCESS if update succeeds
     */
    Status Update(const network &nwk);

protected:
    /**
     * Lookup the network
     *
     * @param[in] pred network instance with reference values set
     * @param[out] ret resulting network record
     * @return
     * @li @ref REG_SUCCESS if precisely one network found (resulting network updated)
     * @li @ref REG_NOT_FOUND if no network was found
     * @li @ref REG_DATA_INVALID is more than one network was found
     * @li @ref REG_ERROR on other errors
     */
    Status LookupOne(const network &pred, network &ret);

    /**
     * Set current network.
     */
    Status SetCurrentNetwork(const network_id &nwk_id);

    Status DropDomainIfEmpty(const domain_id &dom_id);

private:
    bool               manage_storage = false;   /**< flag that storage was create outside*/
    PersistentStorage *storage        = nullptr; /**< persistent storage */
};

Registry *CreateRegistry(const std::string &aFile);

} // namespace persistent_storage
} // namespace commissioner
} // namespace ot

#endif // _REGISTRY_HPP_
