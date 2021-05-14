/*
 *    Copyright (c) 2019, The OpenThread Commissioner Authors.
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
 *   The file implements unit tests for JSON-based persistent storage
 */

#include <catch2/catch.hpp>

#include "persistent_storage_json.hpp"

#include <fstream>
#include <vector>

#include <unistd.h>

#include "app/border_agent.hpp"

using namespace ot::commissioner::persistent_storage;
using namespace ot::commissioner;

TEST_CASE("Create default if not exists", "[ps_json]")
{
    PersistentStorageJson psj("./test_ps.json");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Read empty file", "[ps_json]")
{
    std::ofstream test_tmp("./test.tmp");
    test_tmp.close();

    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Read non empty - default struct", "[ps_json]")
{
    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Add registrar", "[ps_json]")
{
    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    registrar_id new_id;

    REQUIRE(psj.Add(registrar{EMPTY_ID, "0.0.0.1", 1, {"dom1"}}, new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(new_id.id == 0);
    REQUIRE(psj.Add(registrar{EMPTY_ID, "0.0.0.2", 2, {"dom2"}}, new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(new_id.id == 1);
    REQUIRE(psj.Add(registrar{EMPTY_ID, "0.0.0.3", 3, {"dom3"}}, new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(new_id.id == 2);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Add domain", "[ps_json]")
{
    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    domain_id new_id;

    REQUIRE(psj.Add(domain{EMPTY_ID, "dom1"}, new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(new_id.id == 0);
    REQUIRE(psj.Add(domain{EMPTY_ID, "dom2"}, new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(new_id.id == 1);
    REQUIRE(psj.Add(domain{EMPTY_ID, "dom3"}, new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(new_id.id == 2);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Add network", "[ps_json]")
{
    // Make the test independent
    unlink("./test.tmp");

    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    network_id new_id;

    REQUIRE(psj.Add(network{EMPTY_ID, EMPTY_ID, "nwk1", xpan_id{"FFFFFFFFFFFFFFF1"}, 11, "FFF1", "2000:aaa1::0/8", 1},
                    new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(new_id.id == 0);
    REQUIRE(psj.Add(network{EMPTY_ID, EMPTY_ID, "nwk2", xpan_id{"FFFFFFFFFFFFFFF2"}, 11, "FFF2", "2000:aaa2::0/8", 1},
                    new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(new_id.id == 1);
    REQUIRE(psj.Add(network{EMPTY_ID, EMPTY_ID, "nwk3", xpan_id{"FFFFFFFFFFFFFFF3"}, 11, "FFF3", "2000:aaa3::0/8", 1},
                    new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(new_id.id == 2);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Add br", "[ps_json]")
{
    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    border_router_id new_id;

    REQUIRE(psj.Add(border_router{EMPTY_ID, EMPTY_ID,
                                  BorderAgent{"1.1.1.2", 11, ByteArray{}, "th1.x", BorderAgent::State{1, 0, 1, 0, 1},
                                              "network_id", 0x1011223344556677ll, "vendor_name", "model_name",
                                              Timestamp{0, 0, 0}, 1, "vendor_data", ByteArray{1, 2}, "domain_name", 0,
                                              0, "", 0, 0xFFFF}},
                    new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(new_id.id == 0);
    REQUIRE(psj.Add(border_router{EMPTY_ID, EMPTY_ID,
                                  BorderAgent{"1.1.1.3", 12, ByteArray{}, "th1.x", BorderAgent::State{1, 0, 1, 0, 1},
                                              "network_id", 0x1011223344556677ll, "vendor_name", "model_name",
                                              Timestamp{0, 0, 0}, 1, "vendor_data", ByteArray{1, 2}, "domain_name", 0,
                                              0, "", 0, 0xFFFF}},
                    new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(new_id.id == 1);
    REQUIRE(psj.Add(border_router{EMPTY_ID, EMPTY_ID,
                                  BorderAgent{"1.1.1.4", 13, ByteArray{}, "th1.x", BorderAgent::State{1, 0, 1, 0, 1},
                                              "network_id", 0x1011223344556677ll, "vendor_name", "model_name",
                                              Timestamp{0, 0, 0}, 1, "vendor_data", ByteArray{1, 2}, "domain_name", 0,
                                              0, "", 0, 0xFFFF}},
                    new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(new_id.id == 2);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Del registrar", "[ps_json]")
{
    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    REQUIRE(psj.Del(registrar_id(0)) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Del(registrar_id(1)) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Del(registrar_id(2)) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Del(registrar_id(50)) == PersistentStorage::Status::PS_SUCCESS);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Del domain", "[ps_json]")
{
    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    REQUIRE(psj.Del(domain_id(0)) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Del(domain_id(1)) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Del(domain_id(2)) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Del(domain_id(50)) == PersistentStorage::Status::PS_SUCCESS);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Del network", "[ps_json]")
{
    PersistentStorageJson psj("");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    network_id new_id;
    REQUIRE(psj.Add(network{EMPTY_ID, EMPTY_ID, "nwk1", xpan_id{"FFFFFFFFFFFFFFF1"}, 11, "FFF1", "2000:aaa1::0/8", 1},
                    new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(new_id.id == 0);

    REQUIRE(psj.Del(network_id(0)) == PersistentStorage::Status::PS_SUCCESS);
    // Absent network successfully deletes newertheless
    REQUIRE(psj.Del(network_id(1)) == PersistentStorage::Status::PS_SUCCESS);

    NetworkArray nets;
    REQUIRE(psj.Lookup(network{}, nets) == PersistentStorage::Status::PS_NOT_FOUND);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Del br", "[ps_json]")
{
    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    REQUIRE(psj.Del(border_router_id(0)) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Del(border_router_id(1)) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Del(border_router_id(2)) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Del(border_router_id(50)) == PersistentStorage::Status::PS_SUCCESS);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Get registrar from empty", "[ps_json]")
{
    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    registrar ret_val;

    REQUIRE(psj.Get(registrar_id(0), ret_val) == PersistentStorage::Status::PS_NOT_FOUND);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Get domain from empty", "[ps_json]")
{
    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    domain ret_val;

    REQUIRE(psj.Get(domain_id(0), ret_val) == PersistentStorage::Status::PS_NOT_FOUND);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Get network from empty", "[ps_json]")
{
    PersistentStorageJson psj("");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    network ret_val;

    REQUIRE(psj.Get(network_id(0), ret_val) == PersistentStorage::Status::PS_NOT_FOUND);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Get br from empty", "[ps_json]")
{
    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    border_router ret_val;

    REQUIRE(psj.Get(border_router_id(0), ret_val) == PersistentStorage::Status::PS_NOT_FOUND);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Get registrar, not empty", "[ps_json]")
{
    // Make test independent
    unlink("./test.tmp");

    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    registrar_id new_id;

    REQUIRE(psj.Add(registrar{EMPTY_ID, "0.0.0.1", 1, {"dom1"}}, new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(new_id.id == 0);
    REQUIRE(psj.Add(registrar{EMPTY_ID, "0.0.0.2", 2, {"dom2"}}, new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(new_id.id == 1);
    REQUIRE(psj.Add(registrar{EMPTY_ID, "0.0.0.3", 3, {"dom3"}}, new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(new_id.id == 2);

    registrar ret_val;

    REQUIRE(psj.Get(registrar_id(3), ret_val) == PersistentStorage::Status::PS_NOT_FOUND);
    REQUIRE(psj.Get(registrar_id(1), ret_val) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(ret_val.id.id == 1);
    REQUIRE(ret_val.addr == "0.0.0.2");
    REQUIRE(ret_val.port == 2);
    REQUIRE(ret_val.domains == std::vector<std::string>{"dom2"});

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Get domain, not empty", "[ps_json]")
{
    // Make test independent
    unlink("./test.tmp");

    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    domain_id new_id;

    REQUIRE(psj.Add(domain{EMPTY_ID, "dom1"}, new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(new_id.id == 0);
    REQUIRE(psj.Add(domain{EMPTY_ID, "dom2"}, new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(new_id.id == 1);
    REQUIRE(psj.Add(domain{EMPTY_ID, "dom3"}, new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(new_id.id == 2);

    domain ret_val;

    REQUIRE(psj.Get(domain_id(3), ret_val) == PersistentStorage::Status::PS_NOT_FOUND);
    REQUIRE(psj.Get(domain_id(0), ret_val) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(ret_val.id.id == 0);
    REQUIRE(ret_val.name == "dom1");

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Get network, not empty", "[ps_json]")
{
    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    network_id new_id;

    REQUIRE(psj.Add(network{EMPTY_ID, EMPTY_ID, "nwk1", 0xFFFFFFFFFFFFFFF1, 11, "FFF1", "2000:aaa1::0/8", 1}, new_id) ==
            PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(new_id.id == 0);
    REQUIRE(psj.Add(network{EMPTY_ID, EMPTY_ID, "nwk2", 0xFFFFFFFFFFFFFFF2, 12, "FFF2", "2000:aaa2::0/8", 1}, new_id) ==
            PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(new_id.id == 1);
    REQUIRE(psj.Add(network{EMPTY_ID, EMPTY_ID, "nwk3", 0xFFFFFFFFFFFFFFF3, 13, "FFF3", "2000:aaa3::0/8", 1}, new_id) ==
            PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(new_id.id == 2);

    network ret_val;

    REQUIRE(psj.Get(network_id(5), ret_val) == PersistentStorage::Status::PS_NOT_FOUND);
    REQUIRE(psj.Get(network_id(0), ret_val) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(ret_val.id.id == 0);
    REQUIRE(ret_val.name == "nwk1");
    REQUIRE(ret_val.channel == 11);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Get br, not empty", "[ps_json]")
{
    // Make the test independent
    unlink("./test.tmp");

    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    border_router_id new_id;

    REQUIRE(psj.Add(border_router{EMPTY_ID, EMPTY_ID,
                                  BorderAgent{"1.1.1.2", 11, ByteArray{}, "th1.x", BorderAgent::State{1, 0, 1, 0, 1},
                                              "network_id", 0x1011223344556677ll, "vendor_name", "model_name",
                                              Timestamp{0, 0, 0}, 1, "vendor_data", ByteArray{1, 2}, "domain_name", 0,
                                              0, "", 0, 0xFFFF}},
                    new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(new_id.id == 0);
    REQUIRE(psj.Add(border_router{EMPTY_ID, EMPTY_ID,
                                  BorderAgent{"1.1.1.3", 12, ByteArray{}, "th1.x", BorderAgent::State{1, 0, 1, 0, 1},
                                              "network_id", 0x1011223344556677ll, "vendor_name", "model_name",
                                              Timestamp{0, 0, 0}, 1, "vendor_data", ByteArray{1, 2}, "domain_name", 0,
                                              0, "", 0, 0xFFFF}},
                    new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(new_id.id == 1);
    REQUIRE(psj.Add(border_router{EMPTY_ID, EMPTY_ID,
                                  BorderAgent{"1.1.1.4", 13, ByteArray{}, "th1.x", BorderAgent::State{1, 0, 1, 0, 1},
                                              "network_id", 0x1011223344556677ll, "vendor_name", "model_name",
                                              Timestamp{0, 0, 0}, 1, "vendor_data", ByteArray{1, 2}, "domain_name", 0,
                                              0, "", 0, 0xFFFF}},
                    new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(new_id.id == 2);

    border_router ret_val;

    REQUIRE(psj.Get(border_router_id(3), ret_val) == PersistentStorage::Status::PS_NOT_FOUND);
    REQUIRE(psj.Get(border_router_id(1), ret_val) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(ret_val.id.id == 1);
    REQUIRE(ret_val.agent.mPort == 12);
    REQUIRE(ret_val.agent.mAddr == "1.1.1.3");

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

// UPD
TEST_CASE("Upd registrar", "[ps_json]")
{
    // Make the test independent
    unlink("./test.tmp");

    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    // Add initial data
    registrar_id new_id;

    REQUIRE(psj.Add(registrar{EMPTY_ID, "0.0.0.1", 1, {"dom1"}}, new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Add(registrar{EMPTY_ID, "0.0.0.2", 2, {"dom2"}}, new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Add(registrar{EMPTY_ID, "0.0.0.3", 3, {"dom3"}}, new_id) == PersistentStorage::Status::PS_SUCCESS);

    // Test actions
    registrar new_val{EMPTY_ID, "4.4.4.4", 1, {"dom4"}};

    REQUIRE(psj.Update(new_val) == PersistentStorage::Status::PS_NOT_FOUND);
    new_val.id = 2;
    REQUIRE(psj.Update(new_val) == PersistentStorage::Status::PS_SUCCESS);

    registrar ret_val;

    REQUIRE(psj.Get(registrar_id(2), ret_val) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(ret_val.id.id == 2);
    REQUIRE(ret_val.addr == "4.4.4.4");
    REQUIRE(ret_val.port == 1);
    REQUIRE(ret_val.domains == std::vector<std::string>{"dom4"});

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Upd domain", "[ps_json]")
{
    // Make the test independent
    unlink("./test.tmp");

    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    // Add initial data
    domain_id new_id;

    REQUIRE(psj.Add(domain{EMPTY_ID, "dom1"}, new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Add(domain{EMPTY_ID, "dom2"}, new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Add(domain{EMPTY_ID, "dom3"}, new_id) == PersistentStorage::Status::PS_SUCCESS);

    // Test actions
    domain new_val{EMPTY_ID, "dom_upd"};

    REQUIRE(psj.Update(new_val) == PersistentStorage::Status::PS_NOT_FOUND);
    new_val.id = 1;
    REQUIRE(psj.Update(new_val) == PersistentStorage::Status::PS_SUCCESS);

    domain ret_val;

    REQUIRE(psj.Get(domain_id(1), ret_val) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(ret_val.id.id == 1);
    REQUIRE(ret_val.name == "dom_upd");

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Upd network", "[ps_json]")
{
    unlink("./tmp.json");
    PersistentStorageJson psj("./tmp.json");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    network nwk{EMPTY_ID, EMPTY_ID, "nwk", 0xFFFFFFFFFFFFFFFA, 17, "FFFA", "2000:aaa1::0/64", 0};

    REQUIRE(psj.Update(nwk) == PersistentStorage::Status::PS_NOT_FOUND);
    network_id nid;
    REQUIRE(psj.Add(nwk, nid) == PersistentStorage::Status::PS_SUCCESS);
    nwk.id      = nid;
    nwk.channel = 18;
    nwk.name    = "nwk_upd";
    REQUIRE(psj.Update(nwk) == PersistentStorage::Status::PS_SUCCESS);

    network ret_val;

    REQUIRE(psj.Get(network_id(0), ret_val) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(ret_val.id.id == 0);
    REQUIRE(ret_val.name == "nwk_upd");
    REQUIRE(ret_val.channel == 18);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Upd br", "[ps_json]")
{
    // Make test independent
    unlink("./test.tmp");

    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    // Add initial data
    border_router_id new_id;

    REQUIRE(psj.Add(border_router{EMPTY_ID, EMPTY_ID,
                                  BorderAgent{"1.1.1.2", 11, ByteArray{}, "th1.x", BorderAgent::State{1, 0, 1, 0, 1},
                                              "network_id", 0x1011223344556677ll, "vendor_name", "model_name",
                                              Timestamp{0, 0, 0}, 1, "vendor_data", ByteArray{1, 2}, "domain_name", 0,
                                              0, "", 0, 0xFFFF}},
                    new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Add(border_router{EMPTY_ID, EMPTY_ID,
                                  BorderAgent{"1.1.1.3", 12, ByteArray{}, "th1.x", BorderAgent::State{1, 0, 1, 0, 1},
                                              "network_id", 0x1011223344556677ll, "vendor_name", "model_name",
                                              Timestamp{0, 0, 0}, 1, "vendor_data", ByteArray{1, 2}, "domain_name", 0,
                                              0, "", 0, 0xFFFF}},
                    new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Add(border_router{EMPTY_ID, EMPTY_ID,
                                  BorderAgent{"1.1.1.4", 13, ByteArray{}, "th1.x", BorderAgent::State{1, 0, 1, 0, 1},
                                              "network_id", 0x1011223344556677ll, "vendor_name", "model_name",
                                              Timestamp{0, 0, 0}, 1, "vendor_data", ByteArray{1, 2}, "domain_name", 0,
                                              0, "", 0, 0xFFFF}},
                    new_id) == PersistentStorage::Status::PS_SUCCESS);

    // Test actions
    border_router new_val{EMPTY_ID, EMPTY_ID,

                          BorderAgent{"5.5.5.5", 18, ByteArray{}, "th1.x", BorderAgent::State{0, 0, 2, 0, 0},
                                      "network_id", 0x1011223344556677ll, "vendor_name", "model_name",
                                      Timestamp{0, 0, 0}, 1, "vendor_data", ByteArray{1, 2}, "domain_name", 0, 0, "", 0,
                                      0xFFFF}};

    REQUIRE(psj.Update(new_val) == PersistentStorage::Status::PS_NOT_FOUND);
    new_val.id = 2;
    REQUIRE(psj.Update(new_val) == PersistentStorage::Status::PS_SUCCESS);

    border_router ret_val;

    REQUIRE(psj.Get(border_router_id(2), ret_val) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(ret_val.id.id == 2);
    REQUIRE(ret_val.agent.mPort == 18);
    REQUIRE(ret_val.agent.mAddr == "5.5.5.5");

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Lookup registrar", "[ps_json]")
{
    // Make test independent
    unlink("./test.tmp");

    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    //  Populate storage with test data
    registrar_id new_id;

    REQUIRE(psj.Add(registrar{EMPTY_ID, "0.0.0.1", 1, {"dom1"}}, new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Add(registrar{EMPTY_ID, "0.0.0.2", 1, {"dom2"}}, new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Add(registrar{EMPTY_ID, "0.0.0.3", 3, {"dom3"}}, new_id) == PersistentStorage::Status::PS_SUCCESS);

    // Test actions
    std::vector<registrar> ret_Lookup;

    registrar search_req{EMPTY_ID, "", 0, {}};
    REQUIRE(psj.Lookup(search_req, ret_Lookup) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(ret_Lookup.size() == 3);

    ret_Lookup.clear();

    search_req.id.id = 0;
    REQUIRE(psj.Lookup(search_req, ret_Lookup) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(ret_Lookup.size() == 1);
    REQUIRE(ret_Lookup[0].id.id == 0);

    ret_Lookup.clear();

    search_req.id.id = 0;
    search_req.addr  = "0.0.0.2";
    REQUIRE(psj.Lookup(search_req, ret_Lookup) == PersistentStorage::Status::PS_NOT_FOUND);
    REQUIRE(ret_Lookup.size() == 0);

    ret_Lookup.clear();

    search_req.id.id = 0;
    search_req.addr  = "0.0.0.1";
    REQUIRE(psj.Lookup(search_req, ret_Lookup) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(ret_Lookup.size() == 1);
    REQUIRE(ret_Lookup[0].id.id == 0);

    ret_Lookup.clear();

    search_req.id.id = 0;
    search_req.addr  = "0.0.0.1";
    search_req.port  = 1;
    REQUIRE(psj.Lookup(search_req, ret_Lookup) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(ret_Lookup.size() == 1);
    REQUIRE(ret_Lookup[0].id.id == 0);

    ret_Lookup.clear();

    search_req.id.id   = 0;
    search_req.addr    = "0.0.0.1";
    search_req.port    = 1;
    search_req.domains = {"dom1"};
    REQUIRE(psj.Lookup(search_req, ret_Lookup) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(ret_Lookup.size() == 1);
    REQUIRE(ret_Lookup[0].id.id == 0);

    ret_Lookup.clear();

    search_req      = {};
    search_req.port = 1;
    REQUIRE(psj.Lookup(search_req, ret_Lookup) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(ret_Lookup.size() == 2);
    REQUIRE(ret_Lookup[0].id.id == 0);
    REQUIRE(ret_Lookup[1].id.id == 1);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Lookup network", "[ps_json]")
{
    // Make test independent
    unlink("./test.tmp");

    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    // Populate storage with initial data
    network_id new_id;
    REQUIRE(psj.Add(network{EMPTY_ID, EMPTY_ID, "nwk1", xpan_id{"FFFFFFFFFFFFFFF1"}, 11, "FFF1", "2000:aaa1::0/8", 1},
                    new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(new_id.id == 0);
    REQUIRE(psj.Add(network{EMPTY_ID, EMPTY_ID, "nwk2", xpan_id{"FFFFFFFFFFFFFFF2"}, 11, "FFF2", "2000:aaa2::0/8", 1},
                    new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(new_id.id == 1);
    REQUIRE(psj.Add(network{EMPTY_ID, EMPTY_ID, "nwk3", xpan_id{"FFFFFFFFFFFFFFF3"}, 11, "FFF3", "2000:aaa3::0/8", 1},
                    new_id) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(new_id.id == 2);

    // The test
    std::vector<network> ret_Lookup;

    REQUIRE(psj.Lookup(network{}, ret_Lookup) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(ret_Lookup.size() == 3);

    ret_Lookup.clear();

    network net;
    net.name = "nwk1";
    net.ccm  = true;
    REQUIRE(psj.Lookup(net, ret_Lookup) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(ret_Lookup.size() == 1);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}
