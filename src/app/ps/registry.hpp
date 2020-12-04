/***************************************************************************
 * TODO: Copyright
 ***************************************************************************
 * @brief Registry interface
 */

#ifndef _REGISTRY_HPP_
#define _REGISTRY_HPP_

#include "persistent_storage.hpp"
#include "registry_entries.hpp"

#include <map>
#include <vector>

#include "border_agent.hpp"

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
    REG_ERROR /**< operation failed */
};

/**
 * Registry implementation
 */
class registry
{
public:
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
    virtual ~registry();

    /**
     * Opens and prepares registry for work
     */
    registry_status open();

    /**
     * Closes registry
     */
    registry_status close();

    /**
     * Adds value to registry
     *
     * @param[in] val value to be added
     * @param[out] ret_id unique id of the inserted value
     * @return registry_status
     * @see registry_status
     * @see registry_entries.hpp
     */
    registry_status add(registrar const &val, registrar_id &ret_id);
    registry_status add(domain const &val, domain_id &ret_id);
    registry_status add(network const &val, network_id &ret_id);
    registry_status add(border_router const &val, border_router_id &ret_id);
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
     * Deletes value by unique id
     *
     * @param[in] id value's id to be deleted
     * @return registry_status
     * @see registry_status
     * @see registry_entries.hpp
     */
    registry_status del(registrar_id const &id);
    registry_status del(domain_id const &id);
    registry_status del(network_id const &id);
    registry_status del(border_router_id const &id);

    /**
     * Gets value by unique id
     *
     * @param[in] id value's unique id
     * @param[out] ret_val value from registry
     * @return registry_status
     * @see registry_status
     * @see registry_entries.hpp
     */
    registry_status get(registrar_id const &id, registrar &ret_val);
    registry_status get(domain_id const &id, domain &ret_val);
    registry_status get(network_id const &id, network &ret_val);
    registry_status get(border_router_id const &id, border_router &ret_val);

    /**
     * Updates value in registry
     *
     * Updated value is identified by val's id.
     * If value was found it is replaced with val. Old value is lost.
     *
     * @param[in] val new value
     * @return registry_status
     * @see registry_status
     * @see registry_entries.hpp
     */
    registry_status update(registrar const &val);
    registry_status update(domain const &val);
    registry_status update(network const &val);
    registry_status update(border_router const &val);

    /**
     * Looks for a matching values in registry
     *
     * Only non-empty val's fields are compared and combined with AND.
     * Provide nullptr to get all the values.
     * Resulting vector is not cleared. Results are appended to the end.
     *
     * @param[in] val value's fields to compare with.
     * @param[out] ret values matched val condition.
     * @return registry_status
     * @see registry_status
     * @see registry_entries.hpp
     */
    registry_status lookup(registrar const *val, std::vector<registrar> &ret);
    registry_status lookup(domain const *val, std::vector<domain> &ret);
    registry_status lookup(network const *val, std::vector<network> &ret);
    registry_status lookup(border_router const *val, std::vector<border_router> &ret);

private:
    bool                manage_storage = false;   /**< flag that storage was create outside*/
    persistent_storage *storage        = nullptr; /**< persistent storage */
};

} // namespace persistent_storage

} // namespace commissioner

} // namespace ot

#endif // _REGISTRY_HPP_
