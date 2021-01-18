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

/**
 * Registry operation status
 */
enum registry_status
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
};

typedef std::vector<std::string> StringArray;

/**
 * Registry implementation
 */
class registry
{
public:
    using xpan_id = ot::commissioner::xpan_id;

    /**
     * Registry constructor with provided persistent_storage
     * @see persistent_storage
     *
     * @param[in] strg persistent storage to be used
     */
    registry(persistent_storage *strg);

    /**
     * Registry constructor with default persistent_storage impl
     * @see persistent_storage_json
     *
     * @param[in] name persistent_storage_json file name
     */
    registry(std::string const &name);

    /**
     * Registry destructor
     */
    ~registry();

    /**
     * Opens and prepares registry for work
     */
    registry_status open();

    /**
     * Closes registry
     */
    registry_status close();

    /**
     *  Adds necessary values to the registry.
     *
     * Will create @ref border_router entity for the argument and put it into
     * registry. Will @ref lookup() @ref network
     * and @ref domain entities and even create and add the if necessary.
     *
     * @param[in] val value to be added
     * @return @ref registry_status
     * @see registry_entries.hpp
     */
    registry_status add(BorderAgent const &val);

    /**
     * Get list of all border routers
     * @param[out] ret vector of all border routers
     */
    registry_status get_all_border_routers(BorderRouterArray &ret);

    /**
     * Get border router record by raw id.
     *
     * It is expected that user becomes aware of the border router raw id from
     * 'br list' results.
     */
    registry_status get_border_router(const border_router_id rawid, border_router &br);

    /**
     * Get border routers belonging to the specified network.
     *
     * @param[in] xpan network's XPAN ID
     * @param[out] ret resultant array of @ref border_router records
     */
    registry_status get_border_routers_in_network(const xpan_id xpan, BorderRouterArray &ret);

    /**
     * Get networks of the domain
     *
     * @param[in] dom_name domain name
     * @param[out] ret vector of networks belonging to the domain
     * @note Network records will be appended to the end of the output network vector
     */
    registry_status get_networks_in_domain(const std::string &dom_name, NetworkArray &ret);

    /**
     * Get network XPAN IDs of the domain
     *
     * @param[in] dom_name domain name
     * @param[out] ret vector of network XPAN IDs belonging to the domain
     * @note Network XPAN IDs will be appended to the end of the output vector
     */
    registry_status get_network_xpans_in_domain(const std::string &dom_name, XpanIdArray &ret);

    /**
     * Get list of all domains
     * @param[out] ret vector of all domains
     */
    registry_status get_all_domains(DomainArray &ret);

    /**
     * Get list of domains by list of aliases.
     * @param[out] ret vector of all domains
     * @param[out] unresolved list of aliases failed to resolve
     */
    registry_status get_domains_by_aliases(const StringArray &aliases, DomainArray &ret, StringArray &unresolved);

    /**
     * Get list of all networks
     * @param[out] ret vector of all networks
     */
    registry_status get_all_networks(NetworkArray &ret);

    /**
     * Get list of networks by alias
     *
     * @param[in] alieses list of network aliases
     * @param[out] ret list of networks
     * @param[out] unresolved list of aliases failed to resolve
     */
    registry_status get_networks_by_aliases(const StringArray &aliases, NetworkArray &ret, StringArray &unresolved);

    /**
     * Get list of network XPAN IDs by alias
     *
     * @param[in] alieses list of network aliases
     * @param[out] ret list of network XPAN IDs
     * @param[out] unresolved list of aliases failed to resolve
     */
    registry_status get_network_xpans_by_aliases(const StringArray &aliases, XpanIdArray &ret, StringArray &unresolved);

    /**
     * Set current network.
     */
    registry_status set_current_network(const xpan_id xpan);

    /**
     * Set current network by border router specified.
     */
    registry_status set_current_network(const border_router &br);

    /**
     * Forget current network
     */
    registry_status forget_current_network();
    /**
     * Get current network
     *
     * @param [out] ret current network data
     */
    registry_status get_current_network(network &ret);

    /**
     * Get current network XPAN ID
     *
     * @param [out] ret current network XPAN ID
     */
    registry_status get_current_network_xpan(uint64_t &ret);

    /**
     * Get network with specified extended PAN id
     *
     * @param[in] xpan extended PAN id to lookup
     * @param[out] ret resulting network record
     * @return
     * @li @ref REG_SUCCESS if precisely one network found (resulting network updated)
     * @li @ref REG_NOT_FOUND if no network was found
     * @li @ref REG_DATA_INVALID is more than one network was foud
     * @li @ref REG_ERROR on other errors
     */
    registry_status get_network_by_xpan(const xpan_id xpan, network &ret);

    /**
     * Get network with specified name
     *
     * @param[in] name network name
     * @param[out] ret resulting network record
     * @return
     * @li @ref REG_SUCCESS if precisely one network found (resulting network updated)
     * @li @ref REG_NOT_FOUND if no network was found
     * @li @ref REG_AMBUGUITY is more than one network was foud
     * @li @ref REG_ERROR on other errors
     */
    registry_status get_network_by_name(const std::string &name, network &ret);

    /**
     * Get network with specified PAN id
     *
     * @param[in] pan PAN id to lookup
     * @param[out] ret resulting network record
     * @return
     * @li @ref REG_SUCCESS if precisely one network found (resulting network updated)
     * @li @ref REG_NOT_FOUND if no network was found
     * @li @ref REG_DATA_INVALID is more than one network was foud
     * @li @ref REG_ERROR on other errors
     */
    registry_status get_network_by_pan(const std::string &pan, network &ret);

    /**
     * Get domain name for the network identified by XPAN ID
     *
     * @param[in] xpan XPAN ID to lookup by
     * @param[out] name resulting domain name
     * @return
     * @li @ref RET_SUCCESS if name is found
     * @li @ref REG_NOT_FOUND if no network was found
     * @li @ref REG_AMBUGUITY is more than one network was foud
     * @li @ref REG_ERROR on other errors
     */
    registry_status get_domain_name_by_xpan(const xpan_id xpan, std::string &name);

protected:
    /**
     * Lookup the network
     *
     * @param[in] pred network instance with reference values set
     * @param[out] ret resulting network record
     * @return
     * @li @ref REG_SUCCESS if precisely one network found (resulting network updated)
     * @li @ref REG_NOT_FOUND if no network was found
     * @li @ref REG_DATA_INVALID is more than one network was foud
     * @li @ref REG_ERROR on other errors
     */
    registry_status lookup_one(const network &pred, network &ret);

    /**
     * Set current network.
     */
    registry_status set_current_network(const network_id &nwk_id);

private:
    bool                manage_storage = false;   /**< flag that storage was create outside*/
    persistent_storage *storage        = nullptr; /**< persistent storage */
};

registry *CreateRegistry(const std::string &aFile);

} // namespace persistent_storage
} // namespace commissioner
} // namespace ot

#endif // _REGISTRY_HPP_
