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
 *   The file defines entities stored with registry.
 */

#ifndef _REGISTRY_ENTRIES_HPP_
#define _REGISTRY_ENTRIES_HPP_

#include <cstdint>
#include <string>
#include <vector>

#include "app/border_agent.hpp"
#include "commissioner/network_data.hpp"
#include "nlohmann/json.hpp"

namespace ot {

namespace commissioner {

namespace persistent_storage {

/**
 * Empty id constant
 *
 * Id value is not set
 */
const unsigned int EMPTY_ID = static_cast<unsigned int>(-1);

/**
 * Registrar entity id
 */
struct RegistrarId
{
    unsigned int mId;

    RegistrarId(unsigned int aValue);
    RegistrarId();
};

/**
 * Domain entity id
 */
struct DomainId
{
    unsigned int mId;

    DomainId(unsigned int aValue);
    DomainId();
};

/**
 * Network entity id
 */
struct NetworkId
{
    unsigned int mId;

    NetworkId(unsigned int aValue);
    NetworkId();
};

/**
 * Border router entity mId
 */
struct BorderRouterId
{
    unsigned int mId;

    BorderRouterId(unsigned int aValue);
    BorderRouterId();
};

/**
 * Registrar entity
 */
struct Registrar
{
    RegistrarId              mId;      /**< unique mId in registry */
    std::string              mAddr;    /**< registrar address */
    unsigned int             mPort;    /**< registrar port */
    std::vector<std::string> mDomains; /**< domains supplied by registrar */

    Registrar(RegistrarId const              &aId,
              std::string const              &aAddr,
              unsigned int                    aPort,
              std::vector<std::string> const &aDomains);
    Registrar();
};

/**
 * Domain entity
 */
struct Domain
{
    DomainId    mId;   /**< unique mId in registry */
    std::string mName; /**< domain name */

    Domain(DomainId const &aId, std::string const &aName);
    Domain();
};

typedef std::vector<Domain> DomainArray;

/**
 * Network entity
 */
struct Network
{
    NetworkId    mId;       /**< unique mId in registry */
    DomainId     mDomainId; /**< reference to the domain the network belongs to */
    std::string  mName;     /**< network name */
    uint64_t     mXpan;     /**< Extended PAN_ID */
    unsigned int mChannel;  /**< network channel */
    uint16_t     mPan;      /**< PAN_ID */
    std::string  mMlp;      /**< Mesh-local prefix */
    int          mCcm;      /**< Commercial commissioning mode;<0 not set,
                             * 0 false, >0 true */

    Network(const NetworkId   &aId,
            const DomainId    &aDomainId,
            const std::string &aName,
            uint64_t           aXpan,
            unsigned int       aChannel,
            uint16_t           aPan,
            const std::string &aMlp,
            int                aCcm);
    Network();
};

typedef std::vector<Network> NetworkArray;

/**
 * Border router entity
 */
struct BorderRouter
{
    BorderRouterId mId;        /**< unique mId in registry */
    NetworkId      mNetworkId; /**< network data reference */
    BorderAgent    mAgent;     /**< border agent descriptive data */

    BorderRouter();
    BorderRouter(BorderRouterId const &aId, NetworkId const &aNetworkId, BorderAgent const &aAgent);
    /**
     * Minimum polymorphic requirements.
     */
    virtual ~BorderRouter() = default;
};

typedef std::vector<BorderRouter> BorderRouterArray;

/**
 * API to support conversion to/from JSON
 */
const std::string JSON_ID               = "id";
const std::string JSON_ADDR             = "addr";
const std::string JSON_PORT             = "port";
const std::string JSON_DOMAINS          = "domains";
const std::string JSON_NAME             = "name";
const std::string JSON_DOMAIN_NAME      = "domain_name";
const std::string JSON_NETWORK_NAME     = "network_name";
const std::string JSON_PAN              = "pan";
const std::string JSON_XPAN             = "xpan";
const std::string JSON_CHANNEL          = "channel";
const std::string JSON_MLP              = "mlp";
const std::string JSON_CCM              = "ccm";
const std::string JSON_THREAD_VERSION   = "thread_version";
const std::string JSON_NETWORK          = "network";
const std::string JSON_NWK_REF          = "nwk_ref";
const std::string JSON_STATE_BITMAP     = "state_bitmap";
const std::string JSON_VENDOR_NAME      = "vendor_name";
const std::string JSON_MODEL_NAME       = "model_name";
const std::string JSON_ACTIVE_TIMESTAMP = "active_timestamp";
const std::string JSON_PARTITION_ID     = "partition_id";
const std::string JSON_VENDOR_DATA      = "vendor_data";
const std::string JSON_VENDOR_OUI       = "vendor_oui";
const std::string JSON_BBR_SEQ_NUMBER   = "bbr_seq_number";
const std::string JSON_BBR_PORT         = "bbr_port";
const std::string JSON_DOM_REF          = "dom_ref";
const std::string JSON_SERVICE_NAME     = "service_name";
const std::string JSON_UPDATE_TIMESTAMP = "update_timestamp";

void to_json(nlohmann::json &aJson, const RegistrarId &aValue);
void from_json(const nlohmann::json &aJson, RegistrarId &aValue);

void to_json(nlohmann::json &aJson, const DomainId &aValue);
void from_json(const nlohmann::json &aJson, DomainId &aValue);

void to_json(nlohmann::json &aJson, const NetworkId &aValue);
void from_json(const nlohmann::json &aJson, NetworkId &aValue);

void to_json(nlohmann::json &aJson, const BorderRouterId &aValue);
void from_json(const nlohmann::json &aJson, BorderRouterId &aValue);

void to_json(nlohmann::json &aJson, const Registrar &aValue);
void from_json(const nlohmann::json &aJson, Registrar &aValue);

void to_json(nlohmann::json &aJson, const Domain &aValue);
void from_json(const nlohmann::json &aJson, Domain &aValue);

void to_json(nlohmann::json &aJson, const Network &aValue);
void from_json(const nlohmann::json &aJson, Network &aValue);

void to_json(nlohmann::json &aJson, const BorderRouter &aValue);
void from_json(const nlohmann::json &aJson, BorderRouter &aValue);

} // namespace persistent_storage

} // namespace commissioner

} // namespace ot

#endif // _REGISTRY_ENTRIES_HPP_
