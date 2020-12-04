/***************************************************************************
 * TODO: Copyright
 ***************************************************************************
 * @brief Persistent Storage: Json-based implementation
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
class persistent_storage_json : public persistent_storage
{
public:
    /**
     * Constructor
     *
     * @param[in] name file name to be used as storage
     */
    persistent_storage_json(std::string const &fname);

    /**
     * Destructor
     */
    virtual ~persistent_storage_json();

    /**
     * Opens file, loads cache
     *
     * @return ps_status
     * @see ps_status
     */
    ps_status open();

    /**
     * Writes cache to file, closes file
     *
     * @return ps_status
     * @see ps_status
     */
    ps_status close();

    /**
     * Adds value to store
     *
     * @param[in] val value to be added
     * @param[out] ret_id unique id of the inserted value
     * @return ps_status
     * @see ps_status
     * @see registry_entries.hpp
     */
    ps_status add(registrar const &val, registrar_id &ret_id);
    ps_status add(domain const &val, domain_id &ret_id);
    ps_status add(network const &val, network_id &ret_id);
    ps_status add(border_router const &val, border_router_id &ret_id);

    /**
     * Deletes value by unique id from store
     *
     * @param[in] id value's id to be deleted
     * @return ps_status
     * @see ps_status
     * @see registry_entries.hpp
     */
    ps_status del(registrar_id const &id);
    ps_status del(domain_id const &id);
    ps_status del(network_id const &id);
    ps_status del(border_router_id const &id);

    /**
     * Gets value by unique id from store
     *
     * @param[in] id value's unique id
     * @param[out] ret_val value from registry
     * @return ps_status
     * @see ps_status
     * @see registry_entries.hpp
     */
    ps_status get(registrar_id const &id, registrar &ret_val);
    ps_status get(domain_id const &id, domain &ret_val);
    ps_status get(network_id const &id, network &ret_val);
    ps_status get(border_router_id const &id, border_router &ret_val);

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
    ps_status update(registrar const &val);
    ps_status update(domain const &val);
    ps_status update(network const &val);
    ps_status update(border_router const &val);

    /**
     * Looks for a matching values in store
     *
     * Only non-empty val's fields are compared and combined with AND.
     * Provide nullptr to get all the values.
     * Resulting vector is not cleared. Results are appended to the end.
     *
     * @param[in] val value's fields to compare with.
     * @param[out] ret values matched val condition.
     * @return ps_status
     * @see ps_status
     * @see registry_entries.hpp
     */
    ps_status lookup(registrar const *val, std::vector<registrar> &ret);
    ps_status lookup(domain const *val, std::vector<domain> &ret);
    ps_status lookup(network const *val, std::vector<network> &ret);
    ps_status lookup(border_router const *val, std::vector<border_router> &ret);

private:
    std::string            file_name;    /**< name of the file */
    nlohmann::json         cache;        /**< internal chache */
    ot::os::sem::semaphore storage_lock; /**< lock to sync file access */

    /**
     * Reads data from file to cache
     */
    ps_status cache_from_file();

    /**
     * Writes cache to file
     */
    ps_status cache_to_file();

    /**
     * Generates default empty Json structure
     *
     * {rgr_seq:0, dom_seq:0, nwk_seq:0, br_seq:0,
     *  rgr:[], dom:[], nwk:[], br:[]}
     */
    nlohmann::json json_default();

    /**
     * Json structure validation
     */
    bool cache_struct_validation();

    /**
     * Adds all types of values to store
     */
    template <typename V, typename I>
    ps_status add_one(V const &val, I &ret_id, std::string seq_name, std::string arr_name)
    {
        if (cache_from_file() != PS_SUCCESS)
        {
            return PS_ERROR;
        }

        V ins_val = val;
        cache.at(seq_name).get_to(ret_id);
        cache[seq_name] = ret_id.id + 1;

        ins_val.id = ret_id;

        cache[arr_name].push_back(ins_val);

        return cache_to_file();
    }

    /**
     * Deletes all types of values from store
     */
    template <typename V, typename I> ps_status del_id(I const &id, std::string arr_name)
    {
        auto it_end = std::remove_if(std::begin(cache[arr_name]), std::end(cache[arr_name]),
                                     [&id](const V &el) { return el.id.id == id.id; });
        cache[arr_name].erase(it_end, std::end(cache[arr_name]));

        return cache_to_file();
    }

    /**
     * Gets all types of values from store
     */
    template <typename V, typename I> ps_status get_id(I const &id, V &ret, std::string arr_name)
    {
        auto it_val = std::find_if(std::begin(cache[arr_name]), std::end(cache[arr_name]),
                                   [&id](V const &el) { return el.id.id == id.id; });
        if (it_val == std::end(cache[arr_name]))
        {
            return PS_NOT_FOUND;
        }

        ret = *it_val;

        return PS_SUCCESS;
    }

    /**
     * Updates all types of values in store
     */
    template <typename V> ps_status upd_id(V const &new_val, std::string arr_name)
    {
        auto it_val = std::find_if(std::begin(cache[arr_name]), std::end(cache[arr_name]),
                                   [&new_val](const V &el) { return el.id.id == new_val.id.id; });
        if (it_val == std::end(cache[arr_name]))
        {
            return PS_NOT_FOUND;
        }

        *it_val = new_val;

        return cache_to_file();
    }

    /**
     * Looks for a value of any type
     */
    template <typename V>
    ps_status lookup_pred(std::function<bool(V const &)> pred, std::vector<V> &ret, std::string arr_name)
    {
        size_t prev_s = ret.size();
        std::copy_if(std::begin(cache[arr_name]), std::end(cache[arr_name]), std::back_inserter(ret), pred);

        if (ret.size() > prev_s)
        {
            return PS_SUCCESS;
        }

        return PS_NOT_FOUND;
    }
};

} // namespace persistent_storage

} // namespace commissioner

} // namespace ot

#endif // _PERSISTENT_STORAGE_JSON_HPP_
