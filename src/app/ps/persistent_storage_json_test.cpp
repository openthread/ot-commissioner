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

#include <gtest/gtest.h>

#include "persistent_storage_json.hpp"
#include "app/border_agent.hpp"

#include <fstream>

using namespace ot::commissioner::persistent_storage;
using namespace ot::commissioner;

TEST(PSJson, CreateDefaultIfNotExists)
{
    PersistentStorageJson psj("./test_ps.json");

    EXPECT_TRUE(psj.Open() == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(psj.Close() == PersistentStorage::Status::kSuccess);
}

TEST(PSJson, ReadEmptyFile)
{
    std::ofstream testTmp("./test.tmp");
    testTmp.close();

    PersistentStorageJson psj("./test.tmp");

    EXPECT_TRUE(psj.Open() == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(psj.Close() == PersistentStorage::Status::kSuccess);
}

TEST(PSJson, ReadNonEmptyDefault)
{
    PersistentStorageJson psj("./test.tmp");

    EXPECT_TRUE(psj.Open() == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(psj.Close() == PersistentStorage::Status::kSuccess);
}

TEST(PSJson, AddRegistrar)
{
    PersistentStorageJson psj("./test.tmp");

    EXPECT_TRUE(psj.Open() == PersistentStorage::Status::kSuccess);

    RegistrarId newId;

    EXPECT_TRUE(psj.Add(Registrar{EMPTY_ID, "0.0.0.1", 1, {"dom1"}}, newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(newId.mId == 0);
    EXPECT_TRUE(psj.Add(Registrar{EMPTY_ID, "0.0.0.2", 2, {"dom2"}}, newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(newId.mId == 1);
    EXPECT_TRUE(psj.Add(Registrar{EMPTY_ID, "0.0.0.3", 3, {"dom3"}}, newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(newId.mId == 2);

    EXPECT_TRUE(psj.Close() == PersistentStorage::Status::kSuccess);
}

TEST(PSJson, AddDomain)
{
    PersistentStorageJson psj("./test.tmp");

    EXPECT_TRUE(psj.Open() == PersistentStorage::Status::kSuccess);

    DomainId newId;

    EXPECT_TRUE(psj.Add(Domain{EMPTY_ID, "dom1"}, newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(newId.mId == 0);
    EXPECT_TRUE(psj.Add(Domain{EMPTY_ID, "dom2"}, newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(newId.mId == 1);
    EXPECT_TRUE(psj.Add(Domain{EMPTY_ID, "dom3"}, newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(newId.mId == 2);

    EXPECT_TRUE(psj.Close() == PersistentStorage::Status::kSuccess);
}

TEST(PSJson, AddNetwork)
{
    // Make the test independent
    unlink("./test.tmp");

    PersistentStorageJson psj("./test.tmp");

    EXPECT_TRUE(psj.Open() == PersistentStorage::Status::kSuccess);

    NetworkId newId;

    EXPECT_TRUE(
        psj.Add(Network{EMPTY_ID, EMPTY_ID, "nwk1", XpanId{0xFFFFFFFFFFFFFFF1ll}, 11, 0xFFF1, "2000:aaa1::0/8", 1},
                newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(newId.mId == 0);
    EXPECT_TRUE(
        psj.Add(Network{EMPTY_ID, EMPTY_ID, "nwk2", XpanId{0xFFFFFFFFFFFFFFF2ll}, 11, 0xFFF2, "2000:aaa2::0/8", 1},
                newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(newId.mId == 1);
    EXPECT_TRUE(
        psj.Add(Network{EMPTY_ID, EMPTY_ID, "nwk3", XpanId{0xFFFFFFFFFFFFFFF3ll}, 11, 0xFFF3, "2000:aaa3::0/8", 1},
                newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(newId.mId == 2);

    EXPECT_TRUE(psj.Close() == PersistentStorage::Status::kSuccess);
}

TEST(PSJson, AddBorderRouter)
{
    PersistentStorageJson psj("./test.tmp");

    EXPECT_TRUE(psj.Open() == PersistentStorage::Status::kSuccess);

    BorderRouterId newId;

    EXPECT_TRUE(psj.Add(BorderRouter{EMPTY_ID, EMPTY_ID,
                                     BorderAgent{"1.1.1.2", 11, ByteArray{}, "th1.x", BorderAgent::State{1, 0, 1, 0, 1},
                                                 "NetworkId", 0x1011223344556677ll, "vendor_name", "model_name",
                                                 Timestamp{0, 0, 0}, 1, "vendor_data", ByteArray{1, 2}, "domain_name",
                                                 0, 0, "", 0, 0xFFFF}},
                        newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(newId.mId == 0);
    EXPECT_TRUE(psj.Add(BorderRouter{EMPTY_ID, EMPTY_ID,
                                     BorderAgent{"1.1.1.3", 12, ByteArray{}, "th1.x", BorderAgent::State{1, 0, 1, 0, 1},
                                                 "NetworkId", 0x1011223344556677ll, "vendor_name", "model_name",
                                                 Timestamp{0, 0, 0}, 1, "vendor_data", ByteArray{1, 2}, "domain_name",
                                                 0, 0, "", 0, 0xFFFF}},
                        newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(newId.mId == 1);
    EXPECT_TRUE(psj.Add(BorderRouter{EMPTY_ID, EMPTY_ID,
                                     BorderAgent{"1.1.1.4", 13, ByteArray{}, "th1.x", BorderAgent::State{1, 0, 1, 0, 1},
                                                 "NetworkId", 0x1011223344556677ll, "vendor_name", "model_name",
                                                 Timestamp{0, 0, 0}, 1, "vendor_data", ByteArray{1, 2}, "domain_name",
                                                 0, 0, "", 0, 0xFFFF}},
                        newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(newId.mId == 2);

    EXPECT_TRUE(psj.Close() == PersistentStorage::Status::kSuccess);
}

TEST(PSJson, DelRegistrar)
{
    PersistentStorageJson psj("./test.tmp");

    EXPECT_TRUE(psj.Open() == PersistentStorage::Status::kSuccess);

    EXPECT_TRUE(psj.Del(RegistrarId(0)) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(psj.Del(RegistrarId(1)) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(psj.Del(RegistrarId(2)) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(psj.Del(RegistrarId(50)) == PersistentStorage::Status::kSuccess);

    EXPECT_TRUE(psj.Close() == PersistentStorage::Status::kSuccess);
}

TEST(PSJson, DelDomain)
{
    PersistentStorageJson psj("./test.tmp");

    EXPECT_TRUE(psj.Open() == PersistentStorage::Status::kSuccess);

    EXPECT_TRUE(psj.Del(DomainId(0)) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(psj.Del(DomainId(1)) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(psj.Del(DomainId(2)) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(psj.Del(DomainId(50)) == PersistentStorage::Status::kSuccess);

    EXPECT_TRUE(psj.Close() == PersistentStorage::Status::kSuccess);
}

TEST(PSJson, DelNetwork)
{
    PersistentStorageJson psj("");

    EXPECT_TRUE(psj.Open() == PersistentStorage::Status::kSuccess);

    NetworkId newId;
    EXPECT_TRUE(
        psj.Add(Network{EMPTY_ID, EMPTY_ID, "nwk1", XpanId{0xFFFFFFFFFFFFFFF1ll}, 11, 0xFFF1, "2000:aaa1::0/8", 1},
                newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(newId.mId == 0);

    EXPECT_TRUE(psj.Del(NetworkId(0)) == PersistentStorage::Status::kSuccess);
    // Absent network successfully deletes newertheless
    EXPECT_TRUE(psj.Del(NetworkId(1)) == PersistentStorage::Status::kSuccess);

    NetworkArray nets;
    EXPECT_TRUE(psj.Lookup(Network{}, nets) == PersistentStorage::Status::kNotFound);

    EXPECT_TRUE(psj.Close() == PersistentStorage::Status::kSuccess);
}

TEST(PSJson, DelBorderRouter)
{
    PersistentStorageJson psj("./test.tmp");

    EXPECT_TRUE(psj.Open() == PersistentStorage::Status::kSuccess);

    EXPECT_TRUE(psj.Del(BorderRouterId(0)) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(psj.Del(BorderRouterId(1)) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(psj.Del(BorderRouterId(2)) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(psj.Del(BorderRouterId(50)) == PersistentStorage::Status::kSuccess);

    EXPECT_TRUE(psj.Close() == PersistentStorage::Status::kSuccess);
}

TEST(PSJson, GetRegistrarFromEmpty)
{
    PersistentStorageJson psj("./test.tmp");

    EXPECT_TRUE(psj.Open() == PersistentStorage::Status::kSuccess);

    Registrar returnValue;

    EXPECT_TRUE(psj.Get(RegistrarId(0), returnValue) == PersistentStorage::Status::kNotFound);

    EXPECT_TRUE(psj.Close() == PersistentStorage::Status::kSuccess);
}

TEST(PSJson, GetDomainFromEmpty)
{
    PersistentStorageJson psj("./test.tmp");

    EXPECT_TRUE(psj.Open() == PersistentStorage::Status::kSuccess);

    Domain returnValue;

    EXPECT_TRUE(psj.Get(DomainId(0), returnValue) == PersistentStorage::Status::kNotFound);

    EXPECT_TRUE(psj.Close() == PersistentStorage::Status::kSuccess);
}

TEST(PSJson, GetNetworkFromEmpty)
{
    PersistentStorageJson psj("");

    EXPECT_TRUE(psj.Open() == PersistentStorage::Status::kSuccess);

    Network returnValue;

    EXPECT_TRUE(psj.Get(NetworkId(0), returnValue) == PersistentStorage::Status::kNotFound);

    EXPECT_TRUE(psj.Close() == PersistentStorage::Status::kSuccess);
}

TEST(PSJson, GetBorderRouterFromEmpty)
{
    PersistentStorageJson psj("./test.tmp");

    EXPECT_TRUE(psj.Open() == PersistentStorage::Status::kSuccess);

    BorderRouter returnValue;

    EXPECT_TRUE(psj.Get(BorderRouterId(0), returnValue) == PersistentStorage::Status::kNotFound);

    EXPECT_TRUE(psj.Close() == PersistentStorage::Status::kSuccess);
}

TEST(PSJson, GetRegistrarNotEmpty)
{
    // Make test independent
    unlink("./test.tmp");

    PersistentStorageJson psj("./test.tmp");

    EXPECT_TRUE(psj.Open() == PersistentStorage::Status::kSuccess);

    RegistrarId newId;

    EXPECT_TRUE(psj.Add(Registrar{EMPTY_ID, "0.0.0.1", 1, {"dom1"}}, newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(newId.mId == 0);
    EXPECT_TRUE(psj.Add(Registrar{EMPTY_ID, "0.0.0.2", 2, {"dom2"}}, newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(newId.mId == 1);
    EXPECT_TRUE(psj.Add(Registrar{EMPTY_ID, "0.0.0.3", 3, {"dom3"}}, newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(newId.mId == 2);

    Registrar returnValue;

    EXPECT_TRUE(psj.Get(RegistrarId(3), returnValue) == PersistentStorage::Status::kNotFound);
    EXPECT_TRUE(psj.Get(RegistrarId(1), returnValue) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(returnValue.mId.mId == 1);
    EXPECT_TRUE(returnValue.mAddr == "0.0.0.2");
    EXPECT_TRUE(returnValue.mPort == 2);
    EXPECT_TRUE(returnValue.mDomains == std::vector<std::string>{"dom2"});

    EXPECT_TRUE(psj.Close() == PersistentStorage::Status::kSuccess);
}

TEST(PSJson, GetDomainNotEmpty)
{
    // Make test independent
    unlink("./test.tmp");

    PersistentStorageJson psj("./test.tmp");

    EXPECT_TRUE(psj.Open() == PersistentStorage::Status::kSuccess);

    DomainId newId;

    EXPECT_TRUE(psj.Add(Domain{EMPTY_ID, "dom1"}, newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(newId.mId == 0);
    EXPECT_TRUE(psj.Add(Domain{EMPTY_ID, "dom2"}, newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(newId.mId == 1);
    EXPECT_TRUE(psj.Add(Domain{EMPTY_ID, "dom3"}, newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(newId.mId == 2);

    Domain returnValue;

    EXPECT_TRUE(psj.Get(DomainId(3), returnValue) == PersistentStorage::Status::kNotFound);
    EXPECT_TRUE(psj.Get(DomainId(0), returnValue) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(returnValue.mId.mId == 0);
    EXPECT_TRUE(returnValue.mName == "dom1");

    EXPECT_TRUE(psj.Close() == PersistentStorage::Status::kSuccess);
}

TEST(PSJson, GetNetworkNotEmpty)
{
    PersistentStorageJson psj("./test.tmp");

    EXPECT_TRUE(psj.Open() == PersistentStorage::Status::kSuccess);

    NetworkId newId;

    EXPECT_TRUE(psj.Add(Network{EMPTY_ID, EMPTY_ID, "nwk1", 0xFFFFFFFFFFFFFFF1, 11, 0xFFF1, "2000:aaa1::0/8", 1},
                        newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(newId.mId == 0);
    EXPECT_TRUE(psj.Add(Network{EMPTY_ID, EMPTY_ID, "nwk2", 0xFFFFFFFFFFFFFFF2, 12, 0xFFF2, "2000:aaa2::0/8", 1},
                        newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(newId.mId == 1);
    EXPECT_TRUE(psj.Add(Network{EMPTY_ID, EMPTY_ID, "nwk3", 0xFFFFFFFFFFFFFFF3, 13, 0xFFF3, "2000:aaa3::0/8", 1},
                        newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(newId.mId == 2);

    Network returnValue;

    EXPECT_TRUE(psj.Get(NetworkId(5), returnValue) == PersistentStorage::Status::kNotFound);
    EXPECT_TRUE(psj.Get(NetworkId(0), returnValue) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(returnValue.mId.mId == 0);
    EXPECT_TRUE(returnValue.mName == "nwk1");
    EXPECT_TRUE(returnValue.mChannel == 11);

    EXPECT_TRUE(psj.Close() == PersistentStorage::Status::kSuccess);
}

TEST(PSJson, GetBorderRouterNotEmpty)
{
    // Make the test independent
    unlink("./test.tmp");

    PersistentStorageJson psj("./test.tmp");

    EXPECT_TRUE(psj.Open() == PersistentStorage::Status::kSuccess);

    BorderRouterId newId;

    EXPECT_TRUE(psj.Add(BorderRouter{EMPTY_ID, EMPTY_ID,
                                     BorderAgent{"1.1.1.2", 11, ByteArray{}, "th1.x", BorderAgent::State{1, 0, 1, 0, 1},
                                                 "NetworkId", 0x1011223344556677ll, "vendor_name", "model_name",
                                                 Timestamp{0, 0, 0}, 1, "vendor_data", ByteArray{1, 2}, "Domain_name",
                                                 0, 0, "", 0, 0xFFFF}},
                        newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(newId.mId == 0);
    EXPECT_TRUE(psj.Add(BorderRouter{EMPTY_ID, EMPTY_ID,
                                     BorderAgent{"1.1.1.3", 12, ByteArray{}, "th1.x", BorderAgent::State{1, 0, 1, 0, 1},
                                                 "NetworkId", 0x1011223344556677ll, "vendor_name", "model_name",
                                                 Timestamp{0, 0, 0}, 1, "vendor_data", ByteArray{1, 2}, "Domain_name",
                                                 0, 0, "", 0, 0xFFFF}},
                        newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(newId.mId == 1);
    EXPECT_TRUE(psj.Add(BorderRouter{EMPTY_ID, EMPTY_ID,
                                     BorderAgent{"1.1.1.4", 13, ByteArray{}, "th1.x", BorderAgent::State{1, 0, 1, 0, 1},
                                                 "NetworkId", 0x1011223344556677ll, "vendor_name", "model_name",
                                                 Timestamp{0, 0, 0}, 1, "vendor_data", ByteArray{1, 2}, "Domain_name",
                                                 0, 0, "", 0, 0xFFFF}},
                        newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(newId.mId == 2);

    BorderRouter returnValue;

    EXPECT_TRUE(psj.Get(BorderRouterId(3), returnValue) == PersistentStorage::Status::kNotFound);
    EXPECT_TRUE(psj.Get(BorderRouterId(1), returnValue) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(returnValue.mId.mId == 1);
    EXPECT_TRUE(returnValue.mAgent.mPort == 12);
    EXPECT_TRUE(returnValue.mAgent.mAddr == "1.1.1.3");

    EXPECT_TRUE(psj.Close() == PersistentStorage::Status::kSuccess);
}

// UPD
TEST(PSJson, UpdRegistrar)
{
    // Make the test independent
    unlink("./test.tmp");

    PersistentStorageJson psj("./test.tmp");

    EXPECT_TRUE(psj.Open() == PersistentStorage::Status::kSuccess);

    // Add initial data
    RegistrarId newId;

    EXPECT_TRUE(psj.Add(Registrar{EMPTY_ID, "0.0.0.1", 1, {"dom1"}}, newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(psj.Add(Registrar{EMPTY_ID, "0.0.0.2", 2, {"dom2"}}, newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(psj.Add(Registrar{EMPTY_ID, "0.0.0.3", 3, {"dom3"}}, newId) == PersistentStorage::Status::kSuccess);

    // Test actions
    Registrar newValue{EMPTY_ID, "4.4.4.4", 1, {"dom4"}};

    EXPECT_TRUE(psj.Update(newValue) == PersistentStorage::Status::kNotFound);
    newValue.mId = 2;
    EXPECT_TRUE(psj.Update(newValue) == PersistentStorage::Status::kSuccess);

    Registrar returnValue;

    EXPECT_TRUE(psj.Get(RegistrarId(2), returnValue) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(returnValue.mId.mId == 2);
    EXPECT_TRUE(returnValue.mAddr == "4.4.4.4");
    EXPECT_TRUE(returnValue.mPort == 1);
    EXPECT_TRUE(returnValue.mDomains == std::vector<std::string>{"dom4"});

    EXPECT_TRUE(psj.Close() == PersistentStorage::Status::kSuccess);
}

TEST(PSJson, UpdDomain)
{
    // Make the test independent
    unlink("./test.tmp");

    PersistentStorageJson psj("./test.tmp");

    EXPECT_TRUE(psj.Open() == PersistentStorage::Status::kSuccess);

    // Add initial data
    DomainId newId;

    EXPECT_TRUE(psj.Add(Domain{EMPTY_ID, "dom1"}, newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(psj.Add(Domain{EMPTY_ID, "dom2"}, newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(psj.Add(Domain{EMPTY_ID, "dom3"}, newId) == PersistentStorage::Status::kSuccess);

    // Test actions
    Domain newValue{EMPTY_ID, "dom_upd"};

    EXPECT_TRUE(psj.Update(newValue) == PersistentStorage::Status::kNotFound);
    newValue.mId = 1;
    EXPECT_TRUE(psj.Update(newValue) == PersistentStorage::Status::kSuccess);

    Domain returnValue;

    EXPECT_TRUE(psj.Get(DomainId(1), returnValue) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(returnValue.mId.mId == 1);
    EXPECT_TRUE(returnValue.mName == "dom_upd");

    EXPECT_TRUE(psj.Close() == PersistentStorage::Status::kSuccess);
}

TEST(PSJson, UpdNetwork)
{
    unlink("./tmp.json");
    PersistentStorageJson psj("./tmp.json");

    EXPECT_TRUE(psj.Open() == PersistentStorage::Status::kSuccess);

    Network nwk{EMPTY_ID, EMPTY_ID, "nwk", 0xFFFFFFFFFFFFFFFA, 17, 0xFFFA, "2000:aaa1::0/64", 0};

    EXPECT_TRUE(psj.Update(nwk) == PersistentStorage::Status::kNotFound);
    NetworkId nid;
    EXPECT_TRUE(psj.Add(nwk, nid) == PersistentStorage::Status::kSuccess);
    nwk.mId      = nid;
    nwk.mChannel = 18;
    nwk.mName    = "nwk_upd";
    EXPECT_TRUE(psj.Update(nwk) == PersistentStorage::Status::kSuccess);

    Network returnValue;

    EXPECT_TRUE(psj.Get(NetworkId(0), returnValue) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(returnValue.mId.mId == 0);
    EXPECT_TRUE(returnValue.mName == "nwk_upd");
    EXPECT_TRUE(returnValue.mChannel == 18);

    EXPECT_TRUE(psj.Close() == PersistentStorage::Status::kSuccess);
}

TEST(PSJson, UpdBorderRouter)
{
    // Make test independent
    unlink("./test.tmp");

    PersistentStorageJson psj("./test.tmp");

    EXPECT_TRUE(psj.Open() == PersistentStorage::Status::kSuccess);

    // Add initial data
    BorderRouterId newId;

    EXPECT_TRUE(psj.Add(BorderRouter{EMPTY_ID, EMPTY_ID,
                                     BorderAgent{"1.1.1.2", 11, ByteArray{}, "th1.x", BorderAgent::State{1, 0, 1, 0, 1},
                                                 "NetworkId", 0x1011223344556677ll, "vendor_name", "model_name",
                                                 Timestamp{0, 0, 0}, 1, "vendor_data", ByteArray{1, 2}, "Domain_name",
                                                 0, 0, "", 0, 0xFFFF}},
                        newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(psj.Add(BorderRouter{EMPTY_ID, EMPTY_ID,
                                     BorderAgent{"1.1.1.3", 12, ByteArray{}, "th1.x", BorderAgent::State{1, 0, 1, 0, 1},
                                                 "NetworkId", 0x1011223344556677ll, "vendor_name", "model_name",
                                                 Timestamp{0, 0, 0}, 1, "vendor_data", ByteArray{1, 2}, "Domain_name",
                                                 0, 0, "", 0, 0xFFFF}},
                        newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(psj.Add(BorderRouter{EMPTY_ID, EMPTY_ID,
                                     BorderAgent{"1.1.1.4", 13, ByteArray{}, "th1.x", BorderAgent::State{1, 0, 1, 0, 1},
                                                 "NetworkId", 0x1011223344556677ll, "vendor_name", "model_name",
                                                 Timestamp{0, 0, 0}, 1, "vendor_data", ByteArray{1, 2}, "Domain_name",
                                                 0, 0, "", 0, 0xFFFF}},
                        newId) == PersistentStorage::Status::kSuccess);

    // Test actions
    BorderRouter newValue{EMPTY_ID, EMPTY_ID,

                          BorderAgent{"5.5.5.5", 18, ByteArray{}, "th1.x", BorderAgent::State{0, 0, 2, 0, 0},
                                      "NetworkId", 0x1011223344556677ll, "vendor_name", "model_name",
                                      Timestamp{0, 0, 0}, 1, "vendor_data", ByteArray{1, 2}, "Domain_name", 0, 0, "", 0,
                                      0xFFFF}};

    EXPECT_TRUE(psj.Update(newValue) == PersistentStorage::Status::kNotFound);
    newValue.mId = 2;
    EXPECT_TRUE(psj.Update(newValue) == PersistentStorage::Status::kSuccess);

    BorderRouter returnValue;

    EXPECT_TRUE(psj.Get(BorderRouterId(2), returnValue) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(returnValue.mId.mId == 2);
    EXPECT_TRUE(returnValue.mAgent.mPort == 18);
    EXPECT_TRUE(returnValue.mAgent.mAddr == "5.5.5.5");

    EXPECT_TRUE(psj.Close() == PersistentStorage::Status::kSuccess);
}

TEST(PSJson, LookupRegistrar)
{
    // Make test independent
    unlink("./test.tmp");

    PersistentStorageJson psj("./test.tmp");

    EXPECT_TRUE(psj.Open() == PersistentStorage::Status::kSuccess);

    //  Populate storage with test data
    RegistrarId newId;

    EXPECT_TRUE(psj.Add(Registrar{EMPTY_ID, "0.0.0.1", 1, {"dom1"}}, newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(psj.Add(Registrar{EMPTY_ID, "0.0.0.2", 1, {"dom2"}}, newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(psj.Add(Registrar{EMPTY_ID, "0.0.0.3", 3, {"dom3"}}, newId) == PersistentStorage::Status::kSuccess);

    // Test actions
    std::vector<Registrar> retLookup;

    Registrar searchReq{EMPTY_ID, "", 0, {}};
    EXPECT_TRUE(psj.Lookup(searchReq, retLookup) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(retLookup.size() == 3);

    retLookup.clear();

    searchReq.mId.mId = 0;
    EXPECT_TRUE(psj.Lookup(searchReq, retLookup) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(retLookup.size() == 1);
    EXPECT_TRUE(retLookup[0].mId.mId == 0);

    retLookup.clear();

    searchReq.mId.mId = 0;
    searchReq.mAddr   = "0.0.0.2";
    EXPECT_TRUE(psj.Lookup(searchReq, retLookup) == PersistentStorage::Status::kNotFound);
    EXPECT_TRUE(retLookup.size() == 0);

    retLookup.clear();

    searchReq.mId.mId = 0;
    searchReq.mAddr   = "0.0.0.1";
    EXPECT_TRUE(psj.Lookup(searchReq, retLookup) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(retLookup.size() == 1);
    EXPECT_TRUE(retLookup[0].mId.mId == 0);

    retLookup.clear();

    searchReq.mId.mId = 0;
    searchReq.mAddr   = "0.0.0.1";
    searchReq.mPort   = 1;
    EXPECT_TRUE(psj.Lookup(searchReq, retLookup) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(retLookup.size() == 1);
    EXPECT_TRUE(retLookup[0].mId.mId == 0);

    retLookup.clear();

    searchReq.mId.mId  = 0;
    searchReq.mAddr    = "0.0.0.1";
    searchReq.mPort    = 1;
    searchReq.mDomains = {"dom1"};
    EXPECT_TRUE(psj.Lookup(searchReq, retLookup) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(retLookup.size() == 1);
    EXPECT_TRUE(retLookup[0].mId.mId == 0);

    retLookup.clear();

    searchReq       = {};
    searchReq.mPort = 1;
    EXPECT_TRUE(psj.Lookup(searchReq, retLookup) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(retLookup.size() == 2);
    EXPECT_TRUE(retLookup[0].mId.mId == 0);
    EXPECT_TRUE(retLookup[1].mId.mId == 1);

    EXPECT_TRUE(psj.Close() == PersistentStorage::Status::kSuccess);
}

TEST(PSJson, LookupNetwork)
{
    // Make test independent
    unlink("./test.tmp");

    PersistentStorageJson psj("./test.tmp");

    EXPECT_TRUE(psj.Open() == PersistentStorage::Status::kSuccess);

    // Populate storage with initial data
    NetworkId newId;
    EXPECT_TRUE(
        psj.Add(Network{EMPTY_ID, EMPTY_ID, "nwk1", XpanId{0xFFFFFFFFFFFFFFF1ll}, 11, 0xFFF1, "2000:aaa1::0/8", 1},
                newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(newId.mId == 0);
    EXPECT_TRUE(
        psj.Add(Network{EMPTY_ID, EMPTY_ID, "nwk2", XpanId{0xFFFFFFFFFFFFFFF2ll}, 11, 0xFFF2, "2000:aaa2::0/8", 1},
                newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(newId.mId == 1);
    EXPECT_TRUE(
        psj.Add(Network{EMPTY_ID, EMPTY_ID, "nwk3", XpanId{0xFFFFFFFFFFFFFFF3ll}, 11, 0xFFF3, "2000:aaa3::0/8", 1},
                newId) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(newId.mId == 2);

    // The test
    std::vector<Network> retLookup;

    EXPECT_TRUE(psj.Lookup(Network{}, retLookup) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(retLookup.size() == 3);

    retLookup.clear();

    Network net;
    net.mName = "nwk1";
    net.mCcm  = true;
    EXPECT_TRUE(psj.Lookup(net, retLookup) == PersistentStorage::Status::kSuccess);
    EXPECT_TRUE(retLookup.size() == 1);

    EXPECT_TRUE(psj.Close() == PersistentStorage::Status::kSuccess);
}
