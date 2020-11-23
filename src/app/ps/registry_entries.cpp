#include "registry_entries.hpp"

using nlohmann::json;

namespace ot::commissioner::persistent_storage {

void to_json(json &j, const registrar_id &opt)
{
    j = opt.id;
}

void from_json(const json &j, registrar_id &opt)
{
    opt.id = j.get<unsigned int>();
}

void to_json(json &j, const domain_id &opt)
{
    j = opt.id;
}

void from_json(const json &j, domain_id &opt)
{
    opt.id = j.get<unsigned int>();
}

void to_json(json &j, const network_id &opt)
{
    j = opt.id;
}

void from_json(const json &j, network_id &opt)
{
    opt.id = j.get<unsigned int>();
}

void to_json(json &j, const border_router_id &opt)
{
    j = opt.id;
}

void from_json(const json &j, border_router_id &opt)
{
    opt.id = j.get<unsigned int>();
}

void to_json(json &j, const registrar &p)
{
    j = json{{JSON_ID, p.id}, {JSON_ADDR, p.addr}, {JSON_PORT, p.port}, {JSON_DOMAINS, p.domains}};
}

void from_json(const json &j, registrar &p)
{
    j.at(JSON_ID).get_to(p.id);
    j.at(JSON_ADDR).get_to(p.addr);
    j.at(JSON_PORT).get_to(p.port);
    j.at(JSON_DOMAINS).get_to(p.domains);
}

void to_json(json &j, const domain &p)
{
    j = json{{JSON_ID, p.id}, {JSON_NAME, p.name}, {JSON_NETWORKS, p.networks}};
}

void from_json(const json &j, domain &p)
{
    j.at(JSON_ID).get_to(p.id);
    j.at(JSON_NAME).get_to(p.name);
    j.at(JSON_NETWORKS).get_to(p.networks);
}

void to_json(json &j, const network &p)
{
    j = json{{JSON_ID, p.id},   {JSON_NAME, p.name}, {JSON_DOMAIN_NAME, p.domain_name},
             {JSON_PAN, p.pan}, {JSON_XPAN, p.xpan}, {JSON_CHANNEL, p.channel},
             {JSON_MLP, p.mlp}, {JSON_CCM, p.ccm}};
}

void from_json(const json &j, network &p)
{
    j.at(JSON_ID).get_to(p.id);
    j.at(JSON_NAME).get_to(p.name);
    j.at(JSON_DOMAIN_NAME).get_to(p.domain_name);
    j.at(JSON_PAN).get_to(p.pan);
    j.at(JSON_XPAN).get_to(p.xpan);
    j.at(JSON_CHANNEL).get_to(p.channel);
    j.at(JSON_MLP).get_to(p.mlp);
    j.at(JSON_CCM).get_to(p.ccm);
}

void to_json(json &j, const border_router &p)
{
    j = json
    {
        {JSON_ID, p.id},
// TODO re-implement
#if 0
     {JSON_THREAD_VERSION, p.thread_version}, {JSON_NWK_REF, p.network}, {JSON_ADDR, p.addr},
        {JSON_PORT, p.port}, {JSON_STATE_BITMAP, p.state_bitmap},     {JSON_ROLE, p.role},
#endif
    };
}

void from_json(const json &j, border_router &p)
{
    j.at(JSON_ID).get_to(p.id);
// TODO re-implement
#if 0
    j.at(JSON_THREAD_VERSION).get_to(p.thread_version);
    j.at(JSON_NWK_REF).get_to(p.network);
    j.at(JSON_ADDR).get_to(p.addr);
    j.at(JSON_PORT).get_to(p.port);
    j.at(JSON_STATE_BITMAP).get_to(p.state_bitmap);
    j.at(JSON_ROLE).get_to(p.role);
#endif
}

} // namespace ot::commissioner::persistent_storage
