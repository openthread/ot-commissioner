/***************************************************************************
 * TODO: Copyright
 ***************************************************************************
 * @brief Data types could be stored in registry
 */

#ifndef _REGISTRY_ENTRIES_HPP_
#define _REGISTRY_ENTRIES_HPP_

#include "nlohmann_json.hpp"

#include <string>
#include <vector>

#include "border_agent.hpp"

namespace ot::commissioner::persistent_storage {

/**
 * Empty id constant
 *
 * Id value is not set
 */
const unsigned int EMPTY_ID = (unsigned int)-1;

/**
 * Registrar entity id
 */
struct registrar_id
{
    unsigned int id;

    registrar_id(unsigned int val)
        : id(val)
    {
    }
    registrar_id()
        : registrar_id(EMPTY_ID)
    {
    }
};

/**
 * Domain entity id
 */
struct domain_id
{
    unsigned int id;

    domain_id(unsigned int val)
        : id(val)
    {
    }
    domain_id()
        : domain_id(EMPTY_ID)
    {
    }
};

/**
 * Network entity id
 */
struct network_id
{
    unsigned int id;

    network_id(unsigned int val)
        : id(val)
    {
    }
    network_id()
        : network_id(EMPTY_ID)
    {
    }
};

/**
 * Border router entity id
 */
struct border_router_id
{
    unsigned int id;

    border_router_id(unsigned int val)
        : id(val)
    {
    }
    border_router_id()
        : border_router_id(EMPTY_ID)
    {
    }
};

/**
 * Registrar entity
 */
struct registrar
{
    registrar_id             id;      /**< unique id in registry */
    std::string              addr;    /**< registrar address */
    unsigned int             port;    /**< registrar port */
    std::vector<std::string> domains; /**< domains supplied by registrar */

    registrar(registrar_id const &            rid,
              std::string const &             raddr,
              unsigned int const              rport,
              std::vector<std::string> const &rdomains)
        : id(rid)
        , addr(raddr)
        , port(rport)
        , domains(rdomains)
    {
    }
    registrar()
        : registrar(EMPTY_ID, "", 0, {})
    {
    }
};

/**
 * Domain entity
 */
struct domain
{
    domain_id                id;       /**< unique id in registry */
    std::string              name;     /**< domain name */
    std::vector<std::string> networks; /**< networks that belong to domain*/

    domain(domain_id const &did, std::string const &dname, std::vector<std::string> const &dnetworks)
        : id(did)
        , name(dname)
        , networks(dnetworks)
    {
    }
    domain()
        : domain(EMPTY_ID, "", {})
    {
    }
};

/**
 * Network entity
 */
struct network
{
    network_id   id;          /**< unique id in registry */
    std::string  name;        /**< network name */
    std::string  domain_name; /**< domain name if network belongs to */
    std::string  xpan;        /**< Extended PAN_ID */
    unsigned int channel;     /**< network channel */
    std::string  pan;         /**< PAN_ID */
    std::string  mlp;         /**< Mesh-local prefix */
    int          ccm;         /**< Commercial commissioning mode;<0 not set,
                               * 0 false, >0 true */

    network(network_id const & nid,
            std::string const &nname,
            std::string const &ndomain_name,
            std::string const &nxpan,
            unsigned int const nchannel,
            std::string const &npan,
            std::string const &nmlp,
            int const          nccm)
        : id(nid)
        , name(nname)
        , domain_name(ndomain_name)
        , xpan(nxpan)
        , channel(nchannel)
        , pan(npan)
        , mlp(nmlp)
        , ccm(nccm)
    {
    }
    network()
        : network(EMPTY_ID, "", "", "", 0, "", "", -1)
    {
    }
};

/**
 * Border router entity
 */
struct border_router : public BorderAgent
{
    border_router_id id;     /**< unique id in registry */
    network_id       nwk_id; /**< network data reference */

    border_router()
        : border_router(EMPTY_ID, EMPTY_ID, {})
    {
    }

    border_router(border_router_id const &bid, network_id const &nid, BorderAgent const &ba)
        : BorderAgent{ba}
        , id{bid}
        , nwk_id{nid}
    {
    }
    /**
     * Minimum polymorphic requirements.
     */
    virtual ~border_router() {}
};

/**
 * API to support conversion to/from JSON
 */
const std::string JSON_ID               = "id";
const std::string JSON_ADDR             = "addr";
const std::string JSON_PORT             = "port";
const std::string JSON_DOMAINS          = "domains";
const std::string JSON_NAME             = "name";
const std::string JSON_NETWORKS         = "networks";
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

void to_json(nlohmann::json &j, const registrar_id &opt);
void from_json(const nlohmann::json &j, registrar_id &opt);

void to_json(nlohmann::json &j, const domain_id &opt);
void from_json(const nlohmann::json &j, domain_id &opt);

void to_json(nlohmann::json &j, const network_id &opt);
void from_json(const nlohmann::json &j, network_id &opt);

void to_json(nlohmann::json &j, const border_router_id &opt);
void from_json(const nlohmann::json &j, border_router_id &opt);

void to_json(nlohmann::json &j, const registrar &p);
void from_json(const nlohmann::json &j, registrar &p);

void to_json(nlohmann::json &j, const domain &p);
void from_json(const nlohmann::json &j, domain &p);

void to_json(nlohmann::json &j, const network &p);
void from_json(const nlohmann::json &j, network &p);

void to_json(nlohmann::json &j, const border_router &p);
void from_json(const nlohmann::json &j, border_router &p);

} // namespace ot::commissioner::persistent_storage

#endif // _REGISTRY_ENTRIES_HPP_
