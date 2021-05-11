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
 *   The file defines json-based persistent storage.
 */

#ifndef _PERSISTENT_STORAGE_JSON_HPP_
#define _PERSISTENT_STORAGE_JSON_HPP_

#include "persistent_storage.hpp"
#include "semaphore.hpp"

#include "nlohmann_json.hpp"

#include <algorithm>
#include <functional>

namespace ot {

namespace commissioner {

namespace persistent_storage {

/**
 * Implements persistent_storage interface based on single json file
 *
 * Default persistent storage implemetaion used by Registry
 * @see registry.hpp
 * @see registry_entries.hpp
 * @see persistent_storage.hpp
 */
class PersistentStorageJson : public PersistentStorage
{
public:
    /**
     * Constructor
     *
     * @param[in] name file name to be used as storage
     */
    PersistentStorageJson(std::string const &fname);

    /**
     * Destructor
     */
    virtual ~PersistentStorageJson();

    /**
     * Opens file, loads cache
     *
     * @return Status
     * @see Status
     */
    virtual Status Open() override;

    /**
     * Writes cache to file, closes file
     *
     * @return Status
     * @see Status
     */
    virtual Status Close() override;

    /**
     * Adds value to store
     *
     * @param[in] val value to be added
     * @param[out] ret_id unique id of the inserted value
     * @return Status
     * @see Status
     * @see registry_entries.hpp
     */
    virtual Status Add(registrar const &val, registrar_id &ret_id) override;
    virtual Status Add(domain const &val, domain_id &ret_id) override;
    virtual Status Add(network const &val, network_id &ret_id) override;
    virtual Status Add(border_router const &val, border_router_id &ret_id) override;

    /**
     * Deletes value by unique id from store
     *
     * @param[in] id value's id to be deleted
     * @return Status
     * @see Status
     * @see registry_entries.hpp
     */
    virtual Status Del(registrar_id const &id) override;
    virtual Status Del(domain_id const &id) override;
    virtual Status Del(network_id const &id) override;
    virtual Status Del(border_router_id const &id) override;

    /**
     * Gets value by unique id from store
     *
     * @param[in] id value's unique id
     * @param[out] ret_val value from registry
     * @return Status
     * @see Status
     * @see registry_entries.hpp
     */
    virtual Status Get(registrar_id const &id, registrar &ret_val) override;
    virtual Status Get(domain_id const &id, domain &ret_val) override;
    virtual Status Get(network_id const &id, network &ret_val) override;
    virtual Status Get(border_router_id const &id, border_router &ret_val) override;

    /**
     * Updates value in store
     *
     * Updated value is identified by val's id.
     * If value was found it is replaced with val. Old value is lost.
     *
     * @param[in] val new value
     * @return Status
     * @see Status
     * @see registry_entries.hpp
     */
    virtual Status Update(registrar const &val) override;
    virtual Status Update(domain const &val) override;
    virtual Status Update(network const &val) override;
    virtual Status Update(border_router const &val) override;

    /**
     * Looks for a matching values in store
     *
     * Only non-empty val's fields are compared and combined with AND.
     * Provide empty entity to get all the values.
     * Resulting vector is not cleared. Results are appended to the end.
     *
     * @param[in] val value's fields to compare with.
     * @param[out] ret values matched val condition.
     * @return Status
     * @see Status
     * @see registry_entries.hpp
     */
    virtual Status Lookup(registrar const &val, std::vector<registrar> &ret) override;
    virtual Status Lookup(domain const &val, std::vector<domain> &ret) override;
    virtual Status Lookup(network const &val, std::vector<network> &ret) override;
    virtual Status Lookup(border_router const &val, std::vector<border_router> &ret) override;

    /**
     * Looks for a matching values in store
     *
     * Only non-empty val's fields are compared and combined with OR.
     * Provide empty entity to get all the values.
     * Resulting vector is not cleared. Results are appended to the end.
     *
     * @param[in] val value's fields to compare with.
     * @param[out] ret values matched val condition.
     * @return Status
     * @see Status
     * @see registry_entries.hpp
     */
    virtual Status LookupAny(registrar const &val, std::vector<registrar> &ret) override;
    virtual Status LookupAny(domain const &val, std::vector<domain> &ret) override;
    virtual Status LookupAny(network const &val, std::vector<network> &ret) override;
    virtual Status LookupAny(border_router const &val, std::vector<border_router> &ret) override;

    /**
     * Set current network.
     */
    virtual Status CurrentNetworkSet(const network_id &nwk_id) override;
    /**
     * Get current network
     *
     * @param nwk_id[out] current network identifier
     */
    virtual Status CurrentNetworkGet(network_id &nwk_id) override;

private:
    std::string            file_name;    /**< name of the file */
    nlohmann::json         cache;        /**< internal chache */
    ot::os::sem::semaphore storage_lock; /**< lock to sync file access */

    /**
     * Reads data from file to cache
     */
    Status CacheFromFile();

    /**
     * Writes cache to file
     */
    Status CacheToFile();

    /**
     * Generates default empty Json structure
     *
     * {rgr_seq:0, dom_seq:0, nwk_seq:0, br_seq:0,
     *  rgr:[], dom:[], nwk:[], br:[]}
     */
    nlohmann::json JsonDefault();

    /**
     * Json structure validation
     */
    bool CacheStructValidation();

    /**
     * Adds all types of values to store
     */
    template <typename V, typename I> Status AddOne(V const &val, I &ret_id, std::string seq_name, std::string arr_name)
    {
        if (CacheFromFile() != Status::PS_SUCCESS)
        {
            return Status::PS_ERROR;
        }

        V ins_val = val;
        cache.at(seq_name).get_to(ret_id);
        cache[seq_name] = ret_id.id + 1;

        ins_val.id = ret_id;

        cache[arr_name].push_back(ins_val);

        return CacheToFile();
    }

    /**
     * Deletes all types of values from store
     */
    template <typename V, typename I> Status DelId(I const &id, std::string arr_name)
    {
        auto it_end = std::remove_if(std::begin(cache[arr_name]), std::end(cache[arr_name]),
                                     [&id](const V &el) { return el.id.id == id.id; });
        cache[arr_name].erase(it_end, std::end(cache[arr_name]));

        return CacheToFile();
    }

    /**
     * Gets all types of values from store
     */
    template <typename V, typename I> Status GetId(I const &id, V &ret, std::string arr_name)
    {
        auto it_val = std::find_if(std::begin(cache[arr_name]), std::end(cache[arr_name]),
                                   [&id](V const &el) { return el.id.id == id.id; });
        if (it_val == std::end(cache[arr_name]))
        {
            return Status::PS_NOT_FOUND;
        }

        ret = *it_val;

        return Status::PS_SUCCESS;
    }

    /**
     * Updates all types of values in store
     */
    template <typename V> Status UpdId(V const &new_val, std::string arr_name)
    {
        auto it_val = std::find_if(std::begin(cache[arr_name]), std::end(cache[arr_name]),
                                   [&new_val](const V &el) { return el.id.id == new_val.id.id; });
        if (it_val == std::end(cache[arr_name]))
        {
            return Status::PS_NOT_FOUND;
        }

        *it_val = new_val;

        return CacheToFile();
    }

    /**
     * Looks for a value of any type
     */
    template <typename V>
    Status LookupPred(std::function<bool(V const &)> pred, std::vector<V> &ret, std::string arr_name)
    {
        size_t prev_s = ret.size();
        std::copy_if(std::begin(cache[arr_name]), std::end(cache[arr_name]), std::back_inserter(ret), pred);

        if (ret.size() > prev_s)
        {
            return Status::PS_SUCCESS;
        }

        return Status::PS_NOT_FOUND;
    }
};

} // namespace persistent_storage

} // namespace commissioner

} // namespace ot

#endif // _PERSISTENT_STORAGE_JSON_HPP_
