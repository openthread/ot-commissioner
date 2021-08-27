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
#include "app/border_agent.hpp"

#include <vector>

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
    /**
     * Registry operation status
     */
    enum class Status : uint8_t
    {
        kSuccess,  /**< operation succeeded */
        kNotFound, /**< requested data not found */
        /**
         * param combination invalid or new data conflicts with registry
         * content
         */
        kDataInvalid,
        kAmbiguity,  /**< lookup result was ambiguous */
        kError,      /**< operation failed */
        kRestricted, /**< operation restricted */
    };

    /**
     * Registry constructor with provided PersistentStorage
     * @see PersistentStorage
     *
     * @param[in] aStorage persistent storage to be used
     */
    Registry(PersistentStorage *aStorage);

    /**
     * Registry constructor with default PersistentStorage impl
     * @see PersistentStorageJson
     *
     * @param[in] aName PersistentStorageJson file name
     */
    Registry(std::string const &aName);

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
     * Will create @ref BorderRouter entity for the argument and put it into
     * registry. Will @ref lookup() @ref network
     * and @ref domain entities and even create and add the if necessary.
     *
     * @param[in] aValue value to be added
     * @return @ref Status
     * @see registry_entries.hpp
     */
    Status Add(BorderAgent const &aValue);

    /**
     * Get list of all border routers
     * @param[out] aRetValue vector of all border routers
     */
    Status GetAllBorderRouters(BorderRouterArray &aRetValue);

    /**
     * Get border router record by raw id.
     *
     * It is expected that user becomes aware of the border router raw id from
     * 'br list' results.
     */
    Status GetBorderRouter(const BorderRouterId aRawId, BorderRouter &aBr);

    /**
     * Get border routers belonging to the specified network.
     *
     * @param[in] aXpan network's XPAN ID
     * @param[out] aRetValue resultant array of @ref BorderRouter records
     */
    Status GetBorderRoutersInNetwork(const XpanId aXpan, BorderRouterArray &aRetValue);

    /**
     * Get networks of the domain
     *
     * @param[in] aDomainName domain name
     * @param[out] aRetValue vector of networks belonging to the domain
     * @note Network records will be appended to the end of the output network vector
     */
    Status GetNetworksInDomain(const std::string &aDomainName, NetworkArray &aRetValue);

    /**
     * Get network XPAN IDs of the domain
     *
     * @param[in] aDomainName domain name
     * @param[out] aRetValue vector of network XPAN IDs belonging to the domain
     * @note Network XPAN IDs will be appended to the end of the output vector
     */
    Status GetNetworkXpansInDomain(const std::string &aDomainName, XpanIdArray &aRetValue);

    /**
     * Get list of all domains
     * @param[out] aRetValue vector of all domains
     */
    Status GetAllDomains(DomainArray &aRetValue);

    /**
     * Get list of domains by list of aliases.
     * @param[out] aRetValue vector of all domains
     * @param[out] aUnresolved list of aliases failed to resolve
     */
    Status GetDomainsByAliases(const StringArray &aAliases, DomainArray &aRetValue, StringArray &aUnresolved);

    /**
     * Get list of all networks
     * @param[out] aRetValue vector of all networks
     */
    Status GetAllNetworks(NetworkArray &aRetValue);

    /**
     * Get list of networks by alias
     *
     * @param[in] aAliases list of network aliases
     * @param[out] aRetValue list of networks
     * @param[out] aUnresolved list of aliases failed to resolve
     */
    Status GetNetworksByAliases(const StringArray &aAliases, NetworkArray &aRetValue, StringArray &aUnresolved);

    /**
     * Get list of network XPAN IDs by alias
     *
     * @param[in] aAliases list of network aliases
     * @param[out] aRetValue list of network XPAN IDs
     * @param[out] aUnresolved list of aliases failed to resolve
     */
    Status GetNetworkXpansByAliases(const StringArray &aAliases, XpanIdArray &aRetValue, StringArray &aUnresolved);

    /**
     * Set current network.
     */
    Status SetCurrentNetwork(const XpanId aXpan);

    /**
     * Set current network by border router specified.
     */
    Status SetCurrentNetwork(const BorderRouter &aBr);

    /**
     * Forget current network
     */
    Status ForgetCurrentNetwork();
    /**
     * Get current network
     *
     * @param [out] aRetValue current network data
     */
    Status GetCurrentNetwork(Network &aRetValue);

    /**
     * Get current network XPAN ID
     *
     * @param [out] aRetValue current network XPAN ID
     */
    Status GetCurrentNetworkXpan(XpanId &aRetValue);

    /**
     * Get network with specified extended PAN id
     *
     * @param[in] aXpan extended PAN id to lookup
     * @param[out] aRetValue resulting network record
     * @return
     * @li @ref REG_SUCCESS if precisely one network found (resulting network updated)
     * @li @ref REG_NOT_FOUND if no network was found
     * @li @ref REG_DATA_INVALID is more than one network was found
     * @li @ref REG_ERROR on other errors
     */
    Status GetNetworkByXpan(const XpanId aXpan, Network &aRetValue);

    /**
     * Get network with specified name
     *
     * @param[in] aName network name
     * @param[out] aRetValue resulting network record
     * @return
     * @li @ref REG_SUCCESS if precisely one network found (resulting network updated)
     * @li @ref REG_NOT_FOUND if no network was found
     * @li @ref REG_AMBUGUITY is more than one network was found
     * @li @ref REG_ERROR on other errors
     */
    Status GetNetworkByName(const std::string &aName, Network &aRetValue);

    /**
     * Get network with specified PAN id
     *
     * @param[in] aPan PAN id to lookup
     * @param[out] aRetValue resulting network record
     * @return
     * @li @ref REG_SUCCESS if precisely one network found (resulting network updated)
     * @li @ref REG_NOT_FOUND if no network was found
     * @li @ref REG_DATA_INVALID is more than one network was found
     * @li @ref REG_ERROR on other errors
     */
    Status GetNetworkByPan(const std::string &aPan, Network &aRetValue);

    /**
     * Get domain name for the network identified by XPAN ID
     *
     * @param[in] aXpan XPAN ID to lookup by
     * @param[out] aName resulting domain name
     * @return
     * @li @ref REG_SUCCESS if name is found
     * @li @ref REG_NOT_FOUND if no network was found
     * @li @ref REG_AMBUGUITY is more than one network was found
     * @li @ref REG_ERROR on other errors
     */
    Status GetDomainNameByXpan(const XpanId aXpan, std::string &aName);

    /**
     * Remove border router record.
     *
     * Prevents removing the last router in the selected network.
     *
     * @param [in] aRouterId ID of the border router record to delete
     * @return
     * @li @ref REG_SUCCESS if successfully deleted record
     */
    Status DeleteBorderRouterById(const BorderRouterId aRouterId);

    /**
     * Remove border router records corresponding to the network aliases list along with the corresponding networks.
     *
     * Fails if alias corresponds to the selected network or contains it.
     *
     * @param[in] aliases list of network aliases
     * @return
     * @li @ref REG_SUCCESS if all networks were deleted with border routers belonging to them.
     */
    Status DeleteBorderRoutersInNetworks(const StringArray &aAliases, StringArray &aUnresolved);

    Status DeleteBorderRoutersInDomain(const std::string &aDomainName);

    /**
     * Update existing network record.
     *
     * @param[in] aNetwork network record with identifier set.
     *
     * @return
     * @li @ref REG_SUCCESS if update succeeds
     */
    Status Update(const Network &aNetwork);

protected:
    /**
     * Lookup the network
     *
     * @param[in] aPred network instance with reference values set
     * @param[out] aRetValue resulting network record
     * @return
     * @li @ref REG_SUCCESS if precisely one network found (resulting network updated)
     * @li @ref REG_NOT_FOUND if no network was found
     * @li @ref REG_DATA_INVALID is more than one network was found
     * @li @ref REG_ERROR on other errors
     */
    Status LookupOne(const Network &aPred, Network &aRetValue);

    /**
     * Set current network.
     */
    Status SetCurrentNetwork(const NetworkId &aNetworkId);

    Status DropDomainIfEmpty(const DomainId &aDomainId);

private:
    bool               mManageStorage = false;   /**< flag that storage was create outside*/
    PersistentStorage *mStorage       = nullptr; /**< persistent storage */
};

Registry *CreateRegistry(const std::string &aFile);

} // namespace persistent_storage
} // namespace commissioner
} // namespace ot

#endif // _REGISTRY_HPP_
