/**
 * TODO Put copyright here
 */
/**
 * @file Interpreter unit tests
 */

#include "gmock/gmock.h"

#define private public

#include "border_agent_functions_mock.hpp"
#include "commissioner_app_mock.hpp"
#include "app/cli/interpreter.hpp"
#include "app/cli/job_manager.hpp"
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

class InterpreterTestSuite : public testing::Test
{
public:
    using Registry              = ::ot::commissioner::persistent_storage::registry;
    using RegistryStatus        = ::ot::commissioner::persistent_storage::registry_status;
    using PersistentStorageJson = ot::commissioner::persistent_storage::persistent_storage_json;
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
    ASSERT_EQ(ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              registry_status::REG_SUCCESS);
    ASSERT_EQ(ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              registry_status::REG_SUCCESS);
    border_router br;
    br.nwk_id = 0;
    ASSERT_EQ(ctx.mRegistry->set_current_network(br), registry_status::REG_SUCCESS);

    std::string             command;
    Interpreter::Expression expr;
    Interpreter::Expression ret;
    NidArray                nids;

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
    NidArray                nids;
    expr = ctx.mInterpreter.ParseExpression("start --nwk all other");
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_FALSE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
}

TEST_F(InterpreterTestSuite, MNSV_ThisResolvesWithCurrentSet)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              registry_status::REG_SUCCESS);
    border_router br;
    br.nwk_id = 0;
    ASSERT_EQ(ctx.mRegistry->set_current_network(br), registry_status::REG_SUCCESS);

    Interpreter::Expression expr, ret;
    NidArray                nids;
    expr = ctx.mInterpreter.ParseExpression("start --nwk this");
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_TRUE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
}

TEST_F(InterpreterTestSuite, MNSV_ThisUnresolvesWithCurrentUnset)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              registry_status::REG_SUCCESS);

    Interpreter::Expression expr, ret;
    NidArray                nids;
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
        ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    // No current network selected

    Interpreter::Expression expr, ret;
    NidArray                nids;
    expr = ctx.mInterpreter.ParseExpression("start --nwk all");
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_TRUE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
    EXPECT_NE(std::find(nids.begin(), nids.end(), 1), nids.end());
    EXPECT_NE(std::find(nids.begin(), nids.end(), 2), nids.end());
    ctx.mInterpreter.mContext.Cleanup();
    ctx.mInterpreter.mJobManager->CleanupJobs();
    ret.clear();
    nids.clear();

    expr = ctx.mInterpreter.ParseExpression("start --nwk other");
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_TRUE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
    EXPECT_NE(std::find(nids.begin(), nids.end(), 1), nids.end());
    EXPECT_NE(std::find(nids.begin(), nids.end(), 2), nids.end());
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
        ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    border_router br;
    br.nwk_id = 0;
    ASSERT_EQ(ctx.mRegistry->set_current_network(br), registry_status::REG_SUCCESS);

    Interpreter::Expression expr, ret;
    NidArray                nids;
    expr = ctx.mInterpreter.ParseExpression("start --nwk all");
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_TRUE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
    EXPECT_NE(std::find(nids.begin(), nids.end(), 1), nids.end());
    EXPECT_NE(std::find(nids.begin(), nids.end(), 2), nids.end());
    ctx.mInterpreter.mContext.Cleanup();
    ctx.mInterpreter.mJobManager->CleanupJobs();
    ret.clear();
    nids.clear();

    expr = ctx.mInterpreter.ParseExpression("start --nwk other");
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_TRUE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
    EXPECT_EQ(std::find(nids.begin(), nids.end(), 1), nids.end());
    EXPECT_NE(std::find(nids.begin(), nids.end(), 2), nids.end());
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
        ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);

    Interpreter::Expression expr, ret;
    NidArray                nids;
    expr = ctx.mInterpreter.ParseExpression("start --dom domain1 --dom domain2");
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_FALSE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
}

TEST_F(InterpreterTestSuite, MNSV_UnexistingDomainResolveFails)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);

    Interpreter::Expression expr, ret;
    NidArray                nids;
    expr = ctx.mInterpreter.ParseExpression("start --dom domain2");
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_FALSE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
}

TEST_F(InterpreterTestSuite, MNSV_ExistingDomainResolves)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);

    Interpreter::Expression expr, ret;
    NidArray                nids;
    expr = ctx.mInterpreter.ParseExpression("start --dom domain1");
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_TRUE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
    EXPECT_NE(std::find(nids.begin(), nids.end(), 1), nids.end());
    EXPECT_EQ(std::find(nids.begin(), nids.end(), 2), nids.end());
}

TEST_F(InterpreterTestSuite, MNSV_AmbiguousNwkResolutionFails)
{
    TestContext ctx;
    InitContext(ctx);

    network_id nid;
    ASSERT_EQ(ctx.mRegistry->storage->add(network{EMPTY_ID, EMPTY_ID, "net1", 1, 0, "pan1", "", 0}, nid),
              registry_status::REG_SUCCESS);
    ASSERT_EQ(ctx.mRegistry->storage->add(network{EMPTY_ID, EMPTY_ID, "net2", 2, 0, "pan1", "", 0}, nid),
              registry_status::REG_SUCCESS);

    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);

    Interpreter::Expression expr, ret;
    NidArray                nids;
    expr = ctx.mInterpreter.ParseExpression("start --nwk pan1");
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_FALSE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
}

TEST_F(InterpreterTestSuite, MNSV_SameResolutionFromTwoAliasesCollapses)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);

    Interpreter::Expression expr, ret;
    NidArray                nids;
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
        ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);

    Interpreter::Expression expr, ret;
    NidArray                nids;
    expr = ctx.mInterpreter.ParseExpression("start --nwk 1 all");
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_FALSE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
}

TEST_F(InterpreterTestSuite, MNSV_DomThisResolvesWithRespectToSelection)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    border_router br;
    br.nwk_id = 0;
    ASSERT_EQ(ctx.mRegistry->set_current_network(br), registry_status::REG_SUCCESS);

    Interpreter::Expression expr, ret;
    NidArray                nids;
    expr = ctx.mInterpreter.ParseExpression("start --dom this");
    EXPECT_EQ(ctx.mInterpreter.ReParseMultiNetworkSyntax(expr, ret).mCode, ErrorCode::kNone);
    EXPECT_TRUE(ctx.mInterpreter.ValidateMultiNetworkSyntax(ret, nids).HasNoError());
    EXPECT_EQ(nids.size(), 1);
    EXPECT_EQ(nids[0], 1);
}

TEST_F(InterpreterTestSuite, MNSV_NoAliasesResolvesToThisNwk)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.3", 20003, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net3", 3, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.4", 20004, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net4", 4, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    border_router br;
    br.nwk_id = 2;
    ASSERT_EQ(ctx.mRegistry->set_current_network(br), registry_status::REG_SUCCESS);
    uint64_t nid;
    ASSERT_EQ(ctx.mRegistry->get_current_network_xpan(nid), registry_status::REG_SUCCESS);
    ASSERT_EQ(nid, 3);

    Interpreter::Expression expr, ret;
    NidArray                nids;
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
        ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.3", 20003, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net3", 3, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.4", 20004, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net4", 4, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    border_router br;
    br.nwk_id = 0;
    ASSERT_EQ(ctx.mRegistry->set_current_network(br), registry_status::REG_SUCCESS);

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
        ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);

    Interpreter::Expression expr;
    expr = ctx.mInterpreter.ParseExpression("br scan --export ./2.json");
    EXPECT_TRUE(ctx.mInterpreter.Eval(expr).HasNoError());
}

TEST_F(InterpreterTestSuite, IESV_SingleImportFileMustPass)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);

    CommissionerAppMockPtr commissionerAppMock{new CommissionerAppMock()};
    EXPECT_CALL(ctx.mCommissionerAppStaticExpecter, Create(_, _))
        .WillOnce(
            DoAll(WithArg<0>([&](std::shared_ptr<CommissionerApp> &a) { a = commissionerAppMock; }), Return(Error{})));
    EXPECT_CALL(*commissionerAppMock, Start(_, _, _)).WillOnce(Return(Error{}));

    // Create CommissionerAppMock for the network
    Interpreter::Expression expr;
    expr = ctx.mInterpreter.ParseExpression("start --nwk 1");
    EXPECT_TRUE(ctx.mInterpreter.Eval(expr).HasNoError());

    border_router br;
    br.nwk_id = 0;
    ASSERT_EQ(ctx.mRegistry->set_current_network(br), registry_status::REG_SUCCESS);

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
    \"PanId\": 64206, // 0xface\n\
    \"SecurityPolicy\": {\n\
        \"Flags\": \"f8\", // 0xf8\n\
        \"RotationTime\": 672\n\
    }\n\
}";

    EXPECT_EQ(WriteFile(jsonStr, "./json.json").mCode, ErrorCode::kNone);

    EXPECT_CALL(*commissionerAppMock, SetActiveDataset(_)).WillOnce(Return(Error{}));
    expr = ctx.mInterpreter.ParseExpression("opdataset set active --import ./json.json");
    EXPECT_TRUE(ctx.mInterpreter.Eval(expr).HasNoError());
}

TEST_F(InterpreterTestSuite, IESV_NoExportFileMustFail)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);

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
        ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);

    CommissionerAppMockPtr commissionerAppMock{new CommissionerAppMock()};
    EXPECT_CALL(ctx.mCommissionerAppStaticExpecter, Create(_, _))
        .WillOnce(
            DoAll(WithArg<0>([&](std::shared_ptr<CommissionerApp> &a) { a = commissionerAppMock; }), Return(Error{})));
    EXPECT_CALL(*commissionerAppMock, Start(_, _, _)).WillOnce(Return(Error{}));

    // Create CommissionerAppMock for the network
    Interpreter::Expression expr;
    expr = ctx.mInterpreter.ParseExpression("start --nwk 1");
    EXPECT_TRUE(ctx.mInterpreter.Eval(expr).HasNoError());

    border_router br;
    br.nwk_id = 0;
    ASSERT_EQ(ctx.mRegistry->set_current_network(br), registry_status::REG_SUCCESS);

    expr = ctx.mInterpreter.ParseExpression("opdataset set active --import");
    EXPECT_FALSE(ctx.mInterpreter.Eval(expr).HasNoError());
}

TEST_F(InterpreterTestSuite, IESV_TwoImportExportClausesMustFail)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);

    Interpreter::Expression expr;
    Interpreter::Value      value;
    expr  = ctx.mInterpreter.ParseExpression("br scan --export 1.json --export 2.json");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_FALSE(value.HasNoError());

    expr = ctx.mInterpreter.ParseExpression("opdataset set active --import 1.json --import 2.json");
    EXPECT_FALSE(ctx.mInterpreter.Eval(expr).HasNoError());
}

// Collect Multi-Network Output test group
TEST_F(InterpreterTestSuite, CMNO_MultipleSuccessfullJobsPass)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);

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
        ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);

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
        ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);

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

    expr  = ctx.mInterpreter.ParseExpression("start --nwk all");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());

    expr  = ctx.mInterpreter.ParseExpression("start --dom domain1");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_StartCurrentNetworkSuccess)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);

    border_router br;
    br.nwk_id = 0;
    ASSERT_EQ(ctx.mRegistry->set_current_network(br), registry_status::REG_SUCCESS);

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

    expr  = ctx.mInterpreter.ParseExpression("start --dom this");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_StartLegacySyntaxSuccess)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);

    border_router br;
    br.nwk_id = 0;
    ASSERT_EQ(ctx.mRegistry->set_current_network(br), registry_status::REG_SUCCESS);

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
        ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);

    border_router br;
    br.nwk_id = 0;
    ASSERT_EQ(ctx.mRegistry->set_current_network(br), registry_status::REG_SUCCESS);

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
        ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);

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

    expr  = ctx.mInterpreter.ParseExpression("stop --nwk net1 net2");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, PC_StopLegacySyntaxSuccess)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);

    border_router br;
    br.nwk_id = 0;
    ASSERT_EQ(ctx.mRegistry->set_current_network(br), registry_status::REG_SUCCESS);

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

TEST_F(InterpreterTestSuite, DISABLED_PC_Active)
{
    TestContext ctx;
    InitContext(ctx);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);
    ASSERT_EQ(
        ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                       "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0, "",
                                       0, 0x1F | BorderAgent::kDomainNameBit | BorderAgent::kExtendedPanIdBit}),
        registry_status::REG_SUCCESS);

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, IsActive()).Times(2).WillOnce(Return(false)).WillOnce(Return(true));

    Interpreter::Expression expr;
    Interpreter::Value      value;

    // Debug running 'active' from the default CommissionerApp object
    expr  = ctx.mInterpreter.ParseExpression("active");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    // TODO add result verification

    expr  = ctx.mInterpreter.ParseExpression("active");
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
    EXPECT_EQ(WriteFile("cert", "./cert").mCode, ErrorCode::kNone);
    EXPECT_CALL(*ctx.mDefaultCommissionerObject, SetToken(_, _)).WillOnce(Return(Error{}));
    expr  = ctx.mInterpreter.ParseExpression("token set ./token ./cert");
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
    ASSERT_EQ(ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              registry_status::REG_SUCCESS);
    ASSERT_EQ(ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              registry_status::REG_SUCCESS);

    network nwk;
    EXPECT_EQ(ctx.mRegistry->get_current_network(nwk), registry_status::REG_SUCCESS);
    EXPECT_EQ(nwk.id.id, EMPTY_ID);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("network select none");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    EXPECT_EQ(ctx.mRegistry->get_current_network(nwk), registry_status::REG_SUCCESS);
    EXPECT_EQ(nwk.id.id, EMPTY_ID);
}

TEST_F(InterpreterTestSuite, PC_NetworkSelectNoneOnSelected)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              registry_status::REG_SUCCESS);
    ASSERT_EQ(ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              registry_status::REG_SUCCESS);
    border_router br;
    br.nwk_id = 0;
    ASSERT_EQ(ctx.mRegistry->set_current_network(br), registry_status::REG_SUCCESS);

    network nwk;
    EXPECT_EQ(ctx.mRegistry->get_current_network(nwk), registry_status::REG_SUCCESS);
    EXPECT_EQ(nwk.id.id, 0);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("network select none");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    EXPECT_EQ(ctx.mRegistry->get_current_network(nwk), registry_status::REG_SUCCESS);
    EXPECT_EQ(nwk.id.id, EMPTY_ID);
}

TEST_F(InterpreterTestSuite, PC_NetworkSelectOnEmpty)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0xFF}),
              registry_status::REG_SUCCESS);
    ASSERT_EQ(ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0xFF}),
              registry_status::REG_SUCCESS);

    network nwk;
    EXPECT_EQ(ctx.mRegistry->get_current_network(nwk), registry_status::REG_SUCCESS);
    EXPECT_EQ(nwk.id.id, EMPTY_ID);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    // TODO TBD XPAN format on enter
    expr  = ctx.mInterpreter.ParseExpression("network select 1");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    EXPECT_EQ(ctx.mRegistry->get_current_network(nwk), registry_status::REG_SUCCESS);
    EXPECT_EQ(nwk.id.id, 0);
}

TEST_F(InterpreterTestSuite, PC_NetworkSelectAnother)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0xFF}),
              registry_status::REG_SUCCESS);
    ASSERT_EQ(ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0xFF}),
              registry_status::REG_SUCCESS);
    border_router br;
    br.nwk_id = 0;
    ASSERT_EQ(ctx.mRegistry->set_current_network(br), registry_status::REG_SUCCESS);

    network nwk;
    EXPECT_EQ(ctx.mRegistry->get_current_network(nwk), registry_status::REG_SUCCESS);
    EXPECT_EQ(nwk.id.id, 0);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("network select 1");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    EXPECT_EQ(ctx.mRegistry->get_current_network(nwk), registry_status::REG_SUCCESS);
    EXPECT_EQ(nwk.id.id, 1);
}

TEST_F(InterpreterTestSuite, PC_NetworkSelectNonexisting)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              registry_status::REG_SUCCESS);
    ASSERT_EQ(ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              registry_status::REG_SUCCESS);
    border_router br;
    br.nwk_id = 0;
    ASSERT_EQ(ctx.mRegistry->set_current_network(br), registry_status::REG_SUCCESS);

    network nwk;
    EXPECT_EQ(ctx.mRegistry->get_current_network(nwk), registry_status::REG_SUCCESS);
    EXPECT_EQ(nwk.id.id, 0);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("network select 3");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    EXPECT_EQ(ctx.mRegistry->get_current_network(nwk), registry_status::REG_SUCCESS);
    EXPECT_EQ(nwk.id.id, 0);
}

TEST_F(InterpreterTestSuite, PC_NetworkIdentify)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              registry_status::REG_SUCCESS);
    ASSERT_EQ(ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              registry_status::REG_SUCCESS);
    border_router br;
    br.nwk_id = 0;
    ASSERT_EQ(ctx.mRegistry->set_current_network(br), registry_status::REG_SUCCESS);

    network nwk;
    EXPECT_EQ(ctx.mRegistry->get_current_network(nwk), registry_status::REG_SUCCESS);
    EXPECT_EQ(nwk.id.id, 0);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("network identify");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, DISABLED_PC_NetworkList)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0xFF}),
              registry_status::REG_SUCCESS);
    ASSERT_EQ(ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0xFF}),
              registry_status::REG_SUCCESS);
    border_router br;
    br.nwk_id = 0;
    ASSERT_EQ(ctx.mRegistry->set_current_network(br), registry_status::REG_SUCCESS);

    network nwk;
    EXPECT_EQ(ctx.mRegistry->get_current_network(nwk), registry_status::REG_SUCCESS);
    EXPECT_EQ(nwk.id.id, 0);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    CommissionerAppMockPtr pcaMock{new CommissionerAppMock()};
    EXPECT_CALL(ctx.mCommissionerAppStaticExpecter, Create(_, _))
        .WillOnce(DoAll(WithArg<0>([&](std::shared_ptr<CommissionerApp> &a) { a = pcaMock; }), Return(Error{})));

    expr  = ctx.mInterpreter.ParseExpression("network list --dom domain1");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());

    expr  = ctx.mInterpreter.ParseExpression("network list --nwk other");
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

TEST_F(InterpreterTestSuite, DISABLED_PC_OpdatasetGetActive)
{
    TestContext ctx;
    InitContext(ctx);

    EXPECT_CALL(*ctx.mDefaultCommissionerObject, GetActiveDataset(_, _))
        .Times(2)
        .WillRepeatedly(DoAll(WithArg<0>([](ActiveOperationalDataset &a) {
                                  a.mPanId        = 1;
                                  a.mPresentFlags = ActiveOperationalDataset::kPanIdBit;
                              }),
                              Return(Error{})));

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("opdataset get active");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());

    // TODO Implement export
    unlink("./aods.json");
    expr  = ctx.mInterpreter.ParseExpression("opdataset get active --export ./aods.json");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    std::string jsonStr;
    EXPECT_EQ(ReadFile(jsonStr, "./aods.json").GetCode(), ErrorCode::kNone);
    nlohmann::json json = nlohmann::json::parse(jsonStr);
    EXPECT_TRUE(json.contains("PanId"));
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
    ASSERT_EQ(ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              registry_status::REG_SUCCESS);
    ASSERT_EQ(ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              registry_status::REG_SUCCESS);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("domain list");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
}

TEST_F(InterpreterTestSuite, DISABLED_PC_BrScanExport)
{
    TestContext ctx;
    InitContext(ctx);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    std::string jsonFileName = "./br-list.json";
    unlink(jsonFileName.c_str());
    // TODO implementation pending
    expr  = ctx.mInterpreter.ParseExpression(std::string("br scan --export ") + jsonFileName);
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    std::string jsonStr;
    EXPECT_EQ(ReadFile(jsonStr, jsonFileName).GetCode(), ErrorCode::kNone);
    EXPECT_GE(jsonStr.length(), 1);
}

TEST_F(InterpreterTestSuite, DISABLED_PC_BrScanExportDirAbsent)
{
    TestContext ctx;
    InitContext(ctx);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    std::string jsonFileName = "./tmpdir/br-list.json";
    ASSERT_EQ(system("rm -rf ./tmpdir"), 0);
    // TODO implementation pending
    expr  = ctx.mInterpreter.ParseExpression(std::string("br scan --export ") + jsonFileName);
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    std::string jsonStr;
    EXPECT_EQ(ReadFile(jsonStr, jsonFileName).GetCode(), ErrorCode::kNone);
    EXPECT_GE(jsonStr.length(), 1);
}

TEST_F(InterpreterTestSuite, DISABLED_PC_BrAdd)
{
    TestContext ctx;
    InitContext(ctx);

    std::string brJson       = "[\n\
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
        \"Addr\": \"1234::5678\",\n\
        \"Port\": 2000,\n\
        \"ThreadVersion\": \"th1.2\",\n\
        \"State\": 0,\n\
        \"NetworkName\": \"net1\",\n\
        \"ExtendedPanId\": 1234,\n\
        \"DomainName\": \"dom1\"\n\
    }\n]";
    std::string jsonFileName = "./br-list.json";
    EXPECT_EQ(WriteFile(brJson, jsonFileName).GetCode(), ErrorCode::kNone);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    // TODO implementation pending
    expr  = ctx.mInterpreter.ParseExpression(std::string("br add ") + jsonFileName);
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    registry::BorderRouterArray bra;
    EXPECT_EQ(ctx.mRegistry->get_all_border_routers(bra), registry_status::REG_SUCCESS);
    EXPECT_EQ(bra.size(), 2);
}

TEST_F(InterpreterTestSuite, PC_BrList)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              registry_status::REG_SUCCESS);
    ASSERT_EQ(ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 2, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              registry_status::REG_SUCCESS);

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

TEST_F(InterpreterTestSuite, DISABLED_PC_BrDelete)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              registry_status::REG_SUCCESS);
    ASSERT_EQ(ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              registry_status::REG_SUCCESS);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("br delete 1");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    registry::BorderRouterArray bra;
    EXPECT_EQ(ctx.mRegistry->get_all_border_routers(bra), registry_status::REG_SUCCESS);
    EXPECT_EQ(bra.size(), 1);
}

TEST_F(InterpreterTestSuite, DISABLED_PC_BrDeleteNetwork)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              registry_status::REG_SUCCESS);
    ASSERT_EQ(ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              registry_status::REG_SUCCESS);
    ASSERT_EQ(ctx.mRegistry->add(BorderAgent{"127.0.0.3", 20003, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              registry_status::REG_SUCCESS);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("br delete --nwk net1");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    registry::BorderRouterArray bra;
    EXPECT_EQ(ctx.mRegistry->get_all_border_routers(bra), registry_status::REG_SUCCESS);
    EXPECT_EQ(bra.size(), 1);
}

TEST_F(InterpreterTestSuite, DISABLED_PC_BrDeleteDomain)
{
    TestContext ctx;
    InitContext(ctx);

    ASSERT_NE(ctx.mRegistry, nullptr);
    ASSERT_EQ(ctx.mRegistry->add(BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              registry_status::REG_SUCCESS);
    ASSERT_EQ(ctx.mRegistry->add(BorderAgent{"127.0.0.2", 20002, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net2", 1, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain2", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              registry_status::REG_SUCCESS);
    ASSERT_EQ(ctx.mRegistry->add(BorderAgent{"127.0.0.3", 20003, ByteArray{}, "1.1", BorderAgent::State{0, 0, 0, 0, 0},
                                             "net1", 0, "", "", Timestamp{0, 0, 0}, 0, "", ByteArray{}, "domain1", 0, 0,
                                             "", 0, 0x1F | BorderAgent::kDomainNameBit}),
              registry_status::REG_SUCCESS);

    Interpreter::Expression expr;
    Interpreter::Value      value;

    expr  = ctx.mInterpreter.ParseExpression("br delete --dom domain1");
    value = ctx.mInterpreter.Eval(expr);
    EXPECT_TRUE(value.HasNoError());
    registry::BorderRouterArray bra;
    EXPECT_EQ(ctx.mRegistry->get_all_border_routers(bra), registry_status::REG_SUCCESS);
    EXPECT_EQ(bra.size(), 1);
}
