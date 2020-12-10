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
 *   The file defines an abstract persistent storage interface.
 */

#ifndef _PERSISTENT_STORAGE_HPP_
#define _PERSISTENT_STORAGE_HPP_

#include "registry_entries.hpp"

#include <string>
#include <vector>

namespace ot {
namespace commissioner {
namespace persistent_storage {

/**
 * Persistent storage opraton status
 */
enum ps_status
{
    PS_SUCCESS,   /**< operation succeeded */
    PS_NOT_FOUND, /**< data not found */
    PS_ERROR      /**< operation failed */
};

/**
 * Persistent storage interface
 *
 * Assume DB-like interface - all values stored in records.
 * Each record has unique id. Values can be read, updated, deleted by id.
 * Limited lookup functionality by camparing record's fields.
 * @see registry_entries.hpp
 */
class persistent_storage
{
public:
    virtual ~persistent_storage(){};

    /**
     * Prepares storage for work. Called once on start.
     */
    virtual ps_status open() = 0;

    /**
     * Stops storage. Called once on exit.
     */
    virtual ps_status close() = 0;

    /**
     * Adds value to store
     *
     * @param[in] val value to be added
     * @param[out] ret_id unique id of the inserted value
     * @return ps_status
     * @see ps_status
     * @see registry_entries.hpp
     */
    virtual ps_status add(registrar const &val, registrar_id &ret_id)         = 0;
    virtual ps_status add(domain const &val, domain_id &ret_id)               = 0;
    virtual ps_status add(network const &val, network_id &ret_id)             = 0;
    virtual ps_status add(border_router const &val, border_router_id &ret_id) = 0;

    /**
     * Deletes value by unique id from store
     *
     * @param[in] id value's id to be deleted
     * @return ps_status
     * @see ps_status
     * @see registry_entries.hpp
     */
    virtual ps_status del(registrar_id const &id)     = 0;
    virtual ps_status del(domain_id const &id)        = 0;
    virtual ps_status del(network_id const &id)       = 0;
    virtual ps_status del(border_router_id const &id) = 0;

    /**
     * Gets value by unique id from store
     *
     * @param[in] id value's unique id
     * @param[out] ret_val value from registry
     * @return ps_status
     * @see ps_status
     * @see registry_entries.hpp
     */
    virtual ps_status get(registrar_id const &id, registrar &ret_val)         = 0;
    virtual ps_status get(domain_id const &id, domain &ret_val)               = 0;
    virtual ps_status get(network_id const &id, network &ret_val)             = 0;
    virtual ps_status get(border_router_id const &id, border_router &ret_val) = 0;

    /**
     * Updates value in store
     *
     * Updated value is identified by val's id.
     * If value was found it is replaced with val. Old value is lost.
     *
     * @param[in] val new value
     * @return ps_status
     * @see ps_status
     * @see registry_entries.hpp
     */
    virtual ps_status update(registrar const &val)     = 0;
    virtual ps_status update(domain const &val)        = 0;
    virtual ps_status update(network const &val)       = 0;
    virtual ps_status update(border_router const &val) = 0;

    /**
     * Looks for a matching values in store
     *
     * Only non-empty val's fields are compared and combined with AND.
     * Provide empty to get all the values.
     * Resulting vector is not cleared. Results are appended to the end.
     *
     * @param[in] val value's fields to compare with.
     * @param[out] ret values matched val condition.
     * @return ps_status
     * @see ps_status
     * @see registry_entries.hpp
     */
    virtual ps_status lookup(registrar const &val, std::vector<registrar> &ret)         = 0;
    virtual ps_status lookup(domain const &val, std::vector<domain> &ret)               = 0;
    virtual ps_status lookup(network const &val, std::vector<network> &ret)             = 0;
    virtual ps_status lookup(border_router const &val, std::vector<border_router> &ret) = 0;

    /**
     * Looks for a matching values in store
     *
     * Only non-empty val's fields are compared and combined with OR.
     * Provide empty entity to get all the values.
     * Resulting vector is not cleared. Results are appended to the end.
     *
     * @param[in] val value's fields to compare with.
     * @param[out] ret values matched val condition.
     * @return ps_status
     * @see ps_status
     * @see registry_entries.hpp
     */
    virtual ps_status lookup_any(registrar const &val, std::vector<registrar> &ret)         = 0;
    virtual ps_status lookup_any(domain const &val, std::vector<domain> &ret)               = 0;
    virtual ps_status lookup_any(network const &val, std::vector<network> &ret)             = 0;
    virtual ps_status lookup_any(border_router const &val, std::vector<border_router> &ret) = 0;

    /**
     * Set current network.
     */
    virtual ps_status current_network_set(const network_id &nwk_id) = 0;
    /**
     * Get current network
     *
     * @param nwk_id[out] current network identifier
     */
    virtual ps_status current_network_get(network_id &nwk_id) = 0;
};

} // namespace persistent_storage
} // namespace commissioner
} // namespace ot

#endif // _PERSISTENT_STORAGE_HPP_
