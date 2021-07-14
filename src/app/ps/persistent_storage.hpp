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
 * Persistent storage interface
 *
 * Assume DB-like interface - all values stored in records.
 * Each record has unique id. Values can be read, updated, deleted by id.
 * Limited lookup functionality by camparing record's fields.
 * @see registry_entries.hpp
 */
class PersistentStorage
{
public:
    /**
     * Persistent storage opraton status
     */
    enum class Status : uint8_t
    {
        PS_SUCCESS,   /**< operation succeeded */
        PS_NOT_FOUND, /**< data not found */
        PS_ERROR      /**< operation failed */
    };

    virtual ~PersistentStorage(){};

    /**
     * Prepares storage for work. Called once on start.
     */
    virtual Status Open() = 0;

    /**
     * Stops storage. Called once on exit.
     */
    virtual Status Close() = 0;

    /**
     * Adds value to store
     *
     * @param[in] aValue value to be added
     * @param[out] aReturnId unique id of the inserted value
     * @return Status
     * @see Status
     * @see registry_entries.hpp
     */
    virtual Status Add(Registrar const &aValue, RegistrarId &aReturnId)       = 0;
    virtual Status Add(Domain const &aValue, DomainId &aReturnId)             = 0;
    virtual Status Add(Network const &aValue, NetworkId &aReturnId)           = 0;
    virtual Status Add(BorderRouter const &aValue, BorderRouterId &aReturnId) = 0;

    /**
     * Deletes value by unique id from store
     *
     * @param[in] aId value's id to be deleted
     * @return Status
     * @see Status
     * @see registry_entries.hpp
     */
    virtual Status Del(RegistrarId const &aId)    = 0;
    virtual Status Del(DomainId const &aId)       = 0;
    virtual Status Del(NetworkId const &aId)      = 0;
    virtual Status Del(BorderRouterId const &aId) = 0;

    /**
     * Gets value by unique id from store
     *
     * @param[in] aId value's unique id
     * @param[out] aRetValue value from registry
     * @return Status
     * @see Status
     * @see registry_entries.hpp
     */
    virtual Status Get(RegistrarId const &aId, Registrar &aRetValue)       = 0;
    virtual Status Get(DomainId const &aId, Domain &aRetValue)             = 0;
    virtual Status Get(NetworkId const &aId, Network &aRetValue)           = 0;
    virtual Status Get(BorderRouterId const &aId, BorderRouter &aRetValue) = 0;

    /**
     * Updates value in store
     *
     * Updated value is identified by aValue's id.
     * If value was found it is replaced with aValue. Old value is lost.
     *
     * @param[in] aValue new value
     * @return Status
     * @see Status
     * @see registry_entries.hpp
     */
    virtual Status Update(Registrar const &aValue)    = 0;
    virtual Status Update(Domain const &aValue)       = 0;
    virtual Status Update(Network const &aValue)      = 0;
    virtual Status Update(BorderRouter const &aValue) = 0;

    /**
     * Looks for a matching values in store
     *
     * Only non-empty aValue's fields are compared and combined with AND.
     * Provide empty to get all the values.
     * Resulting vector is not cleared. Results are appended to the end.
     *
     * @param[in] aValue value's fields to compare with.
     * @param[out] aRetValue values matched aValue condition.
     * @return Status
     * @see Status
     * @see registry_entries.hpp
     */
    virtual Status Lookup(Registrar const &aValue, std::vector<Registrar> &aRetValue)       = 0;
    virtual Status Lookup(Domain const &aValue, std::vector<Domain> &aRetValue)             = 0;
    virtual Status Lookup(Network const &aValue, std::vector<Network> &aRetValue)           = 0;
    virtual Status Lookup(BorderRouter const &aValue, std::vector<BorderRouter> &aRetValue) = 0;

    /**
     * Looks for a matching values in store
     *
     * Only non-empty aValue's fields are compared and combined with OR.
     * Provide empty entity to get all the values.
     * Resulting vector is not cleared. Results are appended to the end.
     *
     * @param[in] aValue value's fields to compare with.
     * @param[out] aRetValue values matched aValue condition.
     * @return Status
     * @see Status
     * @see registry_entries.hpp
     */
    virtual Status LookupAny(Registrar const &aValue, std::vector<Registrar> &aRetValue)       = 0;
    virtual Status LookupAny(Domain const &aValue, std::vector<Domain> &aRetValue)             = 0;
    virtual Status LookupAny(Network const &aValue, std::vector<Network> &aRetValue)           = 0;
    virtual Status LookupAny(BorderRouter const &aValue, std::vector<BorderRouter> &aRetValue) = 0;

    /**
     * Set current Network.
     */
    virtual Status SetCurrentNetwork(const NetworkId &aNetworkId) = 0;
    /**
     * Get current Network
     *
     * @param aNetworkId[out] current Network identifier
     */
    virtual Status GetCurrentNetwork(NetworkId &aNetworkId) = 0;
};

} // namespace persistent_storage
} // namespace commissioner
} // namespace ot

#endif // _PERSISTENT_STORAGE_HPP_
