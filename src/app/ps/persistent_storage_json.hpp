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

#include <cstddef>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

#include "app/ps/persistent_storage.hpp"
#include "app/ps/registry_entries.hpp"
#include "app/ps/semaphore_posix.hpp"
#include "nlohmann/json.hpp"

namespace ot {

namespace commissioner {

namespace persistent_storage {

/**
 * Implements PersistentStorage interface based on single json file
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
     * @param[in] aFileName file name to be used as storage
     */
    explicit PersistentStorageJson(std::string const &aFileName);

    /**
     * Destructor
     */
    ~PersistentStorageJson() override;

    /**
     * Opens file, loads cache
     *
     * @return Status
     * @see Status
     */
    Status Open() override;

    /**
     * Writes cache to file, closes file
     *
     * @return Status
     * @see Status
     */
    Status Close() override;

    /**
     * Adds value to store
     *
     * @param[in] aValue value to be added
     * @param[out] aRetId unique id of the inserted value
     * @return Status
     * @see Status
     * @see registry_entries.hpp
     */
    Status Add(Registrar const &aValue, RegistrarId &aRetId) override;
    Status Add(Domain const &aValue, DomainId &aRetId) override;
    Status Add(Network const &aValue, NetworkId &aRetId) override;
    Status Add(BorderRouter const &aValue, BorderRouterId &aRetId) override;

    /**
     * Deletes value by unique id from store
     *
     * @param[in] aId value's id to be deleted
     * @return Status
     * @see Status
     * @see registry_entries.hpp
     */
    Status Del(RegistrarId const &aId) override;
    Status Del(DomainId const &aId) override;
    Status Del(NetworkId const &aId) override;
    Status Del(BorderRouterId const &aId) override;

    /**
     * Gets value by unique id from store
     *
     * @param[in] aId value's unique id
     * @param[out] aRetValue value from registry
     * @return Status
     * @see Status
     * @see registry_entries.hpp
     */
    Status Get(RegistrarId const &aId, Registrar &aRetValue) override;
    Status Get(DomainId const &aId, Domain &aRetValue) override;
    Status Get(NetworkId const &aId, Network &aRetValue) override;
    Status Get(BorderRouterId const &aId, BorderRouter &aRetValue) override;

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
    Status Update(Registrar const &aValue) override;
    Status Update(Domain const &aValue) override;
    Status Update(Network const &aValue) override;
    Status Update(BorderRouter const &aValue) override;

    /**
     * Looks for a matching values in store
     *
     * Only non-empty aValue's fields are compared and combined with AND.
     * Provide empty entity to get all the values.
     * Resulting vector is not cleared. Results are appended to the end.
     *
     * @param[in] aValue value's fields to compare with.
     * @param[out] aRet values matched aValue condition.
     * @return Status
     * @see Status
     * @see registry_entries.hpp
     */
    Status Lookup(Registrar const &aValue, std::vector<Registrar> &aRet) override;
    Status Lookup(Domain const &aValue, std::vector<Domain> &aRet) override;
    Status Lookup(Network const &aValue, std::vector<Network> &aRet) override;
    Status Lookup(BorderRouter const &aValue, std::vector<BorderRouter> &aRet) override;

    /**
     * Looks for a matching values in store
     *
     * Only non-empty aValue's fields are compared and combined with OR.
     * Provide empty entity to get all the values.
     * Resulting vector is not cleared. Results are appended to the end.
     *
     * @param[in] aValue value's fields to compare with.
     * @param[out] aRet values matched aValue condition.
     * @return Status
     * @see Status
     * @see registry_entries.hpp
     */
    Status LookupAny(Registrar const &aValue, std::vector<Registrar> &aRet) override;
    Status LookupAny(Domain const &aValue, std::vector<Domain> &aRet) override;
    Status LookupAny(Network const &aValue, std::vector<Network> &aRet) override;
    Status LookupAny(BorderRouter const &aValue, std::vector<BorderRouter> &aRet) override;

    /**
     * Set current Network.
     */
    Status SetCurrentNetwork(const NetworkId &aNwkId) override;
    /**
     * Get current Network
     *
     * @param aNwkId[out] current Network identifier
     */
    Status GetCurrentNetwork(NetworkId &aNwkId) override;

private:
    std::string                  mFileName;    /**< name of the file */
    nlohmann::json               mCache;       /**< internal chache */
    ot::os::semaphore::Semaphore mStorageLock; /**< lock to sync file access */

    /**
     * Reads data from file to mCache
     */
    Status CacheFromFile();

    /**
     * Writes mCache to file
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
    template <typename V, typename I>
    Status AddOne(V const &aValue, I &aRetId, std::string aSeqName, std::string aArrName)
    {
        if (CacheFromFile() != Status::kSuccess)
        {
            return Status::kError;
        }

        V insValue = aValue;
        mCache.at(aSeqName).get_to(aRetId);
        mCache[aSeqName] = aRetId.mId + 1;

        insValue.mId = aRetId;

        mCache[aArrName].push_back(insValue);

        return CacheToFile();
    }

    /**
     * Deletes all types of values from store
     */
    template <typename V, typename I> Status DelId(I const &aId, std::string aArrName)
    {
        auto itEnd = std::remove_if(std::begin(mCache[aArrName]), std::end(mCache[aArrName]),
                                    [&aId](const V &el) { return el.mId.mId == aId.mId; });
        mCache[aArrName].erase(itEnd, std::end(mCache[aArrName]));

        return CacheToFile();
    }

    /**
     * Gets all types of values from store
     */
    template <typename V, typename I> Status GetId(I const &aId, V &aRet, std::string aArrName)
    {
        auto itValue = std::find_if(std::begin(mCache[aArrName]), std::end(mCache[aArrName]),
                                    [&aId](V const &el) { return el.mId.mId == aId.mId; });
        if (itValue == std::end(mCache[aArrName]))
        {
            return Status::kNotFound;
        }

        aRet = *itValue;

        return Status::kSuccess;
    }

    /**
     * Updates all types of values in store
     */
    template <typename V> Status UpdId(V const &aNewValue, std::string aArrName)
    {
        auto itValue = std::find_if(std::begin(mCache[aArrName]), std::end(mCache[aArrName]),
                                    [&aNewValue](const V &el) { return el.mId.mId == aNewValue.mId.mId; });
        if (itValue == std::end(mCache[aArrName]))
        {
            return Status::kNotFound;
        }

        *itValue = aNewValue;

        return CacheToFile();
    }

    /**
     * Looks for a value of any type
     */
    template <typename V>
    Status LookupPred(std::function<bool(V const &)> aPred, std::vector<V> &aRet, std::string aArrName)
    {
        size_t prevSize = aRet.size();
        std::copy_if(std::begin(mCache[aArrName]), std::end(mCache[aArrName]), std::back_inserter(aRet), aPred);

        if (aRet.size() > prevSize)
        {
            return Status::kSuccess;
        }

        return Status::kNotFound;
    }
};

} // namespace persistent_storage

} // namespace commissioner

} // namespace ot

#endif // _PERSISTENT_STORAGE_JSON_HPP_
