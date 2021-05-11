// TODO copyright

#include <catch2/catch.hpp>

#include "registry.hpp"

#include <vector>

#include <unistd.h>

using namespace ot::commissioner::persistent_storage;
using namespace ot::commissioner;

const char json_path[] = "./registry_test.json";

TEST_CASE("create-empty-registry", "[reg_json]")
{
    unlink(json_path);
    Registry reg(json_path);

    REQUIRE(reg.Open() == Registry::Status::REG_SUCCESS);
    REQUIRE(reg.Close() == Registry::Status::REG_SUCCESS);
}

TEST_CASE("create-br-from-ba", "[reg_json]")
{
    unlink(json_path);
    Registry reg(json_path);

    REQUIRE(reg.Open() == Registry::Status::REG_SUCCESS);

    // TODO Re-implement by creating with explicit persistent storage
#if 0
    // Create border_router with network
    {
        BorderAgent ba{"1.1.1.1",
                       1,
                       ByteArray{},
                       "",
                       BorderAgent::State{0},
                       "net1",
                       0x0101010101010101,
                       "",
                       "",
                       Timestamp{},
                       0,
                       "",
                       ByteArray{},
                       "",
                       0,
                       0,
                       "",
                       0,
                       BorderAgent::kAddrBit | BorderAgent::kPortBit | BorderAgent::kNetworkNameBit |
                           BorderAgent::kExtendedPanIdBit};
        REQUIRE(reg.add(ba) == Registry::Status::REG_SUCCESS);
        border_router ret_val;
        REQUIRE(reg.get(border_router_id{0}, ret_val) == Registry::Status::REG_SUCCESS);
        REQUIRE(ret_val.nwk_id.id == 0);
        network nwk;
        REQUIRE(reg.get(network_id{0}, nwk) == Registry::Status::REG_SUCCESS);
        REQUIRE(nwk.dom_id.id == EMPTY_ID);
    }

    // Create border_router with network and domain
    {
        BorderAgent ba{"1.1.1.2",
                       2,
                       ByteArray{},
                       "",
                       BorderAgent::State{0},
                       "net2",
                       0x0202020202020202,
                       "",
                       "",
                       Timestamp{},
                       0,
                       "",
                       ByteArray{},
                       "dom2",
                       0,
                       0,
                       "",
                       0,
                       BorderAgent::kAddrBit | BorderAgent::kPortBit | BorderAgent::kNetworkNameBit |
                           BorderAgent::kExtendedPanIdBit | BorderAgent::kDomainNameBit};
        REQUIRE(reg.add(ba) == Registry::Status::REG_SUCCESS);

        border_router ret_val;
        REQUIRE(reg.get(border_router_id{1}, ret_val) == Registry::Status::REG_SUCCESS);
        REQUIRE(ret_val.nwk_id.id == 1);
        network nwk;
        REQUIRE(reg.get(network_id{1}, nwk) == Registry::Status::REG_SUCCESS);
        REQUIRE(nwk.id.id == 1);
        REQUIRE(nwk.dom_id.id == 0);
        domain dom;
        REQUIRE(reg.get(domain_id{0}, dom) == Registry::Status::REG_SUCCESS);
    }

    // Fail to create border_router without network
    {
        // Modify explicitly
        BorderAgent ba{"1.1.1.1",
                       1,
                       ByteArray{},
                       "",
                       BorderAgent::State{0},
                       "",
                       0,
                       "vendorName",
                       "",
                       Timestamp{},
                       0,
                       "",
                       ByteArray{},
                       "",
                       0,
                       0,
                       "",
                       0,
                       BorderAgent::kAddrBit | BorderAgent::kPortBit | BorderAgent::kVendorNameBit};
        REQUIRE(reg.add(ba) == Registry::Status::REG_ERROR);
    }
    // Update border_router fields
    {
        // Check the border_router present
        border_router val;
        REQUIRE(reg.get(border_router_id{0}, val) == Registry::Status::REG_SUCCESS);
        REQUIRE((val.agent.mPresentFlags & (BorderAgent::kAddrBit | BorderAgent::kPortBit)) ==
                (BorderAgent::kAddrBit | BorderAgent::kPortBit));
        REQUIRE(val.agent.mAddr == "1.1.1.1");
        val.agent.mPresentFlags |=
            BorderAgent::kNetworkNameBit | BorderAgent::kExtendedPanIdBit | BorderAgent::kDomainNameBit;
        network nwk;
        REQUIRE(reg.get(val.nwk_id, nwk) == Registry::Status::REG_SUCCESS);

        // Modify explicitly
        BorderAgent ba{"1.1.1.1",
                       1,
                       ByteArray{},
                       "",
                       BorderAgent::State{0},
                       "net1",
                       0x0101010101010101,
                       "vendorName",
                       "",
                       Timestamp{},
                       0,
                       "",
                       ByteArray{},
                       "",
                       0,
                       0,
                       "",
                       0,
                       BorderAgent::kAddrBit | BorderAgent::kPortBit | BorderAgent::kNetworkNameBit |
                           BorderAgent::kExtendedPanIdBit | BorderAgent::kVendorNameBit};
        REQUIRE(reg.add(ba) == Registry::Status::REG_SUCCESS);
        border_router new_val;
        REQUIRE(reg.get(border_router_id{0}, new_val) == Registry::Status::REG_SUCCESS);
        REQUIRE(val.id.id == new_val.id.id);
        REQUIRE(val.nwk_id.id == new_val.nwk_id.id);
        REQUIRE((new_val.agent.mPresentFlags & BorderAgent::kVendorNameBit) != 0);
        REQUIRE(new_val.agent.mVendorName == "vendorName");
    }

    // Update border_router - switch network
    {
        // Modify explicitly
        BorderAgent ba{"1.1.1.1",
                       1,
                       ByteArray{},
                       "",
                       BorderAgent::State{0},
                       "net3",
                       0x0303030303030303,
                       "vendorName",
                       "",
                       Timestamp{},
                       0,
                       "",
                       ByteArray{},
                       "",
                       0,
                       0,
                       "",
                       0,
                       BorderAgent::kAddrBit | BorderAgent::kPortBit | BorderAgent::kNetworkNameBit |
                           BorderAgent::kExtendedPanIdBit | BorderAgent::kVendorNameBit};
        REQUIRE(reg.add(ba) == Registry::Status::REG_SUCCESS);
        border_router new_val;
        REQUIRE(reg.get(border_router_id{0}, new_val) == Registry::Status::REG_SUCCESS);
        REQUIRE(new_val.nwk_id.id == 2);
    }

    // Update border_router - add network into domain
    {
        // Modify explicitly
        BorderAgent ba{"1.1.1.1",
                       1,
                       ByteArray{},
                       "",
                       BorderAgent::State{0},
                       "net3",
                       0x0303030303030303,
                       "vendorName",
                       "",
                       Timestamp{},
                       0,
                       "",
                       ByteArray{},
                       "dom2",
                       0,
                       0,
                       "",
                       0,
                       BorderAgent::kAddrBit | BorderAgent::kPortBit | BorderAgent::kNetworkNameBit |
                           BorderAgent::kExtendedPanIdBit | BorderAgent::kVendorNameBit | BorderAgent::kDomainNameBit};
        REQUIRE(reg.add(ba) == Registry::Status::REG_SUCCESS);
        border_router new_val;
        REQUIRE(reg.get(border_router_id{0}, new_val) == Registry::Status::REG_SUCCESS);
        REQUIRE(new_val.nwk_id.id == 2);
        network nwk;
        REQUIRE(reg.get(network_id{2}, nwk) == Registry::Status::REG_SUCCESS);
        REQUIRE(nwk.dom_id.id == 0);
    }

    // Update border_router - move network into another domain
    {
        // Modify explicitly
        BorderAgent ba{"1.1.1.1",
                       1,
                       ByteArray{},
                       "",
                       BorderAgent::State{0},
                       "net3",
                       0x0303030303030303,
                       "vendorName",
                       "",
                       Timestamp{},
                       0,
                       "",
                       ByteArray{},
                       "dom1", // New domain, will have id == 1
                       0,
                       0,
                       "",
                       0,
                       BorderAgent::kAddrBit | BorderAgent::kPortBit | BorderAgent::kNetworkNameBit |
                           BorderAgent::kExtendedPanIdBit | BorderAgent::kVendorNameBit | BorderAgent::kDomainNameBit};
        REQUIRE(reg.add(ba) == Registry::Status::REG_SUCCESS);
        border_router new_val;
        REQUIRE(reg.get(border_router_id{0}, new_val) == Registry::Status::REG_SUCCESS);
        REQUIRE(new_val.nwk_id.id == 2);
        network nwk;
        REQUIRE(reg.get(network_id{2}, nwk) == Registry::Status::REG_SUCCESS);
        REQUIRE(nwk.dom_id.id == 1);
    }
#endif
    REQUIRE(reg.Close() == Registry::Status::REG_SUCCESS);
}
