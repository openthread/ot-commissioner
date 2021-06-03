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
    std::ofstream testTmp("./test.tmp");
    testTmp.close();

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

    RegistrarId newId;

    REQUIRE(psj.Add(Registrar{EMPTY_ID, "0.0.0.1", 1, {"dom1"}}, newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(newId.mId == 0);
    REQUIRE(psj.Add(Registrar{EMPTY_ID, "0.0.0.2", 2, {"dom2"}}, newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(newId.mId == 1);
    REQUIRE(psj.Add(Registrar{EMPTY_ID, "0.0.0.3", 3, {"dom3"}}, newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(newId.mId == 2);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Add domain", "[ps_json]")
{
    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    DomainId newId;

    REQUIRE(psj.Add(Domain{EMPTY_ID, "dom1"}, newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(newId.mId == 0);
    REQUIRE(psj.Add(Domain{EMPTY_ID, "dom2"}, newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(newId.mId == 1);
    REQUIRE(psj.Add(Domain{EMPTY_ID, "dom3"}, newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(newId.mId == 2);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Add network", "[ps_json]")
{
    // Make the test independent
    unlink("./test.tmp");

    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    NetworkId newId;

    REQUIRE(psj.Add(Network{EMPTY_ID, EMPTY_ID, "nwk1", XpanId{0xFFFFFFFFFFFFFFF1ll}, 11, "FFF1", "2000:aaa1::0/8", 1},
                    newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(newId.mId == 0);
    REQUIRE(psj.Add(Network{EMPTY_ID, EMPTY_ID, "nwk2", XpanId{0xFFFFFFFFFFFFFFF2ll}, 11, "FFF2", "2000:aaa2::0/8", 1},
                    newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(newId.mId == 1);
    REQUIRE(psj.Add(Network{EMPTY_ID, EMPTY_ID, "nwk3", XpanId{0xFFFFFFFFFFFFFFF3ll}, 11, "FFF3", "2000:aaa3::0/8", 1},
                    newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(newId.mId == 2);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Add br", "[ps_json]")
{
    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    BorderRouterId newId;

    REQUIRE(psj.Add(BorderRouter{EMPTY_ID, EMPTY_ID,
                                 BorderAgent{"1.1.1.2", 11, ByteArray{}, "th1.x", BorderAgent::State{1, 0, 1, 0, 1},
                                             "NetworkId", 0x1011223344556677ll, "vendor_name", "model_name",
                                             Timestamp{0, 0, 0}, 1, "vendor_data", ByteArray{1, 2}, "domain_name", 0, 0,
                                             "", 0, 0xFFFF}},
                    newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(newId.mId == 0);
    REQUIRE(psj.Add(BorderRouter{EMPTY_ID, EMPTY_ID,
                                 BorderAgent{"1.1.1.3", 12, ByteArray{}, "th1.x", BorderAgent::State{1, 0, 1, 0, 1},
                                             "NetworkId", 0x1011223344556677ll, "vendor_name", "model_name",
                                             Timestamp{0, 0, 0}, 1, "vendor_data", ByteArray{1, 2}, "domain_name", 0, 0,
                                             "", 0, 0xFFFF}},
                    newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(newId.mId == 1);
    REQUIRE(psj.Add(BorderRouter{EMPTY_ID, EMPTY_ID,
                                 BorderAgent{"1.1.1.4", 13, ByteArray{}, "th1.x", BorderAgent::State{1, 0, 1, 0, 1},
                                             "NetworkId", 0x1011223344556677ll, "vendor_name", "model_name",
                                             Timestamp{0, 0, 0}, 1, "vendor_data", ByteArray{1, 2}, "domain_name", 0, 0,
                                             "", 0, 0xFFFF}},
                    newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(newId.mId == 2);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Del Registrar", "[ps_json]")
{
    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    REQUIRE(psj.Del(RegistrarId(0)) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Del(RegistrarId(1)) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Del(RegistrarId(2)) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Del(RegistrarId(50)) == PersistentStorage::Status::PS_SUCCESS);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Del domain", "[ps_json]")
{
    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    REQUIRE(psj.Del(DomainId(0)) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Del(DomainId(1)) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Del(DomainId(2)) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Del(DomainId(50)) == PersistentStorage::Status::PS_SUCCESS);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Del network", "[ps_json]")
{
    PersistentStorageJson psj("");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    NetworkId newId;
    REQUIRE(psj.Add(Network{EMPTY_ID, EMPTY_ID, "nwk1", XpanId{0xFFFFFFFFFFFFFFF1ll}, 11, "FFF1", "2000:aaa1::0/8", 1},
                    newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(newId.mId == 0);

    REQUIRE(psj.Del(NetworkId(0)) == PersistentStorage::Status::PS_SUCCESS);
    // Absent network successfully deletes newertheless
    REQUIRE(psj.Del(NetworkId(1)) == PersistentStorage::Status::PS_SUCCESS);

    NetworkArray nets;
    REQUIRE(psj.Lookup(Network{}, nets) == PersistentStorage::Status::PS_NOT_FOUND);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Del br", "[ps_json]")
{
    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    REQUIRE(psj.Del(BorderRouterId(0)) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Del(BorderRouterId(1)) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Del(BorderRouterId(2)) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Del(BorderRouterId(50)) == PersistentStorage::Status::PS_SUCCESS);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Get Registrar from empty", "[ps_json]")
{
    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    Registrar returnValue;

    REQUIRE(psj.Get(RegistrarId(0), returnValue) == PersistentStorage::Status::PS_NOT_FOUND);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Get domain from empty", "[ps_json]")
{
    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    Domain returnValue;

    REQUIRE(psj.Get(DomainId(0), returnValue) == PersistentStorage::Status::PS_NOT_FOUND);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Get network from empty", "[ps_json]")
{
    PersistentStorageJson psj("");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    Network returnValue;

    REQUIRE(psj.Get(NetworkId(0), returnValue) == PersistentStorage::Status::PS_NOT_FOUND);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Get br from empty", "[ps_json]")
{
    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    BorderRouter returnValue;

    REQUIRE(psj.Get(BorderRouterId(0), returnValue) == PersistentStorage::Status::PS_NOT_FOUND);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Get Registrar, not empty", "[ps_json]")
{
    // Make test independent
    unlink("./test.tmp");

    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    RegistrarId newId;

    REQUIRE(psj.Add(Registrar{EMPTY_ID, "0.0.0.1", 1, {"dom1"}}, newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(newId.mId == 0);
    REQUIRE(psj.Add(Registrar{EMPTY_ID, "0.0.0.2", 2, {"dom2"}}, newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(newId.mId == 1);
    REQUIRE(psj.Add(Registrar{EMPTY_ID, "0.0.0.3", 3, {"dom3"}}, newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(newId.mId == 2);

    Registrar returnValue;

    REQUIRE(psj.Get(RegistrarId(3), returnValue) == PersistentStorage::Status::PS_NOT_FOUND);
    REQUIRE(psj.Get(RegistrarId(1), returnValue) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(returnValue.mId.mId == 1);
    REQUIRE(returnValue.mAddr == "0.0.0.2");
    REQUIRE(returnValue.mPort == 2);
    REQUIRE(returnValue.mDomains == std::vector<std::string>{"dom2"});

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Get Domain, not empty", "[ps_json]")
{
    // Make test independent
    unlink("./test.tmp");

    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    DomainId newId;

    REQUIRE(psj.Add(Domain{EMPTY_ID, "dom1"}, newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(newId.mId == 0);
    REQUIRE(psj.Add(Domain{EMPTY_ID, "dom2"}, newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(newId.mId == 1);
    REQUIRE(psj.Add(Domain{EMPTY_ID, "dom3"}, newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(newId.mId == 2);

    Domain returnValue;

    REQUIRE(psj.Get(DomainId(3), returnValue) == PersistentStorage::Status::PS_NOT_FOUND);
    REQUIRE(psj.Get(DomainId(0), returnValue) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(returnValue.mId.mId == 0);
    REQUIRE(returnValue.mName == "dom1");

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Get Network, not empty", "[ps_json]")
{
    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    NetworkId newId;

    REQUIRE(psj.Add(Network{EMPTY_ID, EMPTY_ID, "nwk1", 0xFFFFFFFFFFFFFFF1, 11, "FFF1", "2000:aaa1::0/8", 1}, newId) ==
            PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(newId.mId == 0);
    REQUIRE(psj.Add(Network{EMPTY_ID, EMPTY_ID, "nwk2", 0xFFFFFFFFFFFFFFF2, 12, "FFF2", "2000:aaa2::0/8", 1}, newId) ==
            PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(newId.mId == 1);
    REQUIRE(psj.Add(Network{EMPTY_ID, EMPTY_ID, "nwk3", 0xFFFFFFFFFFFFFFF3, 13, "FFF3", "2000:aaa3::0/8", 1}, newId) ==
            PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(newId.mId == 2);

    Network returnValue;

    REQUIRE(psj.Get(NetworkId(5), returnValue) == PersistentStorage::Status::PS_NOT_FOUND);
    REQUIRE(psj.Get(NetworkId(0), returnValue) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(returnValue.mId.mId == 0);
    REQUIRE(returnValue.mName == "nwk1");
    REQUIRE(returnValue.mChannel == 11);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Get br, not empty", "[ps_json]")
{
    // Make the test independent
    unlink("./test.tmp");

    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    BorderRouterId newId;

    REQUIRE(psj.Add(BorderRouter{EMPTY_ID, EMPTY_ID,
                                 BorderAgent{"1.1.1.2", 11, ByteArray{}, "th1.x", BorderAgent::State{1, 0, 1, 0, 1},
                                             "NetworkId", 0x1011223344556677ll, "vendor_name", "model_name",
                                             Timestamp{0, 0, 0}, 1, "vendor_data", ByteArray{1, 2}, "Domain_name", 0, 0,
                                             "", 0, 0xFFFF}},
                    newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(newId.mId == 0);
    REQUIRE(psj.Add(BorderRouter{EMPTY_ID, EMPTY_ID,
                                 BorderAgent{"1.1.1.3", 12, ByteArray{}, "th1.x", BorderAgent::State{1, 0, 1, 0, 1},
                                             "NetworkId", 0x1011223344556677ll, "vendor_name", "model_name",
                                             Timestamp{0, 0, 0}, 1, "vendor_data", ByteArray{1, 2}, "Domain_name", 0, 0,
                                             "", 0, 0xFFFF}},
                    newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(newId.mId == 1);
    REQUIRE(psj.Add(BorderRouter{EMPTY_ID, EMPTY_ID,
                                 BorderAgent{"1.1.1.4", 13, ByteArray{}, "th1.x", BorderAgent::State{1, 0, 1, 0, 1},
                                             "NetworkId", 0x1011223344556677ll, "vendor_name", "model_name",
                                             Timestamp{0, 0, 0}, 1, "vendor_data", ByteArray{1, 2}, "Domain_name", 0, 0,
                                             "", 0, 0xFFFF}},
                    newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(newId.mId == 2);

    BorderRouter returnValue;

    REQUIRE(psj.Get(BorderRouterId(3), returnValue) == PersistentStorage::Status::PS_NOT_FOUND);
    REQUIRE(psj.Get(BorderRouterId(1), returnValue) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(returnValue.mId.mId == 1);
    REQUIRE(returnValue.mAgent.mPort == 12);
    REQUIRE(returnValue.mAgent.mAddr == "1.1.1.3");

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

// UPD
TEST_CASE("Upd Registrar", "[ps_json]")
{
    // Make the test independent
    unlink("./test.tmp");

    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    // Add initial data
    RegistrarId newId;

    REQUIRE(psj.Add(Registrar{EMPTY_ID, "0.0.0.1", 1, {"dom1"}}, newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Add(Registrar{EMPTY_ID, "0.0.0.2", 2, {"dom2"}}, newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Add(Registrar{EMPTY_ID, "0.0.0.3", 3, {"dom3"}}, newId) == PersistentStorage::Status::PS_SUCCESS);

    // Test actions
    Registrar newValue{EMPTY_ID, "4.4.4.4", 1, {"dom4"}};

    REQUIRE(psj.Update(newValue) == PersistentStorage::Status::PS_NOT_FOUND);
    newValue.mId = 2;
    REQUIRE(psj.Update(newValue) == PersistentStorage::Status::PS_SUCCESS);

    Registrar returnValue;

    REQUIRE(psj.Get(RegistrarId(2), returnValue) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(returnValue.mId.mId == 2);
    REQUIRE(returnValue.mAddr == "4.4.4.4");
    REQUIRE(returnValue.mPort == 1);
    REQUIRE(returnValue.mDomains == std::vector<std::string>{"dom4"});

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Upd Domain", "[ps_json]")
{
    // Make the test independent
    unlink("./test.tmp");

    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    // Add initial data
    DomainId newId;

    REQUIRE(psj.Add(Domain{EMPTY_ID, "dom1"}, newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Add(Domain{EMPTY_ID, "dom2"}, newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Add(Domain{EMPTY_ID, "dom3"}, newId) == PersistentStorage::Status::PS_SUCCESS);

    // Test actions
    Domain newValue{EMPTY_ID, "dom_upd"};

    REQUIRE(psj.Update(newValue) == PersistentStorage::Status::PS_NOT_FOUND);
    newValue.mId = 1;
    REQUIRE(psj.Update(newValue) == PersistentStorage::Status::PS_SUCCESS);

    Domain returnValue;

    REQUIRE(psj.Get(DomainId(1), returnValue) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(returnValue.mId.mId == 1);
    REQUIRE(returnValue.mName == "dom_upd");

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Upd Network", "[ps_json]")
{
    unlink("./tmp.json");
    PersistentStorageJson psj("./tmp.json");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    Network nwk{EMPTY_ID, EMPTY_ID, "nwk", 0xFFFFFFFFFFFFFFFA, 17, "FFFA", "2000:aaa1::0/64", 0};

    REQUIRE(psj.Update(nwk) == PersistentStorage::Status::PS_NOT_FOUND);
    NetworkId nid;
    REQUIRE(psj.Add(nwk, nid) == PersistentStorage::Status::PS_SUCCESS);
    nwk.mId      = nid;
    nwk.mChannel = 18;
    nwk.mName    = "nwk_upd";
    REQUIRE(psj.Update(nwk) == PersistentStorage::Status::PS_SUCCESS);

    Network returnValue;

    REQUIRE(psj.Get(NetworkId(0), returnValue) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(returnValue.mId.mId == 0);
    REQUIRE(returnValue.mName == "nwk_upd");
    REQUIRE(returnValue.mChannel == 18);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Upd br", "[ps_json]")
{
    // Make test independent
    unlink("./test.tmp");

    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    // Add initial data
    BorderRouterId newId;

    REQUIRE(psj.Add(BorderRouter{EMPTY_ID, EMPTY_ID,
                                 BorderAgent{"1.1.1.2", 11, ByteArray{}, "th1.x", BorderAgent::State{1, 0, 1, 0, 1},
                                             "NetworkId", 0x1011223344556677ll, "vendor_name", "model_name",
                                             Timestamp{0, 0, 0}, 1, "vendor_data", ByteArray{1, 2}, "Domain_name", 0, 0,
                                             "", 0, 0xFFFF}},
                    newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Add(BorderRouter{EMPTY_ID, EMPTY_ID,
                                 BorderAgent{"1.1.1.3", 12, ByteArray{}, "th1.x", BorderAgent::State{1, 0, 1, 0, 1},
                                             "NetworkId", 0x1011223344556677ll, "vendor_name", "model_name",
                                             Timestamp{0, 0, 0}, 1, "vendor_data", ByteArray{1, 2}, "Domain_name", 0, 0,
                                             "", 0, 0xFFFF}},
                    newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Add(BorderRouter{EMPTY_ID, EMPTY_ID,
                                 BorderAgent{"1.1.1.4", 13, ByteArray{}, "th1.x", BorderAgent::State{1, 0, 1, 0, 1},
                                             "NetworkId", 0x1011223344556677ll, "vendor_name", "model_name",
                                             Timestamp{0, 0, 0}, 1, "vendor_data", ByteArray{1, 2}, "Domain_name", 0, 0,
                                             "", 0, 0xFFFF}},
                    newId) == PersistentStorage::Status::PS_SUCCESS);

    // Test actions
    BorderRouter newValue{EMPTY_ID, EMPTY_ID,

                          BorderAgent{"5.5.5.5", 18, ByteArray{}, "th1.x", BorderAgent::State{0, 0, 2, 0, 0},
                                      "NetworkId", 0x1011223344556677ll, "vendor_name", "model_name",
                                      Timestamp{0, 0, 0}, 1, "vendor_data", ByteArray{1, 2}, "Domain_name", 0, 0, "", 0,
                                      0xFFFF}};

    REQUIRE(psj.Update(newValue) == PersistentStorage::Status::PS_NOT_FOUND);
    newValue.mId = 2;
    REQUIRE(psj.Update(newValue) == PersistentStorage::Status::PS_SUCCESS);

    BorderRouter returnValue;

    REQUIRE(psj.Get(BorderRouterId(2), returnValue) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(returnValue.mId.mId == 2);
    REQUIRE(returnValue.mAgent.mPort == 18);
    REQUIRE(returnValue.mAgent.mAddr == "5.5.5.5");

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Lookup Registrar", "[ps_json]")
{
    // Make test independent
    unlink("./test.tmp");

    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    //  Populate storage with test data
    RegistrarId newId;

    REQUIRE(psj.Add(Registrar{EMPTY_ID, "0.0.0.1", 1, {"dom1"}}, newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Add(Registrar{EMPTY_ID, "0.0.0.2", 1, {"dom2"}}, newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(psj.Add(Registrar{EMPTY_ID, "0.0.0.3", 3, {"dom3"}}, newId) == PersistentStorage::Status::PS_SUCCESS);

    // Test actions
    std::vector<Registrar> retLookup;

    Registrar searchReq{EMPTY_ID, "", 0, {}};
    REQUIRE(psj.Lookup(searchReq, retLookup) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(retLookup.size() == 3);

    retLookup.clear();

    searchReq.mId.mId = 0;
    REQUIRE(psj.Lookup(searchReq, retLookup) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(retLookup.size() == 1);
    REQUIRE(retLookup[0].mId.mId == 0);

    retLookup.clear();

    searchReq.mId.mId = 0;
    searchReq.mAddr   = "0.0.0.2";
    REQUIRE(psj.Lookup(searchReq, retLookup) == PersistentStorage::Status::PS_NOT_FOUND);
    REQUIRE(retLookup.size() == 0);

    retLookup.clear();

    searchReq.mId.mId = 0;
    searchReq.mAddr   = "0.0.0.1";
    REQUIRE(psj.Lookup(searchReq, retLookup) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(retLookup.size() == 1);
    REQUIRE(retLookup[0].mId.mId == 0);

    retLookup.clear();

    searchReq.mId.mId = 0;
    searchReq.mAddr   = "0.0.0.1";
    searchReq.mPort   = 1;
    REQUIRE(psj.Lookup(searchReq, retLookup) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(retLookup.size() == 1);
    REQUIRE(retLookup[0].mId.mId == 0);

    retLookup.clear();

    searchReq.mId.mId  = 0;
    searchReq.mAddr    = "0.0.0.1";
    searchReq.mPort    = 1;
    searchReq.mDomains = {"dom1"};
    REQUIRE(psj.Lookup(searchReq, retLookup) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(retLookup.size() == 1);
    REQUIRE(retLookup[0].mId.mId == 0);

    retLookup.clear();

    searchReq       = {};
    searchReq.mPort = 1;
    REQUIRE(psj.Lookup(searchReq, retLookup) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(retLookup.size() == 2);
    REQUIRE(retLookup[0].mId.mId == 0);
    REQUIRE(retLookup[1].mId.mId == 1);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}

TEST_CASE("Lookup Network", "[ps_json]")
{
    // Make test independent
    unlink("./test.tmp");

    PersistentStorageJson psj("./test.tmp");

    REQUIRE(psj.Open() == PersistentStorage::Status::PS_SUCCESS);

    // Populate storage with initial data
    NetworkId newId;
    REQUIRE(psj.Add(Network{EMPTY_ID, EMPTY_ID, "nwk1", XpanId{0xFFFFFFFFFFFFFFF1ll}, 11, "FFF1", "2000:aaa1::0/8", 1},
                    newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(newId.mId == 0);
    REQUIRE(psj.Add(Network{EMPTY_ID, EMPTY_ID, "nwk2", XpanId{0xFFFFFFFFFFFFFFF2ll}, 11, "FFF2", "2000:aaa2::0/8", 1},
                    newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(newId.mId == 1);
    REQUIRE(psj.Add(Network{EMPTY_ID, EMPTY_ID, "nwk3", XpanId{0xFFFFFFFFFFFFFFF3ll}, 11, "FFF3", "2000:aaa3::0/8", 1},
                    newId) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(newId.mId == 2);

    // The test
    std::vector<Network> retLookup;

    REQUIRE(psj.Lookup(Network{}, retLookup) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(retLookup.size() == 3);

    retLookup.clear();

    Network net;
    net.mName = "nwk1";
    net.mCcm  = true;
    REQUIRE(psj.Lookup(net, retLookup) == PersistentStorage::Status::PS_SUCCESS);
    REQUIRE(retLookup.size() == 1);

    REQUIRE(psj.Close() == PersistentStorage::Status::PS_SUCCESS);
}
