#include "registry_entries.hpp"

#include "common/utils.hpp"

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
    j               = json{};
    j[JSON_ID]      = p.id;
    j[JSON_NWK_REF] = p.nwk_id;
    j[JSON_DOM_REF] = p.dom_id;
    if (p.mPresentFlags & BorderAgent::kAddrBit)
    {
        j[JSON_ADDR] = p.mAddr;
    }
    if (p.mPresentFlags & BorderAgent::kPortBit)
    {
        j[JSON_PORT] = p.mPort;
    }
    if (p.mPresentFlags & BorderAgent::kThreadVersionBit)
    {
        j[JSON_THREAD_VERSION] = p.mThreadVersion;
    }
    if (p.mPresentFlags & BorderAgent::kStateBit)
    {
        uint32_t value       = p.mState;
        j[JSON_STATE_BITMAP] = value;
    }
    if (p.mPresentFlags & BorderAgent::kVendorNameBit)
    {
        j[JSON_VENDOR_NAME] = p.mVendorName;
    }
    if (p.mPresentFlags & BorderAgent::kModelNameBit)
    {
        j[JSON_MODEL_NAME] = p.mModelName;
    }
    if (p.mPresentFlags & BorderAgent::kActiveTimestampBit)
    {
        j[JSON_ACTIVE_TIMESTAMP] = p.mActiveTimestamp.Encode();
    }
    if (p.mPresentFlags & BorderAgent::kPartitionIdBit)
    {
        j[JSON_PARTITION_ID] = p.mPartitionId;
    }
    if (p.mPresentFlags & BorderAgent::kVendorDataBit)
    {
        j[JSON_VENDOR_DATA] = p.mVendorData;
    }
    if (p.mPresentFlags & BorderAgent::kVendorOuiBit)
    {
        j[JSON_VENDOR_OUI] = ::ot::commissioner::utils::Hex(p.mVendorOui);
    }
    if (p.mPresentFlags & BorderAgent::kBbrSeqNumberBit)
    {
        j[JSON_BBR_SEQ_NUMBER] = p.mBbrSeqNumber;
    }
    if (p.mPresentFlags & BorderAgent::kBbrPortBit)
    {
        j[JSON_BBR_PORT] = p.mBbrPort;
    }
}

void from_json(const json &j, border_router &p)
{
    j.at(JSON_ID).get_to(p.id);
    p.mPresentFlags = 0;
    if (j.contains(JSON_ADDR))
    {
        j.at(JSON_ADDR).get_to(p.mAddr);
        p.mPresentFlags |= BorderAgent::kAddrBit;
    }
    if (j.contains(JSON_PORT))
    {
        j.at(JSON_PORT).get_to(p.mPort);
        p.mPresentFlags |= BorderAgent::kPortBit;
    }
    if (j.contains(JSON_THREAD_VERSION))
    {
        j.at(JSON_THREAD_VERSION).get_to(p.mThreadVersion);
        p.mPresentFlags |= BorderAgent::kThreadVersionBit;
    }
    if (j.contains(JSON_STATE_BITMAP))
    {
        uint32_t value;
        j.at(JSON_STATE_BITMAP).get_to(value);
        p.mState = value;
        p.mPresentFlags |= BorderAgent::kStateBit;
    }
    if (j.contains(JSON_NWK_REF))
    {
        j.at(JSON_NWK_REF).get_to(p.nwk_id);
    }
    if (j.contains(JSON_VENDOR_NAME))
    {
        j.at(JSON_VENDOR_NAME).get_to(p.mVendorName);
        p.mPresentFlags |= BorderAgent::kVendorNameBit;
    }
    if (j.contains(JSON_MODEL_NAME))
    {
        j.at(JSON_MODEL_NAME).get_to(p.mModelName);
        p.mPresentFlags |= BorderAgent::kModelNameBit;
    }
    if (j.contains(JSON_ACTIVE_TIMESTAMP))
    {
        uint64_t value;
        j.at(JSON_ACTIVE_TIMESTAMP).get_to(value);
        p.mActiveTimestamp.Decode(value);
        p.mPresentFlags |= BorderAgent::kActiveTimestampBit;
    }
    if (j.contains(JSON_PARTITION_ID))
    {
        j.at(JSON_PARTITION_ID).get_to(p.mPartitionId);
        p.mPresentFlags |= BorderAgent::kPartitionIdBit;
    }
    if (j.contains(JSON_VENDOR_DATA))
    {
        j.at(JSON_VENDOR_DATA).get_to(p.mVendorData);
        p.mPresentFlags |= BorderAgent::kVendorDataBit;
    }
    if (j.contains(JSON_VENDOR_OUI))
    {
        std::string value;
        j.at(JSON_VENDOR_OUI).get_to(value);
        ::ot::commissioner::utils::Hex(p.mVendorOui, value);
        p.mPresentFlags |= BorderAgent::kVendorOuiBit;
    }
    if (j.contains(JSON_DOM_REF))
    {
        j.at(JSON_DOM_REF).get_to(p.dom_id);
    }
    if (j.contains(JSON_BBR_SEQ_NUMBER))
    {
        j.at(JSON_BBR_SEQ_NUMBER).get_to(p.mBbrSeqNumber);
        p.mPresentFlags |= BorderAgent::kBbrSeqNumberBit;
    }
    if (j.contains(JSON_BBR_PORT))
    {
        j.at(JSON_BBR_PORT).get_to(p.mBbrPort);
        p.mPresentFlags |= BorderAgent::kBbrPortBit;
    }
}

} // namespace ot::commissioner::persistent_storage
