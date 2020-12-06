#include "registry_entries.hpp"

#include "common/utils.hpp"

using nlohmann::json;

namespace ot {

namespace commissioner {

namespace persistent_storage {

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
    j = json{{JSON_ID, p.id}, {JSON_NAME, p.name}};
}

void from_json(const json &j, domain &p)
{
    j.at(JSON_ID).get_to(p.id);
    j.at(JSON_NAME).get_to(p.name);
}

void to_json(json &j, const network &p)
{
    j = json{{JSON_ID, p.id},
             {JSON_DOM_REF, p.dom_id},
             {JSON_NAME, p.name},
             {JSON_PAN, p.pan},
             {JSON_XPAN, (std::string)p.xpan},
             {JSON_CHANNEL, p.channel},
             {JSON_MLP, p.mlp},
             {JSON_CCM, p.ccm}};
}

void from_json(const json &j, network &p)
{
    j.at(JSON_ID).get_to(p.id);
    j.at(JSON_DOM_REF).get_to(p.dom_id);
    j.at(JSON_NAME).get_to(p.name);
    j.at(JSON_PAN).get_to(p.pan);
    std::string xpan_str;
    j.at(JSON_XPAN).get_to(xpan_str);
    p.xpan = xpan_str;
    j.at(JSON_CHANNEL).get_to(p.channel);
    j.at(JSON_MLP).get_to(p.mlp);
    j.at(JSON_CCM).get_to(p.ccm);
}

void to_json(json &j, const border_router &p)
{
    j               = json{};
    j[JSON_ID]      = p.id;
    j[JSON_NWK_REF] = p.nwk_id;
    if (p.agent.mPresentFlags & BorderAgent::kAddrBit)
    {
        j[JSON_ADDR] = p.agent.mAddr;
    }
    if (p.agent.mPresentFlags & BorderAgent::kPortBit)
    {
        j[JSON_PORT] = p.agent.mPort;
    }
    if (p.agent.mPresentFlags & BorderAgent::kThreadVersionBit)
    {
        j[JSON_THREAD_VERSION] = p.agent.mThreadVersion;
    }
    if (p.agent.mPresentFlags & BorderAgent::kStateBit)
    {
        uint32_t value       = p.agent.mState;
        j[JSON_STATE_BITMAP] = value;
    }
    if (p.agent.mPresentFlags & BorderAgent::kVendorNameBit)
    {
        j[JSON_VENDOR_NAME] = p.agent.mVendorName;
    }
    if (p.agent.mPresentFlags & BorderAgent::kModelNameBit)
    {
        j[JSON_MODEL_NAME] = p.agent.mModelName;
    }
    if (p.agent.mPresentFlags & BorderAgent::kActiveTimestampBit)
    {
        j[JSON_ACTIVE_TIMESTAMP] = p.agent.mActiveTimestamp.Encode();
    }
    if (p.agent.mPresentFlags & BorderAgent::kPartitionIdBit)
    {
        j[JSON_PARTITION_ID] = p.agent.mPartitionId;
    }
    if (p.agent.mPresentFlags & BorderAgent::kVendorDataBit)
    {
        j[JSON_VENDOR_DATA] = p.agent.mVendorData;
    }
    if (p.agent.mPresentFlags & BorderAgent::kVendorOuiBit)
    {
        j[JSON_VENDOR_OUI] = ::ot::commissioner::utils::Hex(p.agent.mVendorOui);
    }
    if (p.agent.mPresentFlags & BorderAgent::kBbrSeqNumberBit)
    {
        j[JSON_BBR_SEQ_NUMBER] = p.agent.mBbrSeqNumber;
    }
    if (p.agent.mPresentFlags & BorderAgent::kBbrPortBit)
    {
        j[JSON_BBR_PORT] = p.agent.mBbrPort;
    }
    if (p.agent.mPresentFlags & BorderAgent::kServiceNameBit)
    {
        j[JSON_SERVICE_NAME] = p.agent.mServiceName;
    }
    if (p.agent.mPresentFlags & BorderAgent::kUpdateTimestamp)
    {
        j[JSON_UPDATE_TIMESTAMP] = (std::string)p.agent.mUpdateTimestamp;
    }
}

void from_json(const json &j, border_router &p)
{
    j.at(JSON_ID).get_to(p.id);
    p.agent.mPresentFlags = 0;
    if (j.contains(JSON_ADDR))
    {
        j.at(JSON_ADDR).get_to(p.agent.mAddr);
        p.agent.mPresentFlags |= BorderAgent::kAddrBit;
    }
    if (j.contains(JSON_PORT))
    {
        j.at(JSON_PORT).get_to(p.agent.mPort);
        p.agent.mPresentFlags |= BorderAgent::kPortBit;
    }
    if (j.contains(JSON_THREAD_VERSION))
    {
        j.at(JSON_THREAD_VERSION).get_to(p.agent.mThreadVersion);
        p.agent.mPresentFlags |= BorderAgent::kThreadVersionBit;
    }
    if (j.contains(JSON_STATE_BITMAP))
    {
        uint32_t value;
        j.at(JSON_STATE_BITMAP).get_to(value);
        p.agent.mState = value;
        p.agent.mPresentFlags |= BorderAgent::kStateBit;
    }
    if (j.contains(JSON_NWK_REF))
    {
        j.at(JSON_NWK_REF).get_to(p.nwk_id);
    }
    if (j.contains(JSON_VENDOR_NAME))
    {
        j.at(JSON_VENDOR_NAME).get_to(p.agent.mVendorName);
        p.agent.mPresentFlags |= BorderAgent::kVendorNameBit;
    }
    if (j.contains(JSON_MODEL_NAME))
    {
        j.at(JSON_MODEL_NAME).get_to(p.agent.mModelName);
        p.agent.mPresentFlags |= BorderAgent::kModelNameBit;
    }
    if (j.contains(JSON_ACTIVE_TIMESTAMP))
    {
        uint64_t value;
        j.at(JSON_ACTIVE_TIMESTAMP).get_to(value);
        p.agent.mActiveTimestamp.Decode(value);
        p.agent.mPresentFlags |= BorderAgent::kActiveTimestampBit;
    }
    if (j.contains(JSON_PARTITION_ID))
    {
        j.at(JSON_PARTITION_ID).get_to(p.agent.mPartitionId);
        p.agent.mPresentFlags |= BorderAgent::kPartitionIdBit;
    }
    if (j.contains(JSON_VENDOR_DATA))
    {
        j.at(JSON_VENDOR_DATA).get_to(p.agent.mVendorData);
        p.agent.mPresentFlags |= BorderAgent::kVendorDataBit;
    }
    if (j.contains(JSON_VENDOR_OUI))
    {
        Error       error;
        std::string value;
        j.at(JSON_VENDOR_OUI).get_to(value);
        error = ot::commissioner::utils::Hex(p.agent.mVendorOui, value);
        ASSERT(error.GetCode() == ErrorCode::kNone);
        p.agent.mPresentFlags |= BorderAgent::kVendorOuiBit;
    }
    if (j.contains(JSON_BBR_SEQ_NUMBER))
    {
        j.at(JSON_BBR_SEQ_NUMBER).get_to(p.agent.mBbrSeqNumber);
        p.agent.mPresentFlags |= BorderAgent::kBbrSeqNumberBit;
    }
    if (j.contains(JSON_BBR_PORT))
    {
        j.at(JSON_BBR_PORT).get_to(p.agent.mBbrPort);
        p.agent.mPresentFlags |= BorderAgent::kBbrPortBit;
    }
    if (j.contains(JSON_SERVICE_NAME))
    {
        j.at(JSON_SERVICE_NAME).get_to(p.agent.mServiceName);
        p.agent.mPresentFlags |= BorderAgent::kServiceNameBit;
    }
    if (j.contains(JSON_UPDATE_TIMESTAMP))
    {
        std::string tmp;
        j.at(JSON_UPDATE_TIMESTAMP).get_to(tmp);
        UnixTime ts{tmp};
        if (ts.mTime != 0)
        {
            p.agent.mUpdateTimestamp.mTime = ts.mTime;
            p.agent.mPresentFlags |= BorderAgent::kUpdateTimestamp;
        }
    }
}

} // namespace persistent_storage

} // namespace commissioner

} // namespace ot
