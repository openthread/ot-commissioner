/*
 *    Copyright (c) 2021, The OpenThread Commissioner Authors.
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
 * @file Interpreter unit tests
 */

#include "gmock/gmock.h"

#include <fmt/format.h>

#define private public

#include "app/border_agent_functions_mock.hpp"
#include "app/cli/interpreter.hpp"
#include "app/cli/job_manager.hpp"
#include "app/commissioner_app_mock.hpp"
#include "app/file_util.hpp"
#include "app/ps/persistent_storage_json.hpp"
#include "app/ps/registry.hpp"
#include "app/ps/registry_entries.hpp"

using namespace ot::commissioner;
using namespace ot::commissioner::persistent_storage;

using testing::_;
using testing::DoAll;
using testing::Mock;
using testing::Return;
using testing::ReturnRef;
using testing::StrEq;
using testing::WithArg;

using Json = nlohmann::json;

class InterpreterTestSuite : public testing::Test
{
public:
    using RegistryStatus = Registry::Status;
    typedef std::shared_ptr<CommissionerAppMock> CommissionerAppMockPtr;

    InterpreterTestSuite()          = default;
    virtual ~InterpreterTestSuite() = default;

    struct TestContext
    {
        TestContext() { SetCommissionerAppStaticExpecter(&mCommissionerAppStaticExpecter); }
        ~TestContext() { ClearCommissionerAppStaticExpecter(); }

        Interpreter                   mInterpreter;
        Registry *                    mRegistry = nullptr;
        CommissionerAppMockPtr        mDefaultCommissionerObject{new CommissionerAppMock()};
        CommissionerAppStaticExpecter mCommissionerAppStaticExpecter;
    };

    void InitContext(TestContext &ctx)
    {
        // Minimum test setup: create config file
        const std::string configFile = "./config";
        auto              error      = WriteFile("{\"ThreadSMRoot\": \"./\"}", configFile);
        ASSERT_EQ(error.mCode, ErrorCode::kNone);

        ASSERT_NE(ctx.mDefaultCommissionerObject, nullptr);

        EXPECT_CALL(ctx.mCommissionerAppStaticExpecter, Create(_, _))
            .WillOnce(
                DoAll(WithArg<0>([&](std::shared_ptr<CommissionerApp> &a) { a = ctx.mDefaultCommissionerObject; }),
                      Return(Error{})));

        auto result = ctx.mInterpreter.Init("./config", "");
        ASSERT_EQ(result.mCode, ErrorCode::kNone);

        ctx.mRegistry = ctx.mInterpreter.mRegistry.get();

        // Add formal default PSKc
        ctx.mInterpreter.mJobManager->mDefaultConf.mPSKc = {'1', '0'};
    }
};

TEST_F(InterpreterTestSuite, TestInit)
{
    TestContext ctx;
    InitContext(ctx);
}

// Multi-network syntax validation (MNSV) test group
TEST_F(InterpreterTestSuite, MNSV_ValidSyntaxPass)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    BorderRouter br;
    br.mNetworkId = 0;
    ASSERT_EQ(ctx.mRegistry->SetCurrentNetwork(br), RegistryStatus::kSuccess);

    std::string             command;
    Interpreter::Expression expr;
    Interpreter::Expression ret;
    XpanIdArray             nids;

    command = "start --nwk all";
    expr    = ctx.mInterpreter.ParseExpression(command);
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_TRUE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
    ctx.mInterpreter.mContext.Cleanup();
    ctx.mInterpreter.mJobManager->CleanupJobs();
    ret.clear();
    nids.clear();

    command = "start --nwk this";
    expr    = ctx.mInterpreter.ParseExpression(command);
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_TRUE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
    ctx.mInterpreter.mContext.Cleanup();
    ctx.mInterpreter.mJobManager->CleanupJobs();
    ret.clear();
    nids.clear();

    command = "start --nwk other";
    expr    = ctx.mInterpreter.ParseExpression(command);
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_TRUE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
    ctx.mInterpreter.mContext.Cleanup();
    ctx.mInterpreter.mJobManager->CleanupJobs();
    ret.clear();
    nids.clear();

    command = "start --nwk net1 net2";
    expr    = ctx.mInterpreter.ParseExpression(command);
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_TRUE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
    ctx.mInterpreter.mContext.Cleanup();
    ctx.mInterpreter.mJobManager->CleanupJobs();
    ret.clear();
    nids.clear();

    command = "start --dom domain1";
    expr    = ctx.mInterpreter.ParseExpression(command);
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_TRUE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
    ctx.mInterpreter.mContext.Cleanup();
    ctx.mInterpreter.mJobManager->CleanupJobs();
    ret.clear();
    nids.clear();
}

TEST_F(InterpreterTestSuite, MNSV_TwoGroupNwkAliasesFail)
{
    TestContext ctx;
    InitContext(ctx);

    Interpreter::Expression expr, ret;
    XpanIdArray             nids;
    expr = ctx.mInterpreter.ParseExpression("start --nwk all other");
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_FALSE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
}

TEST_F(InterpreterTestSuite, MNSV_ThisResolvesWithCurrentSet)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    BorderRouter br;
    br.mNetworkId = 0;
    ASSERT_EQ(ctx.mRegistry->SetCurrentNetwork(br), RegistryStatus::kSuccess);

    Interpreter::Expression expr, ret;
    XpanIdArray             nids;
    expr = ctx.mInterpreter.ParseExpression("start --nwk this");
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_TRUE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
}

TEST_F(InterpreterTestSuite, MNSV_ThisUnresolvesWithCurrentUnset)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);

    Interpreter::Expression expr, ret;
    XpanIdArray             nids;
    expr = ctx.mInterpreter.ParseExpression("start --nwk this");
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_FALSE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
    EXPECT_EQ(nids.size(), 0);
}

TEST_F(InterpreterTestSuite, MNSV_AllOtherSameWithCurrentUnselected)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    // No current network selected

    Interpreter::Expression expr, ret;
    XpanIdArray             nids;
    expr = ctx.mInterpreter.ParseExpression("start --nwk all");
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_TRUE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
    EXPECT_NE(std::find(nids.begin(), nids.end(), XpanId{1}), nids.end());
    EXPECT_NE(std::find(nids.begin(), nids.end(), XpanId{2}), nids.end());
    ctx.mInterpreter.mContext.Cleanup();
    ctx.mInterpreter.mJobManager->CleanupJobs();
    ret.clear();
    nids.clear();

    expr = ctx.mInterpreter.ParseExpression("start --nwk other");
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_TRUE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
    EXPECT_NE(std::find(nids.begin(), nids.end(), XpanId{1}), nids.end());
    EXPECT_NE(std::find(nids.begin(), nids.end(), XpanId{2}), nids.end());
    ctx.mInterpreter.mContext.Cleanup();
    ctx.mInterpreter.mJobManager->CleanupJobs();
    ret.clear();
    nids.clear();
}

TEST_F(InterpreterTestSuite, MNSV_AllOtherDifferWithCurrentSelected)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    BorderRouter br;
    br.mNetworkId = 0;
    ASSERT_EQ(ctx.mRegistry->SetCurrentNetwork(br), RegistryStatus::kSuccess);

    Interpreter::Expression expr, ret;
    XpanIdArray             nids;
    expr = ctx.mInterpreter.ParseExpression("start --nwk all");
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_TRUE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
    EXPECT_NE(std::find(nids.begin(), nids.end(), XpanId{1}), nids.end());
    EXPECT_NE(std::find(nids.begin(), nids.end(), XpanId{2}), nids.end());
    ctx.mInterpreter.mContext.Cleanup();
    ctx.mInterpreter.mJobManager->CleanupJobs();
    ret.clear();
    nids.clear();

    expr = ctx.mInterpreter.ParseExpression("start --nwk other");
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_TRUE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
    EXPECT_EQ(std::find(nids.begin(), nids.end(), XpanId{1}), nids.end());
    EXPECT_NE(std::find(nids.begin(), nids.end(), XpanId{2}), nids.end());
    ctx.mInterpreter.mContext.Cleanup();
    ctx.mInterpreter.mJobManager->CleanupJobs();
    ret.clear();
    nids.clear();
}

TEST_F(InterpreterTestSuite, MNSV_TwoDomSwitchesFail)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);

    Interpreter::Expression expr, ret;
    XpanIdArray             nids;
    expr = ctx.mInterpreter.ParseExpression("start --dom domain1 --dom domain2");
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_FALSE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
}

TEST_F(InterpreterTestSuite, MNSV_UnexistingDomainResolveFails)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);

    Interpreter::Expression expr, ret;
    XpanIdArray             nids;
    expr = ctx.mInterpreter.ParseExpression("start --dom domain2");
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_FALSE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
}

TEST_F(InterpreterTestSuite, MNSV_ExistingDomainResolves)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);

    Interpreter::Expression expr, ret;
    XpanIdArray             nids;
    expr = ctx.mInterpreter.ParseExpression("start --dom domain1");
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_TRUE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
    EXPECT_NE(std::find(nids.begin(), nids.end(), XpanId{1}), nids.end());
    EXPECT_EQ(std::find(nids.begin(), nids.end(), XpanId{2}), nids.end());
}

TEST_F(InterpreterTestSuite, MNSV_AmbiguousNwkResolutionFails)
{
    TestContext ctx;
    InitContext(ctx);

    NetworkId nid;
    ASSERT_EQ(ctx.mRegistry->mStorage->Add(Network{EMPTY_ID, EMPTY_ID, "net1", 1, 0, "pan1", "", 0}, nid),
              PersistentStorage::Status::PS_SUCCESS);
    ASSERT_EQ(ctx.mRegistry->mStorage->Add(Network{EMPTY_ID, EMPTY_ID, "net2", 2, 0, "pan1", "", 0}, nid),
              PersistentStorage::Status::PS_SUCCESS);

    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);

    Interpreter::Expression expr, ret;
    XpanIdArray             nids;
    expr = ctx.mInterpreter.ParseExpression("start --nwk pan1");
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_FALSE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
}

TEST_F(InterpreterTestSuite, MNSV_SameResolutionFromTwoAliasesCollapses)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);

    Interpreter::Expression expr, ret;
    XpanIdArray             nids;
    expr = ctx.mInterpreter.ParseExpression("start --nwk 1 net1");
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_TRUE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
    EXPECT_EQ(nids.size(), 1);
}

TEST_F(InterpreterTestSuite, MNSV_GroupAndIndividualNwkAliasesMustFail)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);

    Interpreter::Expression expr, ret;
    XpanIdArray             nids;
    expr = ctx.mInterpreter.ParseExpression("start --nwk 1 all");
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_FALSE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
}

TEST_F(InterpreterTestSuite, MNSV_DomThisResolvesWithRespectToSelection)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    BorderRouter br;
    br.mNetworkId = 0;
    ASSERT_EQ(ctx.mRegistry->SetCurrentNetwork(br), RegistryStatus::kSuccess);

    Interpreter::Expression expr, ret;
    XpanIdArray             nids;
    expr = ctx.mInterpreter.ParseExpression("start --dom this");
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_TRUE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
    EXPECT_EQ(nids.size(), 1);
    EXPECT_EQ(nids[0], XpanId{1});
}

TEST_F(InterpreterTestSuite, MNSV_NoAliasesResolvesToThisNwk)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.3", 20003, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net3", 3, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.4", 20004, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net4", 4, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    BorderRouter br;
    br.mNetworkId = 2;
    ASSERT_EQ(ctx.mRegistry->SetCurrentNetwork(br), RegistryStatus::kSuccess);
    XpanId nid;
    ASSERT_EQ(ctx.mRegistry->GetCurrentNetworkXpan(nid), RegistryStatus::kSuccess);
    ASSERT_EQ(nid, XpanId{3});

    Interpreter::Expression expr, ret;
    XpanIdArray             nids;
    expr = ctx.mInterpreter.ParseExpression("start");

    CommissionerAppMockPtr pcaMock{new CommissionerAppMock()};
    EXPECT_CALL(ctx.mCommissionerAppStaticExpecter, Create(_, _))
        .WillOnce(DoAll(WithArg<0>([&](std::shared_ptr<CommissionerApp> &a) { a = pcaMock; }), Return(Error{})));

    EXPECT_CALL(*pcaMock, Start(_, StrEq("127.0.0.3"), 20003)).Times(1).WillOnce(Return(Error{}));
    EXPECT_TRUE(ctx.mInterpreter.Eval(expr).HasNoError());
}

TEST_F(InterpreterTestSuite, MNSV_EmptyNwkOrDomMustFail)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.3", 20003, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net3", 3, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.4", 20004, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net4", 4, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    BorderRouter br;
    br.mNetworkId = 0;
    ASSERT_EQ(ctx.mRegistry->SetCurrentNetwork(br), RegistryStatus::kSuccess);

    Interpreter::Expression expr, ret;
    expr = ctx.mInterpreter.ParseExpression("start --nwk");
    EXPECT_NE(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);

    expr = ctx.mInterpreter.ParseExpression("start --dom");
    EXPECT_NE(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
}

// Import/Export Syntax Validation test group
TEST_F(InterpreterTestSuite, IESV_SingleExportFileMustPass)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);

    Interpreter::Expression expr;
    expr = ctx.mInterpreter.ParseExpression("br scan --timeout 1 --export ./2.json");

    Interpreter::Value value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, IESV_SingleImportFileMustPass)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);

    CommissionerAppMockPtr commissionerAppMock{new CommissionerAppMock()};
    EXPECT_CALL(ctx.mCommissionerAppStaticExpecter, Create(_, _))
        .WillOnce(
            DoAll(WithArg<0>([&](std::shared_ptr<CommissionerApp> &a) { a = commissionerAppMock; }), Return(Error{})));
    EXPECT_CALL(*commissionerAppMock, Start(_, _, _)).WillOnce(Return(Error{}));

    // Create CommissionerAppMock for the network
    Interpreter::Expression expr;
    expr = ctx.mInterpreter.ParseExpression("start --nwk net1");
    EXPECT_TRUE(ctx.mInterpreter.Eval(expr).HasNoError());
    ctx.mInterpreter.mContext.Cleanup();
    ctx.mInterpreter.mJobManager->CleanupJobs();

    BorderRouter br;
    br.mNetworkId = 0;
    ASSERT_EQ(ctx.mRegistry->SetCurrentNetwork(br), RegistryStatus::kSuccess);

    // Attention: changed Channel->Number and SecurityPolicy->Flags
    std::string jsonStr = "{\n\
    \"ActiveTimestamp\": {\n\
        \"Seconds\": 56, // 48 bits\n\
        \"Ticks\": 44, // 15 bits\n\
        \"U\": 1 // 1 bit\n\
    },\n\
    \"Channel\": {\n\
        \"Number\": 19,\n\
        \"Page\": 0\n\
    },\n\
    \"ChannelMask\": [\n\
        {\n\
            \"Length\": 4,\n\
            \"Masks\": \"001fffe0\", // ByteArray in hex string.\n\
            \"Page\": 0\n\
        }\n\
    ],\n\
    \"ExtendedPanId\": \"dead00beef00cafe\",\n\
    \"NetworkName\": \"test-active\",\n\
    \"PSKc\": \"3aa55f91ca47d1e4e71a08cb35e91591\", // ByteArray in hex string.\n\
    \"PanId\": \"0xface\", // 0xface\n\
    \"SecurityPolicy\": {\n\
        \"Flags\": \"f8\", // 0xf8\n\
        \"RotationTime\": 672\n\
    }\n\
}";

    EXPECT_EQ(WriteFile(jsonStr, "./json.json").mCode, ErrorCode::kNone);

    EXPECT_CALL(*commissionerAppMock, SetActiveDataset(_)).WillOnce(Return(Error{}));
    expr       = ctx.mInterpreter.ParseExpression("opdataset set active --import ./json.json");
    auto value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, IESV_NoExportFileMustFail)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);

    Interpreter::Expression expr;
    Interpreter::Value      value;
    expr  = ctx.mInterpreter.ParseExpression("br scan --export");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_FALSE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, IESV_NoImportFileMustFail)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);

    CommissionerAppMockPtr commissionerAppMock{new CommissionerAppMock()};
    EXPECT_CALL(ctx.mCommissionerAppStaticExpecter, Create(_, _))
        .WillOnce(
            DoAll(WithArg<0>([&](std::shared_ptr<CommissionerApp> &a) { a = commissionerAppMock; }), Return(Error{})));
    EXPECT_CALL(*commissionerAppMock, Start(_, _, _)).WillOnce(Return(Error{}));

    // Create CommissionerAppMock for the network
    Interpreter::Expression expr;
    expr = ctx.mInterpreter.ParseExpression("start --nwk 1");
    EXPECT_TRUE(ctx.mInterpreter.Eval(expr).HasNoError());
    ctx.mInterpreter.mContext.Cleanup();
    ctx.mInterpreter.mJobManager->CleanupJobs();

    BorderRouter br;
    br.mNetworkId = 0;
    ASSERT_EQ(ctx.mRegistry->SetCurrentNetwork(br), RegistryStatus::kSuccess);

    expr = ctx.mInterpreter.ParseExpression("opdataset set active --import");
    EXPECT_FALSE(ctx.mInterpreter.Eval(expr).HasNoError());
}

TEST_F(InterpreterTestSuite, IESV_TwoImportExportClausesMustFail)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);

    Interpreter::Expression expr;
    Interpreter::Value      value;
    expr  = ctx.mInterpreter.ParseExpression("br scan --export 1.json --export 2.json");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_FALSE(value.HasNoError());
    ctx.mInterpreter.mContext.Cleanup();
    ctx.mInterpreter.mJobManager->CleanupJobs();

    expr = ctx.mInterpreter.ParseExpression("opdataset set active --import 1.json --import 2.json");
    EXPECT_FALSE(ctx.mInterpreter.Eval(expr).HasNoError());
}

// Collect Multi-Network Output test group
TEST_F(InterpreterTestSuite, CMNO_MultipleSuccessfullJobsPass)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);

    uint8_t                camIdx                  = 0;
    CommissionerAppMockPtr commissionerAppMocks[2] = {CommissionerAppMockPtr{new CommissionerAppMock()},
                                                      CommissionerAppMockPtr{new CommissionerAppMock()}};
    EXPECT_CALL(ctx.mCommissionerAppStaticExpecter, Create(_, _))
        .Times(2)
        .WillRepeatedly(
            DoAll(WithArg<0>([&](std::shared_ptr<CommissionerApp> &a) { a = commissionerAppMocks[camIdx++]; }),
                  Return(Error{})));
    EXPECT_CALL(*commissionerAppMocks[0], Start(_, _, _)).Times(1).WillOnce(Return(Error{}));
    EXPECT_CALL(*commissionerAppMocks[1], Start(_, _, _)).Times(1).WillOnce(Return(Error{}));

    Interpreter::Expression expr;
    Interpreter::Value      value;
    expr  = ctx.mInterpreter.ParseExpression("start --nwk net1 net2");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, CMNO_UnsuccessfullResultFromAJobMustNotFail)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);

    uint8_t                camIdx                  = 0;
    CommissionerAppMockPtr commissionerAppMocks[2] = {CommissionerAppMockPtr{new CommissionerAppMock()},
                                                      CommissionerAppMockPtr{new CommissionerAppMock()}};
    EXPECT_CALL(ctx.mCommissionerAppStaticExpecter, Create(_, _))
        .Times(2)
        .WillRepeatedly(
            DoAll(WithArg<0>([&](std::shared_ptr<CommissionerApp> &a) { a = commissionerAppMocks[camIdx++]; }),
                  Return(Error{})));
    EXPECT_CALL(*commissionerAppMocks[0], Start(_, _, _)).Times(1).WillOnce(Return(Error{}));
    EXPECT_CALL(*commissionerAppMocks[1], Start(_, _, _))
        .Times(1)
        .WillOnce(Return(Error{ErrorCode::kAborted, "Test failure"}));

    Interpreter::Expression expr;
    Interpreter::Value      value;
    expr  = ctx.mInterpreter.ParseExpression("start --nwk net1 net2");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

// Per-Command tests group
TEST_F(InterpreterTestSuite, PC_StartNetworkSyntaxSuccess)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);

    uint8_t                camIdx                  = 0;
    CommissionerAppMockPtr commissionerAppMocks[2] = {CommissionerAppMockPtr{new CommissionerAppMock()},
                                                      CommissionerAppMockPtr{new CommissionerAppMock()}};
    EXPECT_CALL(ctx.mCommissionerAppStaticExpecter, Create(_, _))
        .Times(2)
        .WillRepeatedly(
            DoAll(WithArg<0>([&](std::shared_ptr<CommissionerApp> &a) { a = commissionerAppMocks[camIdx++]; }),
                  Return(Error{})));
    EXPECT_CALL(*commissionerAppMocks[0], Start(_, _, _)).Times(3).WillRepeatedly(Return(Error{}));
    // Will be omitted on domain start
    EXPECT_CALL(*commissionerAppMocks[1], Start(_, _, _)).Times(2).WillRepeatedly(Return(Error{}));

    Interpreter::Expression expr;
    Interpreter::Value      value;
    expr  = ctx.mInterpreter.ParseExpression("start --nwk net1 net2");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    ctx.mInterpreter.mContext.Cleanup();
    ctx.mInterpreter.mJobManager->CleanupJobs();

    expr  = ctx.mInterpreter.ParseExpression("start --nwk all");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    ctx.mInterpreter.mContext.Cleanup();
    ctx.mInterpreter.mJobManager->CleanupJobs();

    expr  = ctx.mInterpreter.ParseExpression("start --dom domain1");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_StartCurrentNetworkSuccess)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);

    BorderRouter br;
    br.mNetworkId = 0;
    ASSERT_EQ(ctx.mRegistry->SetCurrentNetwork(br), RegistryStatus::kSuccess);

    uint8_t                camIdx                  = 0;
    CommissionerAppMockPtr commissionerAppMocks[2] = {CommissionerAppMockPtr{new CommissionerAppMock()},
                                                      CommissionerAppMockPtr{new CommissionerAppMock()}};
    EXPECT_CALL(ctx.mCommissionerAppStaticExpecter, Create(_, _))
        .Times(2)
        .WillRepeatedly(
            DoAll(WithArg<0>([&](std::shared_ptr<CommissionerApp> &a) { a = commissionerAppMocks[camIdx++]; }),
                  Return(Error{})));
    EXPECT_CALL(*commissionerAppMocks[0], Start(_, _, _)).Times(2).WillRepeatedly(Return(Error{}));
    // Will be omitted on domain start
    EXPECT_CALL(*commissionerAppMocks[1], Start(_, _, _)).Times(1).WillRepeatedly(Return(Error{}));

    Interpreter::Expression expr;
    Interpreter::Value      value;
    expr  = ctx.mInterpreter.ParseExpression("start --nwk this");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    ctx.mInterpreter.mContext.Cleanup();
    ctx.mInterpreter.mJobManager->CleanupJobs();

    expr  = ctx.mInterpreter.ParseExpression("start --dom this");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_StartLegacySyntaxSuccess)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);

    BorderRouter br;
    br.mNetworkId = 0;
    ASSERT_EQ(ctx.mRegistry->SetCurrentNetwork(br), RegistryStatus::kSuccess);

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, Start(_, _, _)).Times(1).WillRepeatedly(Return(Error{}));

    Interpreter::Expression expr;
    Interpreter::Value      value;
    expr  = ctx.mInterpreter.ParseExpression("start 127.0.0.1 20001");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_StartLegacySyntaxErrorFails)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);

    BorderRouter br;
    br.mNetworkId = 0;
    ASSERT_EQ(ctx.mRegistry->SetCurrentNetwork(br), RegistryStatus::kSuccess);

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, Start(_, _, _))
        .Times(1)
        .WillRepeatedly(Return(Error{ErrorCode::kAborted, "Test failure"}));

    Interpreter::Expression expr;
    Interpreter::Value      value;
    expr  = ctx.mInterpreter.ParseExpression("start 127.0.0.1 20001");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_FALSE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_StopNetworkSyntaxSuccess)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);

    uint8_t                camIdx                  = 0;
    CommissionerAppMockPtr commissionerAppMocks[2] = {CommissionerAppMockPtr{new CommissionerAppMock()},
                                                      CommissionerAppMockPtr{new CommissionerAppMock()}};
    EXPECT_CALL(ctx.mCommissionerAppStaticExpecter, Create(_, _))
        .Times(2)
        .WillRepeatedly(
            DoAll(WithArg<0>([&](std::shared_ptr<CommissionerApp> &a) { a = commissionerAppMocks[camIdx++]; }),
                  Return(Error{})));
    EXPECT_CALL(*commissionerAppMocks[0], Start(_, _, _)).Times(1).WillRepeatedly(Return(Error{}));
    EXPECT_CALL(*commissionerAppMocks[1], Start(_, _, _)).Times(1).WillRepeatedly(Return(Error{}));
    EXPECT_CALL(*commissionerAppMocks[0], IsActive()).WillOnce(Return(false)).WillOnce(Return(true));
    EXPECT_CALL(*commissionerAppMocks[1], IsActive()).WillOnce(Return(false)).WillOnce(Return(true));

    Interpreter::Expression expr;
    Interpreter::Value      value;
    expr  = ctx.mInterpreter.ParseExpression("start --nwk net1 net2");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    ctx.mInterpreter.mContext.Cleanup();
    ctx.mInterpreter.mJobManager->CleanupJobs();

    expr  = ctx.mInterpreter.ParseExpression("stop --nwk net1 net2");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_StopLegacySyntaxSuccess)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);

    BorderRouter br;
    br.mNetworkId = 0;
    ASSERT_EQ(ctx.mRegistry->SetCurrentNetwork(br), RegistryStatus::kSuccess);

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, Start(_, _, _)).Times(1).WillOnce(Return(Error{}));

    Interpreter::Expression expr;
    Interpreter::Value      value;
    expr  = ctx.mInterpreter.ParseExpression("start 127.0.0.1 20001");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());

    expr  = ctx.mInterpreter.ParseExpression("stop");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_Active)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);
    ASSERT_EQ(
        ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        RegistryStatus::kSuccess);

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, IsActive()).Times(2).WillOnce(Return(false)).WillOnce(Return(true));

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("active");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    // TODO add result verification

    expr  = ctx.mInterpreter.ParseExpression("active");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());

    CommissionerAppMockPtr pcaMock{new CommissionerAppMock()};
    EXPECT_CALL(ctx.mCommissionerAppStaticExpecter, Create(_, _))
        .WillOnce(DoAll(WithArg<0>([&](std::shared_ptr<CommissionerApp> &a) { a = pcaMock; }), Return(Error{})));
    EXPECT_CALL(*pcaMock, Start).WillOnce(Return(Error{}));
    EXPECT_CALL(*pcaMock, IsActive).Times(3).WillOnce(Return(false)).WillOnce(Return(true)).WillOnce(Return(false));
    expr  = ctx.mInterpreter.ParseExpression("start --nwk net1");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());

    expr  = ctx.mInterpreter.ParseExpression("active --nwk net1");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());

    expr  = ctx.mInterpreter.ParseExpression("active --nwk net1");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_Token)
{
    TestContext ctx;
    InitContext(ctx);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, RequestToken(_, _)).WillOnce(Return(Error{}));

    expr  = ctx.mInterpreter.ParseExpression("token request 127.0.0.1 2001");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());

    const ByteArray token = {'1', '2', '3', 'a', 'e', 'f'};
    EXPECT_CALL(*ctx.mDefaultCommissionerObject, GetToken()).WillOnce(ReturnRef(token));
    expr  = ctx.mInterpreter.ParseExpression("token print");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());

    EXPECT_EQ(WriteFile("123aef", "./token").mCode, ErrorCode::kNone);
    EXPECT_CALL(*ctx.mDefaultCommissionerObject, SetToken(_)).WillOnce(Return(Error{}));
    expr  = ctx.mInterpreter.ParseExpression("token set ./token");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_NetworkSave)
{
    TestContext ctx;
    InitContext(ctx);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, SaveNetworkData(_)).WillOnce(Return(Error{}));
    expr  = ctx.mInterpreter.ParseExpression("network save ./network.txt");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_NetworkSelectNoneOnEmpty)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);

    Network nwk;
    EXPECT_EQ(ctx.mRegistry->GetCurrentNetwork(nwk), RegistryStatus::kSuccess);
    EXPECT_EQ(nwk.mId.mId, EMPTY_ID);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("network select none");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    EXPECT_EQ(ctx.mRegistry->GetCurrentNetwork(nwk), RegistryStatus::kSuccess);
    EXPECT_EQ(nwk.mId.mId, EMPTY_ID);
}

TEST_F(InterpreterTestSuite, PC_NetworkSelectNoneOnSelected)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    BorderRouter br;
    br.mNetworkId = 0;
    ASSERT_EQ(ctx.mRegistry->SetCurrentNetwork(br), RegistryStatus::kSuccess);

    Network nwk;
    EXPECT_EQ(ctx.mRegistry->GetCurrentNetwork(nwk), RegistryStatus::kSuccess);
    EXPECT_EQ(nwk.mId.mId, 0);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("network select none");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    EXPECT_EQ(ctx.mRegistry->GetCurrentNetwork(nwk), RegistryStatus::kSuccess);
    EXPECT_EQ(nwk.mId.mId, EMPTY_ID);
}

TEST_F(InterpreterTestSuite, PC_NetworkSelectOnEmpty)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0xFF}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0xFF}),
              RegistryStatus::kSuccess);

    Network nwk;
    EXPECT_EQ(ctx.mRegistry->GetCurrentNetwork(nwk), RegistryStatus::kSuccess);
    EXPECT_EQ(nwk.mId.mId, EMPTY_ID);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    // TODO TBD XPAN format on enter
    expr  = ctx.mInterpreter.ParseExpression("Network select 1");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    EXPECT_EQ(ctx.mRegistry->GetCurrentNetwork(nwk), RegistryStatus::kSuccess);
    EXPECT_EQ(nwk.mId.mId, 0);
}

TEST_F(InterpreterTestSuite, PC_NetworkSelectAnother)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0xFF}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0xFF}),
              RegistryStatus::kSuccess);
    BorderRouter br;
    br.mNetworkId = 0;
    ASSERT_EQ(ctx.mRegistry->SetCurrentNetwork(br), RegistryStatus::kSuccess);

    Network nwk;
    EXPECT_EQ(ctx.mRegistry->GetCurrentNetwork(nwk), RegistryStatus::kSuccess);
    EXPECT_EQ(nwk.mId.mId, 0);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("Network select 1");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    EXPECT_EQ(ctx.mRegistry->GetCurrentNetwork(nwk), RegistryStatus::kSuccess);
    EXPECT_EQ(nwk.mId.mId, 1);
}

TEST_F(InterpreterTestSuite, PC_NetworkSelectNonexisting)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    BorderRouter br;
    br.mNetworkId = 0;
    ASSERT_EQ(ctx.mRegistry->SetCurrentNetwork(br), RegistryStatus::kSuccess);

    Network nwk;
    EXPECT_EQ(ctx.mRegistry->GetCurrentNetwork(nwk), RegistryStatus::kSuccess);
    EXPECT_EQ(nwk.mId.mId, 0);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("Network select 3");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_FALSE(value.HasNoError());
    EXPECT_EQ(ctx.mRegistry->GetCurrentNetwork(nwk), RegistryStatus::kSuccess);
    EXPECT_EQ(nwk.mId.mId, 0);
}

TEST_F(InterpreterTestSuite, PC_NetworkSelectByName)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x3F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x3F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    BorderRouter br;
    br.mNetworkId = 1;
    ASSERT_EQ(ctx.mRegistry->SetCurrentNetwork(br), RegistryStatus::kSuccess);

    Network nwk;
    EXPECT_EQ(ctx.mRegistry->GetCurrentNetwork(nwk), RegistryStatus::kSuccess);
    EXPECT_EQ(1, nwk.mId.mId);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("Network select net1");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    EXPECT_EQ(ctx.mRegistry->GetCurrentNetwork(nwk), RegistryStatus::kSuccess);
    EXPECT_EQ(0, nwk.mId.mId);
}

TEST_F(InterpreterTestSuite, PC_NetworkIdentifyWithDomain)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x3F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "", 0, 0, "", 0,
                                             0x3F}),
              RegistryStatus::kSuccess);
    BorderRouter br;
    br.mNetworkId = 0;
    ASSERT_EQ(ctx.mRegistry->SetCurrentNetwork(br), RegistryStatus::kSuccess);

    Network nwk;
    EXPECT_EQ(ctx.mRegistry->GetCurrentNetwork(nwk), RegistryStatus::kSuccess);
    EXPECT_EQ(nwk.mId.mId, 0);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("Network identify");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());

    Json json;
    try
    {
        json = Json::parse(value.ToString());
    } catch (Json::parse_error &e)
    {
        EXPECT_TRUE(false) << "Failed to parse value: " << e.what();
    }
    EXPECT_TRUE(json.contains("0000000000000001"));
    EXPECT_STREQ("domain1/net1", json.at("0000000000000001").get<std::string>().c_str());
}

TEST_F(InterpreterTestSuite, PC_NetworkIdentifyWithoutDomain)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x3F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "", 0, 0, "", 0,
                                             0x3F}),
              RegistryStatus::kSuccess);
    BorderRouter br;
    br.mNetworkId = 1;
    ASSERT_EQ(ctx.mRegistry->SetCurrentNetwork(br), RegistryStatus::kSuccess);

    Network nwk;
    EXPECT_EQ(ctx.mRegistry->GetCurrentNetwork(nwk), RegistryStatus::kSuccess);
    EXPECT_EQ(nwk.mId.mId, 1);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("Network identify");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());

    Json json;
    try
    {
        json = Json::parse(value.ToString());
    } catch (Json::parse_error &e)
    {
        EXPECT_TRUE(false) << "Failed to parse value: " << e.what();
    }
    EXPECT_TRUE(json.contains("0000000000000002"));
    EXPECT_STREQ("net2", json.at("0000000000000002").get<std::string>().c_str());
}

TEST_F(InterpreterTestSuite, PC_NetworkIdentifyUnset)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x3F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "", 0, 0, "", 0,
                                             0x3F}),
              RegistryStatus::kSuccess);

    Network nwk;
    EXPECT_EQ(ctx.mRegistry->GetCurrentNetwork(nwk), RegistryStatus::kSuccess);
    EXPECT_EQ(nwk.mId.mId, EMPTY_ID);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("Network identify");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    EXPECT_STREQ("none", value.ToString().c_str());
}

TEST_F(InterpreterTestSuite, PC_NetworkList)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0xFFFFF}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0xFFFFF}),
              RegistryStatus::kSuccess);
    BorderRouter br;
    br.mNetworkId = 0;
    ASSERT_EQ(ctx.mRegistry->SetCurrentNetwork(br), RegistryStatus::kSuccess);

    Network nwk;
    EXPECT_EQ(ctx.mRegistry->GetCurrentNetwork(nwk), RegistryStatus::kSuccess);
    EXPECT_EQ(nwk.mId.mId, 0);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("Network list --dom domain1");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());

    expr  = ctx.mInterpreter.ParseExpression("Network list --nwk other");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_Sessionid)
{
    TestContext ctx;
    InitContext(ctx);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, GetSessionId(_))
        .WillOnce(DoAll(WithArg<0>([=](uint16_t &a) { a = 1; }), Return(Error{})));
    expr  = ctx.mInterpreter.ParseExpression("sessionid");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_BorderagentDiscover)
{
    TestContext ctx;
    InitContext(ctx);

    BorderAgentFunctionsMock bafm;
    SetBorderAgentFunctionsMock(&bafm);

    EXPECT_CALL(bafm, DiscoverBorderAgent(_, _)).WillOnce(Return(Error{}));

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("borderagent discover");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());

    ClearBorderAgentFunctionsMock();
}

TEST_F(InterpreterTestSuite, PC_BorderagentGetLocator)
{
    TestContext ctx;
    InitContext(ctx);

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, GetBorderAgentLocator(_)).WillOnce(Return(Error{}));

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("borderagent get locator");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_JoinerEnable)
{
    TestContext ctx;
    InitContext(ctx);

    EXPECT_CALL(*ctx.mDefaultCommissionerObject,
                EnableJoiner(JoinerType::kMeshCoP, 1, StrEq("psk"), StrEq("url://provision.ing")))
        .WillOnce(Return(Error{}));

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("joiner enable meshcop 1 psk url://provision.ing");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_JoinerEnableall)
{
    TestContext ctx;
    InitContext(ctx);

    EXPECT_CALL(*ctx.mDefaultCommissionerObject,
                EnableAllJoiners(JoinerType::kMeshCoP, StrEq("psk"), StrEq("url://provision.ing")))
        .WillOnce(Return(Error{}));

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, EnableAllJoiners(JoinerType::kAE, StrEq(""), StrEq("")))
        .WillOnce(Return(Error{}));

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("joiner enableall meshcop psk url://provision.ing");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());

    expr  = ctx.mInterpreter.ParseExpression("joiner enableall ae");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_JoinerDisable)
{
    TestContext ctx;
    InitContext(ctx);

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, DisableJoiner(JoinerType::kNMKP, 1)).WillOnce(Return(Error{}));

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("joiner disable nmkp 1");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_JoinerDisableall)
{
    TestContext ctx;
    InitContext(ctx);

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, DisableAllJoiners(JoinerType::kMeshCoP)).WillOnce(Return(Error{}));

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("joiner disableall meshcop");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_JoinerGetport)
{
    TestContext ctx;
    InitContext(ctx);

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, GetJoinerUdpPort(_, JoinerType::kNMKP)).WillOnce(Return(Error{}));

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("joiner getport nmkp");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_JoinerSetport)
{
    TestContext ctx;
    InitContext(ctx);

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, SetJoinerUdpPort(JoinerType::kMeshCoP, 2001))
        .WillOnce(Return(Error{}));

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("joiner setport meshcop 2001");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_CommdatasetGet)
{
    TestContext ctx;
    InitContext(ctx);

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, GetCommissionerDataset(_, _)).WillOnce(Return(Error{}));

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("commdataset get");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_CommdatasetSet)
{
    TestContext ctx;
    InitContext(ctx);

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, SetCommissionerDataset(_)).WillOnce(Return(Error{}));

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("commdataset set '{}'");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());

    expr  = ctx.mInterpreter.ParseExpression("commdataset set 'invalid-json'");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_FALSE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_OpdatasetGetActive)
{
    TestContext ctx;
    InitContext(ctx);

    NetworkId nwk_id;
    EXPECT_EQ(ctx.mRegistry->mStorage->Add(Network{EMPTY_ID, EMPTY_ID, "", XpanId{1}, 0, "", "", 0}, nwk_id),
              PersistentStorage::Status::PS_SUCCESS);
    Network nwk;
    EXPECT_EQ(ctx.mRegistry->mStorage->Get(nwk_id, nwk), PersistentStorage::Status::PS_SUCCESS);
    EXPECT_STREQ("", nwk.mPan.c_str());

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, GetActiveDataset(_, _))
        .Times(2)
        .WillRepeatedly(DoAll(WithArg<0>([](ActiveOperationalDataset &a) {
                                  a.mExtendedPanId = XpanId{1}, a.mPanId = 1;
                                  a.mPresentFlags =
                                      ActiveOperationalDataset::kPanIdBit | ActiveOperationalDataset::kExtendedPanIdBit;
                              }),
                              Return(Error{})));

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("opdataset get active");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());

    EXPECT_EQ(ctx.mRegistry->mStorage->Get(nwk_id, nwk), PersistentStorage::Status::PS_SUCCESS);
    EXPECT_STREQ("0x0001", nwk.mPan.c_str());

    EXPECT_EQ(system("rm -f ./aods.json"), 0);
    EXPECT_NE(PathExists("./aods.json").GetCode(), ErrorCode::kNone);

    expr  = ctx.mInterpreter.ParseExpression("opdataset get active --export ./aods.json");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    ctx.mInterpreter.PrintOrExport(value);
    EXPECT_EQ(PathExists("./aods.json").GetCode(), ErrorCode::kNone);

    std::string jsonStr;
    EXPECT_EQ(ReadFile(jsonStr, "./aods.json").GetCode(), ErrorCode::kNone);
    nlohmann::json json = nlohmann::json::parse(jsonStr);
    EXPECT_TRUE(json.contains("PanId"));
    EXPECT_STREQ("0x0001", json.at("PanId").get<std::string>().c_str());
}

TEST_F(InterpreterTestSuite, PC_OpdatasetSetActive)
{
    TestContext ctx;
    InitContext(ctx);

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, SetActiveDataset(_)).WillOnce(Return(Error{}));

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("opdataset set active '{}'");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());

    expr  = ctx.mInterpreter.ParseExpression("opdataset set active 'invalid-json'");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_FALSE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_OpdatasetGetPending)
{
    TestContext ctx;
    InitContext(ctx);

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, GetPendingDataset(_, _)).WillOnce(Return(Error{}));

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("opdataset get pending");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_OpdatasetSetPending)
{
    TestContext ctx;
    InitContext(ctx);

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, SetPendingDataset(_)).WillOnce(Return(Error{}));

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("opdataset set pending '{}'");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());

    expr  = ctx.mInterpreter.ParseExpression("opdataset set pending 'invalid-json'");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_FALSE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_BbrdatasetGet)
{
    TestContext ctx;
    InitContext(ctx);

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, GetBbrDataset(_, _)).WillOnce(Return(Error{}));

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("bbrdataset get");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_BbrdatasetSet)
{
    TestContext ctx;
    InitContext(ctx);

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, SetBbrDataset(_)).WillOnce(Return(Error{}));

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("bbrdataset set '{}'");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());

    expr  = ctx.mInterpreter.ParseExpression("bbrdataset set 'invalid-json'");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_FALSE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_Reenroll)
{
    TestContext ctx;
    InitContext(ctx);

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, Reenroll(_)).WillOnce(Return(Error{}));

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("reenroll 1234::5678");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_Domainreset)
{
    TestContext ctx;
    InitContext(ctx);

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, DomainReset(_)).WillOnce(Return(Error{}));

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("domainreset 1234::5678");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_Migrate)
{
    TestContext ctx;
    InitContext(ctx);

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, Migrate(_, _)).WillOnce(Return(Error{}));

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("migrate 1234::5678 net1");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_Mlr)
{
    TestContext ctx;
    InitContext(ctx);

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, RegisterMulticastListener(_, _)).WillOnce(Return(Error{}));

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("mlr 1234::5678 100");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_Announce)
{
    TestContext ctx;
    InitContext(ctx);

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, AnnounceBegin(1, 2, _, _)).WillOnce(Return(Error{}));

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("announce 1 2 3 1234::5678");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_PanidQuery)
{
    TestContext ctx;
    InitContext(ctx);

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, PanIdQuery(_, _, _)).WillOnce(Return(Error{}));

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("panid query 1 2 1234::5678");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_PanidConflict)
{
    TestContext ctx;
    InitContext(ctx);

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, HasPanIdConflict(_)).WillOnce(Return(true));

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("panid conflict 2");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    // TODO TBD should it be 1 or true?
    EXPECT_STREQ(value.ToString().c_str(), "1");
    // EXPECT_STREQ(value.ToString().c_str(), "true");
}

TEST_F(InterpreterTestSuite, PC_EnergyScan)
{
    TestContext ctx;
    InitContext(ctx);

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, EnergyScan(_, _, _, _, _)).WillOnce(Return(Error{}));

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("energy scan 1 2 3 4 1234::5678");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_EnergyReport)
{
    TestContext ctx;
    InitContext(ctx);

    EnergyReport rep;
    EXPECT_CALL(*ctx.mDefaultCommissionerObject, GetEnergyReport(_)).WillOnce(Return(&rep));

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("energy report 1234::5678");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_Help)
{
    TestContext ctx;
    InitContext(ctx);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("help");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_DomainList)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("domain list");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_BrScanExport)
{
    TestContext ctx;
    InitContext(ctx);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    std::string jsonFileName = "./br-list.json";

    EXPECT_EQ(system(fmt::format("rm -rf {}", jsonFileName).c_str()), 0);
    EXPECT_NE(PathExists(jsonFileName).GetCode(), ErrorCode::kNone);

    expr  = ctx.mInterpreter.ParseExpression(std::string("br scan --timeout 1 --export ") + jsonFileName);
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    ctx.mInterpreter.PrintOrExport(value);
    EXPECT_EQ(PathExists(jsonFileName).GetCode(), ErrorCode::kNone);

    std::string jsonStr;
    EXPECT_EQ(ReadFile(jsonStr, jsonFileName).GetCode(), ErrorCode::kNone);
    try
    {
        nlohmann::json json = nlohmann::json::parse(jsonStr);
    } catch (...)
    {
        EXPECT_TRUE(false) << "JSON parse failed";
    }
}

TEST_F(InterpreterTestSuite, PC_BrScanExportDirAbsent)
{
    TestContext ctx;
    InitContext(ctx);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    std::string jsonFileName = "./tmpdir/br-list.json";
    ASSERT_EQ(system("rm -rf ./tmpdir"), 0);
    expr  = ctx.mInterpreter.ParseExpression(std::string("br scan --timeout 1 --export ") + jsonFileName);
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    ctx.mInterpreter.PrintOrExport(value);
    value = PathExists(jsonFileName);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_BrAddNoMandatoryFail)
{
    TestContext ctx;
    InitContext(ctx);

    std::string brJsonNoAddr    = "[\n\
    {\n\
        \"Port\": 2000,\n\
        \"ThreadVersion\": \"th1.2\",\n\
        \"State\": 0,\n\
        \"NetworkName\": \"net1\",\n\
        \"ExtendedPanId\": 1234,\n\
        \"DomainName\": \"dom1\"\n\
    }\n\
]";
    std::string brJsonNoPort    = "[\n\
    {\n\
        \"Addr\": \"1234::5678\",\n\
        \"ThreadVersion\": \"th1.2\",\n\
        \"State\": 0,\n\
        \"NetworkName\": \"net1\",\n\
        \"ExtendedPanId\": 1234,\n\
        \"DomainName\": \"dom1\"\n\
    }\n\
]";
    std::string brJsonNoVersion = "[\n\
    {\n\
        \"Addr\": \"1234::5678\",\n\
        \"Port\": 2000,\n\
        \"State\": 0,\n\
        \"NetworkName\": \"net1\",\n\
        \"ExtendedPanId\": 1234,\n\
        \"DomainName\": \"dom1\"\n\
    }\n\
]";
    std::string brJsonNoState   = "[\n\
    {\n\
        \"Addr\": \"1234::5678\",\n\
        \"Port\": 2000,\n\
        \"ThreadVersion\": \"th1.2\",\n\
        \"NetworkName\": \"net1\",\n\
        \"ExtendedPanId\": 1234,\n\
        \"DomainName\": \"dom1\"\n\
    }\n\
]";

    Interpreter::Expression expr;
    Interpreter::Value      value;

    EXPECT_EQ(WriteFile(brJsonNoAddr, "./json.json"), Error{});
    expr  = ctx.mInterpreter.ParseExpression("br add ./json.json");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_FALSE(value.HasNoError());

    EXPECT_EQ(WriteFile(brJsonNoPort, "./json.json"), Error{});
    expr  = ctx.mInterpreter.ParseExpression("br add ./json.json");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_FALSE(value.HasNoError());

    EXPECT_EQ(WriteFile(brJsonNoVersion, "./json.json"), Error{});
    expr  = ctx.mInterpreter.ParseExpression("br add ./json.json");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_FALSE(value.HasNoError());

    EXPECT_EQ(WriteFile(brJsonNoState, "./json.json"), Error{});
    expr  = ctx.mInterpreter.ParseExpression("br add ./json.json");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_FALSE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_BrAddWrongLocalNwkDataFail)
{
    TestContext ctx;
    InitContext(ctx);

    std::string brJsonNwkName            = "[\n\
    {\n\
        \"Addr\": \"1234::5678\",\n\
        \"Port\": 2000,\n\
        \"ThreadVersion\": \"th1.2\",\n\
        \"State\": 0,\n\
        \"NetworkName\": \"net1\",\n\
    }\n\
]";
    std::string brJsonDomainName         = "[\n\
    {\n\
        \"Addr\": \"1234::5678\",\n\
        \"Port\": 2000,\n\
        \"ThreadVersion\": \"th1.2\",\n\
        \"State\": 0,\n\
        \"DomainName\": \"dom1\"\n\
    }\n\
n]";
    std::string brJsonNwkNameZeroXPan    = "[\n\
    {\n\
        \"Addr\": \"1234::5678\",\n\
        \"Port\": 2000,\n\
        \"ThreadVersion\": \"th1.2\",\n\
        \"State\": 0,\n\
        \"NetworkName\": \"net1\",\n\
        \"ExtendedPanId\": 0,\n\
    }\n\
]";
    std::string brJsonDomainNameZeroXPan = "[\n\
    {\n\
        \"Addr\": \"1234::5678\",\n\
        \"Port\": 2000,\n\
        \"ThreadVersion\": \"th1.2\",\n\
        \"State\": 0,\n\
        \"NetworkName\": \"net1\",\n\
        \"ExtendedPanId\": 0,\n\
        \"DomainName\": \"dom1\"\n\
    }\n\
]";

    Interpreter::Expression expr;
    Interpreter::Value      value;

    EXPECT_EQ(WriteFile(brJsonNwkName, "./json.json"), Error{});
    expr  = ctx.mInterpreter.ParseExpression("br add ./json.json");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_FALSE(value.HasNoError());

    EXPECT_EQ(WriteFile(brJsonNwkNameZeroXPan, "./json.json"), Error{});
    expr  = ctx.mInterpreter.ParseExpression("br add ./json.json");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_FALSE(value.HasNoError());

    EXPECT_EQ(WriteFile(brJsonDomainName, "./json.json"), Error{});
    expr  = ctx.mInterpreter.ParseExpression("br add ./json.json");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_FALSE(value.HasNoError());

    EXPECT_EQ(WriteFile(brJsonDomainNameZeroXPan, "./json.json"), Error{});
    expr  = ctx.mInterpreter.ParseExpression("br add ./json.json");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_FALSE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_BrAddInterObjectInconsistencyFail)
{
    TestContext ctx;
    InitContext(ctx);

    std::string brJsonSameAddr                  = "[\n\
    {\n\
        \"Addr\": \"1234::5678\",\n\
        \"Port\": 2000,\n\
        \"ThreadVersion\": \"th1.2\",\n\
        \"State\": 0,\n\
    },\n\
    {\n\
        \"Addr\": \"1234::5678\",\n\
        \"Port\": 2000,\n\
        \"ThreadVersion\": \"th1.2\",\n\
        \"State\": 0,\n\
    }\n\
]";
    std::string brJsonSameXPanDifferentNwkNames = "[\n\
    {\n\
        \"Addr\": \"1234::5678\",\n\
        \"Port\": 2000,\n\
        \"ThreadVersion\": \"th1.2\",\n\
        \"State\": 0,\n\
        \"NetworkName\": \"net1\",\n\
        \"ExtendedPanId\": 1234,\n\
     },\n\
    {\n\
        \"Addr\": \"1234::5679\",\n\
        \"Port\": 2000,\n\
        \"ThreadVersion\": \"th1.2\",\n\
        \"State\": 0,\n\
        \"NetworkName\": \"net2\",\n\
        \"ExtendedPanId\": 1234,\n\
    }\n\
]";
    std::string brJsonSameXPanDifferentDomains  = "[\n\
    {\n\
        \"Addr\": \"1234::5678\",\n\
        \"Port\": 2000,\n\
        \"ThreadVersion\": \"th1.2\",\n\
        \"State\": 0,\n\
        \"ExtendedPanId\": 1234,\n\
        \"NetworkName\": \"net2\",\n\
        \"DomainName\": \"dom1\"\n\
    },\n\
    {\n\
        \"Addr\": \"1234::5679\",\n\
        \"Port\": 2000,\n\
        \"ThreadVersion\": \"th1.2\",\n\
        \"State\": 0,\n\
        \"NetworkName\": \"net2\",\n\
        \"ExtendedPanId\": 1234,\n\
        \"DomainName\": \"dom2\"\n\
    }\n\
]";

    Interpreter::Expression expr;
    Interpreter::Value      value;

    EXPECT_EQ(WriteFile(brJsonSameAddr, "./json.json"), Error{});
    expr  = ctx.mInterpreter.ParseExpression("br add ./json.json");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_FALSE(value.HasNoError());

    EXPECT_EQ(WriteFile(brJsonSameXPanDifferentNwkNames, "./json.json"), Error{});
    expr  = ctx.mInterpreter.ParseExpression("br add ./json.json");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_FALSE(value.HasNoError());

    EXPECT_EQ(WriteFile(brJsonSameXPanDifferentDomains, "./json.json"), Error{});
    expr  = ctx.mInterpreter.ParseExpression("br add ./json.json");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_FALSE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_BrAdd)
{
    TestContext ctx;
    InitContext(ctx);

    std::string brJson = "[\n\
    {\n\
        \"Addr\": \"1234::5678\",\n\
        \"Port\": 2000,\n\
        \"ThreadVersion\": \"th1.2\",\n\
        \"State\": 0,\n\
        \"NetworkName\": \"net1\",\n\
        \"ExtendedPanId\": 1234,\n\
        \"DomainName\": \"dom1\"\n\
    },\n\
    {\n\
        \"Addr\": \"1234::5679\",\n\
        \"Port\": 2001,\n\
        \"ThreadVersion\": \"th1.2\",\n\
        \"State\": 0,\n\
        \"NetworkName\": \"net2\",\n\
        \"ExtendedPanId\": 1235,\n\
        \"DomainName\": \"dom1\"\n\
    },\n\
    {\n\
        \"Addr\": \"1234::5670\",\n\
        \"Port\": 2000,\n\
        \"ThreadVersion\": \"th1.2\",\n\
        \"State\": 0,\n\
        \"NetworkName\": \"net2\",\n\
        \"ExtendedPanId\": 1235,\n\
        \"DomainName\": \"dom1\"\n\
    },\n\
    {\n\
        \"Addr\": \"1234::5671\",\n\
        \"Port\": 2001,\n\
        \"ThreadVersion\": \"th1.2\",\n\
        \"State\": 0,\n\
        \"NetworkName\": \"net3\",\n\
        \"ExtendedPanId\": 1236,\n\
        \"DomainName\": \"dom3\"\n\
    }\n\
]";

    Interpreter::Expression expr;
    Interpreter::Value      value;

    EXPECT_EQ(WriteFile(brJson, "./json.json"), Error{});
    expr  = ctx.mInterpreter.ParseExpression("br add ./json.json");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());

    BorderRouterArray brs;
    EXPECT_EQ(ctx.mInterpreter.mRegistry->GetAllBorderRouters(brs), RegistryStatus::kSuccess);
    EXPECT_EQ(brs.size(), 4);

    NetworkArray nwks;
    EXPECT_EQ(ctx.mInterpreter.mRegistry->GetAllNetworks(nwks), RegistryStatus::kSuccess);
    EXPECT_EQ(nwks.size(), 3);

    DomainArray doms;
    EXPECT_EQ(ctx.mInterpreter.mRegistry->GetAllDomains(doms), RegistryStatus::kSuccess);
    EXPECT_EQ(doms.size(), 2);
}

TEST_F(InterpreterTestSuite, PC_BrList)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("br list");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());

    expr  = ctx.mInterpreter.ParseExpression("br list --nwk net1");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());

    expr  = ctx.mInterpreter.ParseExpression("br list --dom domain1");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_BrDeleteExplicitPass)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray(), "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("br delete 1");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    BorderRouterArray bra;
    EXPECT_EQ(ctx.mRegistry->GetAllBorderRouters(bra), RegistryStatus::kSuccess);
    EXPECT_EQ(bra.size(), 1);
    EXPECT_EQ(bra[0].mId.mId, 0);
}

TEST_F(InterpreterTestSuite, PC_BrDeleteTooManyFail)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("br delete 1 1");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_FALSE(value.HasNoError());
    ctx.mInterpreter.mContext.Cleanup();
    ctx.mInterpreter.mJobManager->CleanupJobs();

    expr  = ctx.mInterpreter.ParseExpression("br delete 1 --nwk net1");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_FALSE(value.HasNoError());
    ctx.mInterpreter.mContext.Cleanup();
    ctx.mInterpreter.mJobManager->CleanupJobs();

    expr  = ctx.mInterpreter.ParseExpression("br delete 1 --dom domain1");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_FALSE(value.HasNoError());
    ctx.mInterpreter.mContext.Cleanup();
    ctx.mInterpreter.mJobManager->CleanupJobs();
}

TEST_F(InterpreterTestSuite, PC_BrDeleteExplicitLastPass)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x3F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0,
                                             "", 0, 0x3F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("br delete 1");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    BorderRouterArray bra;
    EXPECT_EQ(ctx.mRegistry->GetAllBorderRouters(bra), RegistryStatus::kSuccess);
    EXPECT_EQ(bra.size(), 1);
    EXPECT_EQ(bra[0].mId.mId, 0);

    NetworkArray nwks;
    EXPECT_EQ(ctx.mRegistry->GetAllNetworks(nwks), RegistryStatus::kSuccess);
    EXPECT_EQ(nwks.size(), 1);

    DomainArray doms;
    EXPECT_EQ(ctx.mRegistry->GetAllDomains(doms), RegistryStatus::kSuccess);
    EXPECT_EQ(doms.size(), 1);
}

TEST_F(InterpreterTestSuite, PC_BrDeleteExplicitSelectedFails)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x3F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0,
                                             "", 0, 0x3F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);

    EXPECT_EQ(ctx.mRegistry->SetCurrentNetwork(XpanId{2}), RegistryStatus::kSuccess);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("br delete 1");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_FALSE(value.HasNoError());

    BorderRouterArray bra;
    EXPECT_EQ(ctx.mRegistry->GetAllBorderRouters(bra), RegistryStatus::kSuccess);
    EXPECT_EQ(bra.size(), 2);
}

TEST_F(InterpreterTestSuite, PC_BrDeleteNetworkSuccess)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x3F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0,
                                             "", 0, 0x3F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("br delete --nwk net1");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());

    BorderRouterArray bra;
    EXPECT_EQ(ctx.mRegistry->GetAllBorderRouters(bra), RegistryStatus::kSuccess);
    EXPECT_EQ(bra.size(), 1);

    NetworkArray nwks;
    EXPECT_EQ(ctx.mRegistry->GetAllNetworks(nwks), RegistryStatus::kSuccess);
    EXPECT_EQ(nwks.size(), 1);

    DomainArray doms;
    EXPECT_EQ(ctx.mRegistry->GetAllDomains(doms), RegistryStatus::kSuccess);
    EXPECT_EQ(doms.size(), 1);
}

TEST_F(InterpreterTestSuite, PC_BrDeleteDomainSuccess)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x3F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0,
                                             "", 0, 0x3F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("br delete --dom domain2");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());

    BorderRouterArray bra;
    EXPECT_EQ(ctx.mRegistry->GetAllBorderRouters(bra), RegistryStatus::kSuccess);
    EXPECT_EQ(bra.size(), 1);
    EXPECT_EQ(bra[0].mId.mId, 0);

    NetworkArray nwks;
    EXPECT_EQ(ctx.mRegistry->GetAllNetworks(nwks), RegistryStatus::kSuccess);
    EXPECT_EQ(nwks.size(), 1);

    DomainArray doms;
    EXPECT_EQ(ctx.mRegistry->GetAllDomains(doms), RegistryStatus::kSuccess);
    EXPECT_EQ(doms.size(), 1);
}

TEST_F(InterpreterTestSuite, PC_BrDeleteNetwork)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.3", 20003, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("br delete --nwk net1");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    BorderRouterArray bra;
    EXPECT_EQ(ctx.mRegistry->GetAllBorderRouters(bra), RegistryStatus::kSuccess);
    EXPECT_EQ(bra.size(), 1);
}

TEST_F(InterpreterTestSuite, PC_BrDeleteDomain)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.3", 20003, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("br delete --dom domain1");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    BorderRouterArray bra;
    EXPECT_EQ(ctx.mRegistry->GetAllBorderRouters(bra), RegistryStatus::kSuccess);
    EXPECT_EQ(bra.size(), 1);
}

TEST_F(InterpreterTestSuite, MNI_ImportAllNetworksFail)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);

    EXPECT_CALL(ctx.mCommissionerAppStaticExpecter, Create(_, _)).Times(0);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("opdataset set active --nwk all --import absent.json");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_FALSE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, MNI_ImportOtherNetworksFail)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.3", 20003, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);

    BorderRouter br;
    br.mNetworkId = 0;
    ASSERT_EQ(ctx.mRegistry->SetCurrentNetwork(br), RegistryStatus::kSuccess);

    EXPECT_CALL(ctx.mCommissionerAppStaticExpecter, Create(_, _)).Times(0);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("opdataset set active --nwk other --import absent.json");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_FALSE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, MNI_ImportDomainFail)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);

    EXPECT_CALL(ctx.mCommissionerAppStaticExpecter, Create(_, _)).Times(0);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("opdataset set active --dom domain1 --import absent.json");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_FALSE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, MNI_MultiEntryImportExplicitNetworkPass)
{
    TestContext ctx;
    InitContext(ctx);

    const std::string multiEntryPendingDataset = "\n\
{\n\
  \"1122334455667788\":\n\
  {\n\
    \"DelayTimer\": 60000, // in milliseconds\n\
    \"PendingTimestamp\": {\n\
        \"Seconds\": 58, // 48 bits\n\
        \"Ticks\": 48, // 15 bits\n\
        \"U\": 0 // 1 bit\n\
    },\n\
    \"ActiveTimestamp\": {\n\
        \"Seconds\": 58, // 48 bits\n\
        \"Ticks\": 48, // 15 bits\n\
        \"U\": 0 // 1 bit\n\
    },\n\
    \"ExtendedPanId\": \"1122334455667788\",\n\
    \"NetworkName\": \"net1\",\n\
    \"SecurityPolicy\": {\n\
        \"Flags\": \"ff\",\n\
        \"RotationTime\": 673\n\
    }\n\
  },\n\
  \"99AABBCCDDEEFF00\":\n\
  {\n\
    \"DelayTimer\": 60000, // in milliseconds\n\
    \"PendingTimestamp\": {\n\
        \"Seconds\": 58, // 48 bits\n\
        \"Ticks\": 48, // 15 bits\n\
        \"U\": 0 // 1 bit\n\
    },\n\
    \"ActiveTimestamp\": {\n\
        \"Seconds\": 58, // 48 bits\n\
        \"Ticks\": 48, // 15 bits\n\
        \"U\": 0 // 1 bit\n\
    },\n\
    \"ExtendedPanId\": \"99AABBCCDDEEFF00\",\n\
    \"NetworkName\": \"net9\",\n\
    \"SecurityPolicy\": {\n\
        \"Flags\": \"ff\",\n\
        \"RotationTime\": 673\n\
    }\n\
  }\n\
}";
    const std::string mepdsFileName            = "./mepds.json";
    ASSERT_EQ(WriteFile(multiEntryPendingDataset, mepdsFileName).mCode, ErrorCode::kNone);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 0x1122334455667788ul, "", "", Timestamp{0, 0, 0}, 0, "",
                                             ByteArray{}, "domain1", 0, 0, "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 0x99AABBCCDDEEFF00ul, "", "", Timestamp{0, 0, 0}, 0, "",
                                             ByteArray{}, "domain2", 0, 0, "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);

    CommissionerAppMockPtr commissionerAppMock{new CommissionerAppMock()};
    EXPECT_CALL(ctx.mCommissionerAppStaticExpecter, Create(_, _))
        .WillOnce(
            DoAll(WithArg<0>([&](std::shared_ptr<CommissionerApp> &a) { a = commissionerAppMock; }), Return(Error{})));
    EXPECT_CALL(*commissionerAppMock, IsActive()).WillOnce(Return(false)).WillOnce(Return(true));
    EXPECT_CALL(*commissionerAppMock, SetActiveDataset(_)).WillOnce(Return(Error{}));
    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr = ctx.mInterpreter.ParseExpression("start --nwk net1");
    EXPECT_TRUE(ctx.mInterpreter.Eval(expr).HasNoError());
    ctx.mInterpreter.mContext.Cleanup();
    ctx.mInterpreter.mJobManager->CleanupJobs();

    expr  = ctx.mInterpreter.ParseExpression(std::string("opdataset set active --nwk net1 --import ") + mepdsFileName);
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());

    unlink(mepdsFileName.c_str());
}

TEST_F(InterpreterTestSuite, MNI_MultiEntryImportImplicitNetworkPass)
{
    TestContext ctx;
    InitContext(ctx);

    const std::string multiEntryPendingDataset = "\n\
{\n\
  \"1122334455667788\":\n\
  {\n\
    \"DelayTimer\": 60000, // in milliseconds\n\
    \"PendingTimestamp\": {\n\
        \"Seconds\": 58, // 48 bits\n\
        \"Ticks\": 48, // 15 bits\n\
        \"U\": 0 // 1 bit\n\
    },\n\
    \"ActiveTimestamp\": {\n\
        \"Seconds\": 58, // 48 bits\n\
        \"Ticks\": 48, // 15 bits\n\
        \"U\": 0 // 1 bit\n\
    },\n\
    \"ExtendedPanId\": \"1122334455667788\",\n\
    \"NetworkName\": \"net1\",\n\
    \"SecurityPolicy\": {\n\
        \"Flags\": \"ff\",\n\
        \"RotationTime\": 673\n\
    }\n\
  },\n\
  \"99AABBCCDDEEFF00\":\n\
  {\n\
    \"DelayTimer\": 60000, // in milliseconds\n\
    \"PendingTimestamp\": {\n\
        \"Seconds\": 58, // 48 bits\n\
        \"Ticks\": 48, // 15 bits\n\
        \"U\": 0 // 1 bit\n\
    },\n\
    \"ActiveTimestamp\": {\n\
        \"Seconds\": 58, // 48 bits\n\
        \"Ticks\": 48, // 15 bits\n\
        \"U\": 0 // 1 bit\n\
    },\n\
    \"ExtendedPanId\": \"99AABBCCDDEEFF00\",\n\
    \"NetworkName\": \"net9\",\n\
    \"SecurityPolicy\": {\n\
        \"Flags\": \"ff\",\n\
        \"RotationTime\": 673\n\
    }\n\
  }\n\
}";
    const std::string mepdsFileName            = "./mepds.json";
    ASSERT_EQ(WriteFile(multiEntryPendingDataset, mepdsFileName).mCode, ErrorCode::kNone);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 0x1122334455667788ul, "", "", Timestamp{0, 0, 0}, 0, "",
                                             ByteArray{}, "domain1", 0, 0, "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 0x99AABBCCDDEEFF00ul, "", "", Timestamp{0, 0, 0}, 0, "",
                                             ByteArray{}, "domain2", 0, 0, "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    BorderRouter br;
    br.mNetworkId = 0;
    ASSERT_EQ(ctx.mRegistry->SetCurrentNetwork(br), RegistryStatus::kSuccess);

    CommissionerAppMockPtr commissionerAppMock{new CommissionerAppMock()};
    EXPECT_CALL(ctx.mCommissionerAppStaticExpecter, Create(_, _))
        .WillOnce(
            DoAll(WithArg<0>([&](std::shared_ptr<CommissionerApp> &a) { a = commissionerAppMock; }), Return(Error{})));
    EXPECT_CALL(*commissionerAppMock, SetActiveDataset(_)).WillOnce(Return(Error{}));
    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr = ctx.mInterpreter.ParseExpression("start");
    EXPECT_TRUE(ctx.mInterpreter.Eval(expr).HasNoError());
    ctx.mInterpreter.mContext.Cleanup();
    ctx.mInterpreter.mJobManager->CleanupJobs();

    expr  = ctx.mInterpreter.ParseExpression(std::string("opdataset set active --import ") + mepdsFileName);
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());

    unlink(mepdsFileName.c_str());
}

TEST_F(InterpreterTestSuite, MNI_SingleEntryImportExplicitNetworkPass)
{
    TestContext ctx;
    InitContext(ctx);

    const std::string multiEntryPendingDataset = "\n\
{\n\
    \"DelayTimer\": 60000, // in milliseconds\n\
    \"PendingTimestamp\": {\n\
        \"Seconds\": 58, // 48 bits\n\
        \"Ticks\": 48, // 15 bits\n\
        \"U\": 0 // 1 bit\n\
    },\n\
    \"ActiveTimestamp\": {\n\
        \"Seconds\": 58, // 48 bits\n\
        \"Ticks\": 48, // 15 bits\n\
        \"U\": 0 // 1 bit\n\
    },\n\
    \"ExtendedPanId\": \"1122334455667788\",\n\
    \"NetworkName\": \"net1\",\n\
    \"SecurityPolicy\": {\n\
        \"Flags\": \"ff\",\n\
        \"RotationTime\": 673\n\
    }\n\
}";
    const std::string mepdsFileName            = "./mepds.json";
    ASSERT_EQ(WriteFile(multiEntryPendingDataset, mepdsFileName).mCode, ErrorCode::kNone);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 0x1122334455667788ul, "", "", Timestamp{0, 0, 0}, 0, "",
                                             ByteArray{}, "domain1", 0, 0, "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 0x99AABBCCDDEEFF00ul, "", "", Timestamp{0, 0, 0}, 0, "",
                                             ByteArray{}, "domain2", 0, 0, "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);

    CommissionerAppMockPtr commissionerAppMock{new CommissionerAppMock()};
    EXPECT_CALL(ctx.mCommissionerAppStaticExpecter, Create(_, _))
        .WillOnce(
            DoAll(WithArg<0>([&](std::shared_ptr<CommissionerApp> &a) { a = commissionerAppMock; }), Return(Error{})));
    EXPECT_CALL(*commissionerAppMock, IsActive()).WillOnce(Return(false)).WillOnce(Return(true));
    EXPECT_CALL(*commissionerAppMock, SetActiveDataset(_)).WillOnce(Return(Error{}));
    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr = ctx.mInterpreter.ParseExpression("start --nwk net1");
    EXPECT_TRUE(ctx.mInterpreter.Eval(expr).HasNoError());
    ctx.mInterpreter.mContext.Cleanup();
    ctx.mInterpreter.mJobManager->CleanupJobs();

    expr  = ctx.mInterpreter.ParseExpression(std::string("opdataset set active --nwk net1 --import ") + mepdsFileName);
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());

    unlink(mepdsFileName.c_str());
}

TEST_F(InterpreterTestSuite, MNI_SingleEntryImportImplicitNetworkPass)
{
    TestContext ctx;
    InitContext(ctx);

    const std::string multiEntryPendingDataset = "\n\
{\n\
    \"DelayTimer\": 60000, // in milliseconds\n\
    \"PendingTimestamp\": {\n\
        \"Seconds\": 58, // 48 bits\n\
        \"Ticks\": 48, // 15 bits\n\
        \"U\": 0 // 1 bit\n\
    },\n\
    \"ActiveTimestamp\": {\n\
        \"Seconds\": 58, // 48 bits\n\
        \"Ticks\": 48, // 15 bits\n\
        \"U\": 0 // 1 bit\n\
    },\n\
    \"ExtendedPanId\": \"1122334455667788\",\n\
    \"NetworkName\": \"net1\",\n\
    \"SecurityPolicy\": {\n\
        \"Flags\": \"ff\",\n\
        \"RotationTime\": 673\n\
    }\n\
}";
    const std::string mepdsFileName            = "./mepds.json";
    ASSERT_EQ(WriteFile(multiEntryPendingDataset, mepdsFileName).mCode, ErrorCode::kNone);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 0x1122334455667788ul, "", "", Timestamp{0, 0, 0}, 0, "",
                                             ByteArray{}, "domain1", 0, 0, "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    ASSERT_EQ(ctx.mRegistry->Add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 0x99AABBCCDDEEFF00ul, "", "", Timestamp{0, 0, 0}, 0, "",
                                             ByteArray{}, "domain2", 0, 0, "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              RegistryStatus::kSuccess);
    BorderRouter br;
    br.mNetworkId = 0;
    ASSERT_EQ(ctx.mRegistry->SetCurrentNetwork(br), RegistryStatus::kSuccess);

    CommissionerAppMockPtr commissionerAppMock{new CommissionerAppMock()};
    EXPECT_CALL(ctx.mCommissionerAppStaticExpecter, Create(_, _))
        .WillOnce(
            DoAll(WithArg<0>([&](std::shared_ptr<CommissionerApp> &a) { a = commissionerAppMock; }), Return(Error{})));
    EXPECT_CALL(*commissionerAppMock, SetActiveDataset(_)).WillOnce(Return(Error{}));
    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr = ctx.mInterpreter.ParseExpression("start");
    EXPECT_TRUE(ctx.mInterpreter.Eval(expr).HasNoError());
    ctx.mInterpreter.mContext.Cleanup();
    ctx.mInterpreter.mJobManager->CleanupJobs();

    expr  = ctx.mInterpreter.ParseExpression(std::string("opdataset set active --import ") + mepdsFileName);
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());

    unlink(mepdsFileName.c_str());
}
