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
 * @file Job manager unit tests
 */

#include "gmock/gmock.h"

#define private public

#include "commissioner_app_mock.hpp"
#include "app/cli/interpreter.hpp"
#include "app/cli/job.hpp"
#include "app/cli/job_manager.hpp"
#include "app/file_util.hpp"
#include "app/ps/persistent_storage_json.hpp"
#include "app/ps/registry.hpp"

#include <sys/stat.h>
#include <thread>

using namespace ot::commissioner;

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::WithArg;
using testing::WithArgs;

class JobManagerTestSuite : public testing::Test
{
public:
    using Registry              = ot::commissioner::persistent_storage::Registry;
    using RegistryStatus        = Registry::Status;
    using PersistentStorageJson = ot::commissioner::persistent_storage::PersistentStorageJson;
    typedef std::shared_ptr<CommissionerAppMock> CommissionerAppMockPtr;

    struct TestContext
    {
        TestContext()
            : mPS{}
            , mRegistry{}
            , mConf{}
            , mInterpreter{}
            , mJobManager{mInterpreter}
            , mDefaultCommissioner{new CommissionerAppMock()}
        {
            mPS       = std::shared_ptr<PersistentStorageJson>(new PersistentStorageJson(""));
            mRegistry = std::shared_ptr<Registry>(new Registry(mPS.get()));
            SetCommissionerAppStaticExpecter(&mCommissionerAppStaticExpecter);
        }
        ~TestContext() { ClearCommissionerAppStaticExpecter(); }

        std::shared_ptr<PersistentStorageJson> mPS;
        std::shared_ptr<Registry>              mRegistry;

        Config                               mConf;
        Interpreter                          mInterpreter;
        JobManager                           mJobManager;
        std::shared_ptr<CommissionerAppMock> mDefaultCommissioner;
        CommissionerAppStaticExpecter        mCommissionerAppStaticExpecter;
    };

    JobManagerTestSuite()
        : Test()
    {
    }
    virtual ~JobManagerTestSuite() = default;

    void SetInitialExpectations(TestContext &aContext)
    {
        ASSERT_TRUE(aContext.mPS);
        EXPECT_CALL(aContext.mCommissionerAppStaticExpecter, Create(_, _))
            .Times(1)
            .WillOnce(DoAll(WithArg<0>([&](std::shared_ptr<CommissionerApp> &a) { a = aContext.mDefaultCommissioner; }),
                            Return(Error{})));

        ASSERT_EQ(aContext.mPS->Open(), persistent_storage::PersistentStorage::Status::PS_SUCCESS);
    }

    void Init(TestContext &aContext, const std::string &aSMRoot)
    {
        aContext.mConf.mThreadSMRoot = aSMRoot;
        EXPECT_EQ(aContext.mJobManager.Init(aContext.mConf).mCode, ErrorCode::kNone);
        aContext.mInterpreter.mRegistry = aContext.mRegistry;
    }
};

TEST_F(JobManagerTestSuite, TestInit)
{
    TestContext ctx;
    SetInitialExpectations(ctx);
    Init(ctx, ".");
}

TEST_F(JobManagerTestSuite, StartStopSuccess)
// Stop jobs fail
{
    TestContext ctx;
    SetInitialExpectations(ctx);

    // Formally set default PSKc
    ctx.mConf.mPSKc = {'1', '0'};

    using namespace ot::commissioner::persistent_storage;
    // Prepare test data
    network_id       nid;
    border_router_id rid;

    ASSERT_EQ(ctx.mPS->Add(network{0, 0, "pan1", 1, 1, "1", "", 0}, nid), PersistentStorage::Status::PS_SUCCESS);
    EXPECT_EQ(nid.id, 0);

    ASSERT_EQ(ctx.mPS->Add(border_router{0, nid,
                                         BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1",
                                                     BorderAgent::State{0, 0, 0, 0, 0}, "", 0, "", "",
                                                     Timestamp{0, 0, 0}, 0, "", ByteArray{}, "", 0, 0, "", 0, 0x0F}},
                           rid),
              PersistentStorage::Status::PS_SUCCESS);
    EXPECT_EQ(rid.id, 0);

    ASSERT_EQ(ctx.mPS->Add(network{0, 0, "pan2", 2, 2, "2", "", 0}, nid), PersistentStorage::Status::PS_SUCCESS);
    EXPECT_EQ(nid.id, 1);

    ASSERT_EQ(ctx.mPS->Add(border_router{0, nid,
                                         BorderAgent{"127.0.0.1", 20002, ByteArray{}, "1.1",
                                                     BorderAgent::State{0, 0, 0, 0, 0}, "", 0, "", "",
                                                     Timestamp{0, 0, 0}, 0, "", ByteArray{}, "", 0, 0, "", 0, 0x0F}},
                           rid),
              PersistentStorage::Status::PS_SUCCESS);
    EXPECT_EQ(rid.id, 1);

    ASSERT_EQ(ctx.mPS->Add(network{0, 0, "pan3", 3, 3, "3", "", 0}, nid), PersistentStorage::Status::PS_SUCCESS);
    EXPECT_EQ(nid.id, 2);

    ASSERT_EQ(ctx.mPS->Add(border_router{0, nid,
                                         BorderAgent{"127.0.0.1", 20003, ByteArray{}, "1.1",
                                                     BorderAgent::State{0, 0, 0, 0, 0}, "", 0, "", "",
                                                     Timestamp{0, 0, 0}, 0, "", ByteArray{}, "", 0, 0, "", 0, 0x0F}},
                           rid),
              PersistentStorage::Status::PS_SUCCESS);
    EXPECT_EQ(rid.id, 2);

    Init(ctx, ".");

    uint8_t                camIdx                  = 0;
    CommissionerAppMockPtr commissionerAppMocks[3] = {CommissionerAppMockPtr{new CommissionerAppMock()},
                                                      CommissionerAppMockPtr{new CommissionerAppMock()},
                                                      CommissionerAppMockPtr{new CommissionerAppMock()}};

    EXPECT_CALL(ctx.mCommissionerAppStaticExpecter, Create(_, _))
        .Times(3)
        .WillRepeatedly(
            DoAll(WithArg<0>([&](std::shared_ptr<CommissionerApp> &a) { a = commissionerAppMocks[camIdx++]; }),
                  Return(Error{})));

    EXPECT_EQ(ctx.mJobManager.PrepareJobs({"start"}, {0x1, 0x2, 0x3}, false).mCode, ErrorCode::kNone);
    EXPECT_EQ(camIdx, 3);

    std::thread::id current = std::this_thread::get_id();
    std::thread::id ids[3];
    for (camIdx = 0; camIdx < 3; ++camIdx)
    {
        EXPECT_CALL(*commissionerAppMocks[camIdx], Start(_, _, _))
            .Times(1)
            .WillOnce(DoAll(Invoke([camIdx, &ids]() { ids[camIdx] = std::this_thread::get_id(); }), Return(Error{})));
    }

    ctx.mJobManager.RunJobs();
    for (camIdx = 0; camIdx < 3; ++camIdx)
    {
        // Expected function did't run in the current thread
        EXPECT_NE(current, ids[camIdx]);
        // Expected function did actually run (thread identifier was set to the value other than default)
        EXPECT_NE(ids[camIdx], std::thread::id());
    }

    for (camIdx = 0; camIdx < 3; ++camIdx)
    {
        ids[camIdx] = current;
        EXPECT_CALL(*commissionerAppMocks[camIdx], Stop()).WillOnce(Invoke([camIdx, &ids]() {
            ids[camIdx] = std::this_thread::get_id();
        }));
        EXPECT_CALL(*commissionerAppMocks[camIdx], IsActive()).WillOnce(Return(true));
    }
    ctx.mJobManager.CleanupJobs();

    EXPECT_EQ(ctx.mJobManager.PrepareJobs({"stop"}, {0x1, 0x2, 0x3}, false).mCode, ErrorCode::kNone);
    ctx.mJobManager.RunJobs();
    for (camIdx = 0; camIdx < 3; ++camIdx)
    {
        // Expected function didn't run in the current thread
        EXPECT_NE(current, ids[camIdx]);
        // Expected function did actually run (thread identifier was set to the value other than default)
        EXPECT_NE(ids[camIdx], std::thread::id());
    }
}

TEST_F(JobManagerTestSuite, StartCancel)
{
    TestContext ctx;
    SetInitialExpectations(ctx);

    // Formally set default PSKc
    ctx.mConf.mPSKc = {'1', '0'};

    using namespace ot::commissioner::persistent_storage;
    // Prepare test data
    network_id       nid;
    border_router_id rid;

    ASSERT_EQ(ctx.mPS->Add(network{0, 0, "pan1", 1, 1, "1", "", 0}, nid), PersistentStorage::Status::PS_SUCCESS);
    EXPECT_EQ(nid.id, 0);

    ASSERT_EQ(ctx.mPS->Add(border_router{0, nid,
                                         BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1",
                                                     BorderAgent::State{0, 0, 0, 0, 0}, "", 0, "", "",
                                                     Timestamp{0, 0, 0}, 0, "", ByteArray{}, "", 0, 0, "", 0, 0x0F}},
                           rid),
              PersistentStorage::Status::PS_SUCCESS);
    EXPECT_EQ(rid.id, 0);

    ASSERT_EQ(ctx.mPS->Add(network{0, 0, "pan2", 2, 2, "2", "", 0}, nid), PersistentStorage::Status::PS_SUCCESS);
    EXPECT_EQ(nid.id, 1);

    ASSERT_EQ(ctx.mPS->Add(border_router{0, nid,
                                         BorderAgent{"127.0.0.1", 20002, ByteArray{}, "1.1",
                                                     BorderAgent::State{0, 0, 0, 0, 0}, "", 0, "", "",
                                                     Timestamp{0, 0, 0}, 0, "", ByteArray{}, "", 0, 0, "", 0, 0x0F}},
                           rid),
              PersistentStorage::Status::PS_SUCCESS);
    EXPECT_EQ(rid.id, 1);

    ASSERT_EQ(ctx.mPS->Add(network{0, 0, "pan3", 3, 3, "3", "", 0}, nid), PersistentStorage::Status::PS_SUCCESS);
    EXPECT_EQ(nid.id, 2);

    ASSERT_EQ(ctx.mPS->Add(border_router{0, nid,
                                         BorderAgent{"127.0.0.1", 20003, ByteArray{}, "1.1",
                                                     BorderAgent::State{0, 0, 0, 0, 0}, "", 0, "", "",
                                                     Timestamp{0, 0, 0}, 0, "", ByteArray{}, "", 0, 0, "", 0, 0x0F}},
                           rid),
              PersistentStorage::Status::PS_SUCCESS);
    EXPECT_EQ(rid.id, 2);

    Init(ctx, ".");

    uint8_t                camIdx                  = 0;
    CommissionerAppMockPtr commissionerAppMocks[3] = {CommissionerAppMockPtr{new CommissionerAppMock()},
                                                      CommissionerAppMockPtr{new CommissionerAppMock()},
                                                      CommissionerAppMockPtr{new CommissionerAppMock()}};

    EXPECT_CALL(ctx.mCommissionerAppStaticExpecter, Create(_, _))
        .Times(3)
        .WillRepeatedly(
            DoAll(WithArg<0>([&](std::shared_ptr<CommissionerApp> &a) { a = commissionerAppMocks[camIdx++]; }),
                  Return(Error{})));

    EXPECT_EQ(ctx.mJobManager.PrepareJobs({"start"}, {0x1, 0x2, 0x3}, false).mCode, ErrorCode::kNone);
    EXPECT_EQ(camIdx, 3);

    for (camIdx = 0; camIdx < 2; ++camIdx)
    {
        EXPECT_CALL(*commissionerAppMocks[camIdx], Start(_, _, _)).WillOnce(Return(Error{}));
        EXPECT_CALL(*commissionerAppMocks[camIdx], CancelRequests()).Times(1);
    }
    volatile bool wasThreadRunning = false;
    volatile bool shouldStop       = false;
    EXPECT_CALL(*commissionerAppMocks[2], Start(_, _, _)).Times(1).WillOnce(Invoke([&]() {
        wasThreadRunning = true;
        while (!shouldStop)
        {
        };
        return Error{};
    }));
    EXPECT_CALL(*commissionerAppMocks[2], CancelRequests()).Times(1).WillOnce(Invoke([&]() { shouldStop = true; }));

    for (auto job : ctx.mJobManager.mJobPool)
    {
        job->Run();
    }
    EXPECT_FALSE(shouldStop);

    ctx.mJobManager.CancelCommand();
    EXPECT_TRUE(wasThreadRunning);
    EXPECT_TRUE(shouldStop);
}

TEST_F(JobManagerTestSuite, MalformedCredentialsJobCreateFailsByXPan)
{
    // Remove './nwk' subtree
    ASSERT_EQ(system("rm -rf ./dom ./nwk"), 0);

    EXPECT_EQ(mkdir("./nwk", 0777), 0);
    EXPECT_EQ(mkdir("./nwk/0000000000000001", 0777), 0);
    EXPECT_EQ(mkdir("./nwk/0000000000000002", 0777), 0);
    EXPECT_EQ(mkdir("./nwk/0000000000000003", 0777), 0);

    // Loose credentials for network 1
    EXPECT_EQ(WriteFile("1", "./nwk/0000000000000001/ca.pem").mCode, ErrorCode::kNone);
    EXPECT_EQ(WriteFile("1", "./nwk/0000000000000001/priv.pem").mCode, ErrorCode::kNone);
    // Loose credentials for network 2
    EXPECT_EQ(WriteFile("1", "./nwk/0000000000000002/cert.pem").mCode, ErrorCode::kNone);
    EXPECT_EQ(WriteFile("1", "./nwk/0000000000000002/priv.pem").mCode, ErrorCode::kNone);
    // Loose credentials for network 3
    EXPECT_EQ(WriteFile("1", "./nwk/0000000000000003/cert.pem").mCode, ErrorCode::kNone);
    EXPECT_EQ(WriteFile("1", "./nwk/0000000000000003/ca.pem").mCode, ErrorCode::kNone);

    TestContext ctx;
    SetInitialExpectations(ctx);

    using namespace ot::commissioner::persistent_storage;
    // Prepare test data
    network_id       nid;
    border_router_id rid;

    ASSERT_EQ(ctx.mPS->Add(network{0, 0, "pan1", 1, 1, "1", "", 1}, nid), PersistentStorage::Status::PS_SUCCESS);
    EXPECT_EQ(nid.id, 0);

    ASSERT_EQ(ctx.mPS->Add(border_router{0, nid,
                                         BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1",
                                                     BorderAgent::State{0, 0, 0, 0, 0}, "", 0, "", "",
                                                     Timestamp{0, 0, 0}, 0, "", ByteArray{}, "", 0, 0, "", 0, 0x0F}},
                           rid),
              PersistentStorage::Status::PS_SUCCESS);
    EXPECT_EQ(rid.id, 0);

    ASSERT_EQ(ctx.mPS->Add(network{0, 0, "pan2", 2, 2, "2", "", 1}, nid), PersistentStorage::Status::PS_SUCCESS);
    EXPECT_EQ(nid.id, 1);

    ASSERT_EQ(ctx.mPS->Add(border_router{0, nid,
                                         BorderAgent{"127.0.0.1", 20002, ByteArray{}, "1.1",
                                                     BorderAgent::State{0, 0, 0, 0, 0}, "", 0, "", "",
                                                     Timestamp{0, 0, 0}, 0, "", ByteArray{}, "", 0, 0, "", 0, 0x0F}},
                           rid),
              PersistentStorage::Status::PS_SUCCESS);
    EXPECT_EQ(rid.id, 1);

    ASSERT_EQ(ctx.mPS->Add(network{0, 0, "pan3", 3, 3, "3", "", 1}, nid), PersistentStorage::Status::PS_SUCCESS);
    EXPECT_EQ(nid.id, 2);

    ASSERT_EQ(ctx.mPS->Add(border_router{0, nid,
                                         BorderAgent{"127.0.0.1", 20003, ByteArray{}, "1.1",
                                                     BorderAgent::State{0, 0, 0, 0, 0}, "", 0, "", "",
                                                     Timestamp{0, 0, 0}, 0, "", ByteArray{}, "", 0, 0, "", 0, 0x0F}},
                           rid),
              PersistentStorage::Status::PS_SUCCESS);
    EXPECT_EQ(rid.id, 2);

    Init(ctx, ".");

    uint8_t                camIdx                  = 0;
    CommissionerAppMockPtr commissionerAppMocks[3] = {CommissionerAppMockPtr{new CommissionerAppMock()},
                                                      CommissionerAppMockPtr{new CommissionerAppMock()},
                                                      CommissionerAppMockPtr{new CommissionerAppMock()}};

    ON_CALL(ctx.mCommissionerAppStaticExpecter, Create(_, _))
        .WillByDefault(
            DoAll(WithArg<0>([&](std::shared_ptr<CommissionerApp> &a) { a = commissionerAppMocks[camIdx++]; }),
                  Return(Error{})));

    EXPECT_EQ(ctx.mJobManager.PrepareJobs({"start"}, {0x1, 0x2, 0x3}, false).mCode, ErrorCode::kNone);
    EXPECT_EQ(camIdx, 0);
    EXPECT_EQ(ctx.mJobManager.mJobPool.size(), 0);
}

TEST_F(JobManagerTestSuite, MalformedCredentialsJobCreateFailsByName)
{
    // Remove './nwk' subtree
    ASSERT_EQ(system("rm -rf ./dom ./nwk"), 0);

    EXPECT_EQ(mkdir("./nwk", 0777), 0);
    EXPECT_EQ(mkdir("./nwk/pan1", 0777), 0);
    EXPECT_EQ(mkdir("./nwk/pan2", 0777), 0);
    EXPECT_EQ(mkdir("./nwk/pan3", 0777), 0);

    // Loose credentials for network 1
    EXPECT_EQ(WriteFile("1", "./nwk/pan1/ca.pem").mCode, ErrorCode::kNone);
    EXPECT_EQ(WriteFile("1", "./nwk/pan1/priv.pem").mCode, ErrorCode::kNone);
    // Loose credentials for panwork 2
    EXPECT_EQ(WriteFile("1", "./nwk/pan2/cert.pem").mCode, ErrorCode::kNone);
    EXPECT_EQ(WriteFile("1", "./nwk/pan2/priv.pem").mCode, ErrorCode::kNone);
    // Loose credentials for panwork 3
    EXPECT_EQ(WriteFile("1", "./nwk/pan3/cert.pem").mCode, ErrorCode::kNone);
    EXPECT_EQ(WriteFile("1", "./nwk/pan3/ca.pem").mCode, ErrorCode::kNone);

    TestContext ctx;
    SetInitialExpectations(ctx);

    using namespace ot::commissioner::persistent_storage;
    // Prepare test data
    network_id       nid;
    border_router_id rid;

    ASSERT_EQ(ctx.mPS->Add(network{0, 0, "pan1", 1, 1, "1", "", 1}, nid), PersistentStorage::Status::PS_SUCCESS);
    EXPECT_EQ(nid.id, 0);

    ASSERT_EQ(ctx.mPS->Add(border_router{0, nid,
                                         BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1",
                                                     BorderAgent::State{0, 0, 0, 0, 0}, "", 0, "", "",
                                                     Timestamp{0, 0, 0}, 0, "", ByteArray{}, "", 0, 0, "", 0, 0x0F}},
                           rid),
              PersistentStorage::Status::PS_SUCCESS);
    EXPECT_EQ(rid.id, 0);

    ASSERT_EQ(ctx.mPS->Add(network{0, 0, "pan2", 2, 2, "2", "", 1}, nid), PersistentStorage::Status::PS_SUCCESS);
    EXPECT_EQ(nid.id, 1);

    ASSERT_EQ(ctx.mPS->Add(border_router{0, nid,
                                         BorderAgent{"127.0.0.1", 20002, ByteArray{}, "1.1",
                                                     BorderAgent::State{0, 0, 0, 0, 0}, "", 0, "", "",
                                                     Timestamp{0, 0, 0}, 0, "", ByteArray{}, "", 0, 0, "", 0, 0x0F}},
                           rid),
              PersistentStorage::Status::PS_SUCCESS);
    EXPECT_EQ(rid.id, 1);

    ASSERT_EQ(ctx.mPS->Add(network{0, 0, "pan3", 3, 3, "3", "", 1}, nid), PersistentStorage::Status::PS_SUCCESS);
    EXPECT_EQ(nid.id, 2);

    ASSERT_EQ(ctx.mPS->Add(border_router{0, nid,
                                         BorderAgent{"127.0.0.1", 20003, ByteArray{}, "1.1",
                                                     BorderAgent::State{0, 0, 0, 0, 0}, "", 0, "", "",
                                                     Timestamp{0, 0, 0}, 0, "", ByteArray{}, "", 0, 0, "", 0, 0x0F}},
                           rid),
              PersistentStorage::Status::PS_SUCCESS);
    EXPECT_EQ(rid.id, 2);

    Init(ctx, ".");

    uint8_t                camIdx                  = 0;
    CommissionerAppMockPtr commissionerAppMocks[3] = {CommissionerAppMockPtr{new CommissionerAppMock()},
                                                      CommissionerAppMockPtr{new CommissionerAppMock()},
                                                      CommissionerAppMockPtr{new CommissionerAppMock()}};

    ON_CALL(ctx.mCommissionerAppStaticExpecter, Create(_, _))
        .WillByDefault(
            DoAll(WithArg<0>([&](std::shared_ptr<CommissionerApp> &a) { a = commissionerAppMocks[camIdx++]; }),
                  Return(Error{})));

    EXPECT_EQ(ctx.mJobManager.PrepareJobs({"start"}, {0x1, 0x2, 0x3}, false).mCode, ErrorCode::kNone);
    EXPECT_EQ(camIdx, 0);
    EXPECT_EQ(ctx.mJobManager.mJobPool.size(), 0);
}

TEST_F(JobManagerTestSuite, MalformedCredentialsJobCreateFailsByDomain)
{
    // Remove SM subtrees
    ASSERT_EQ(system("rm -rf ./dom ./nwk"), 0);

    EXPECT_EQ(mkdir("./dom", 0777), 0);
    EXPECT_EQ(mkdir("./dom/domain1", 0777), 0);
    EXPECT_EQ(mkdir("./dom/domain2", 0777), 0);
    EXPECT_EQ(mkdir("./dom/domain3", 0777), 0);

    // Loose credentials for domain1
    EXPECT_EQ(WriteFile("1", "./dom/domain1/ca.pem").mCode, ErrorCode::kNone);
    EXPECT_EQ(WriteFile("1", "./dom/domain1/priv.pem").mCode, ErrorCode::kNone);
    // Loose credentials for domain2
    EXPECT_EQ(WriteFile("1", "./dom/domain2/cert.pem").mCode, ErrorCode::kNone);
    EXPECT_EQ(WriteFile("1", "./dom/domain2/priv.pem").mCode, ErrorCode::kNone);
    // Loose credentials for domain 3
    EXPECT_EQ(WriteFile("1", "./dom/domain3/cert.pem").mCode, ErrorCode::kNone);
    EXPECT_EQ(WriteFile("1", "./dom/domain3/ca.pem").mCode, ErrorCode::kNone);

    TestContext ctx;
    SetInitialExpectations(ctx);

    using namespace ot::commissioner::persistent_storage;
    // Prepare test data
    domain_id        did;
    network_id       nid;
    border_router_id rid;

    ASSERT_EQ(ctx.mPS->Add(domain{0, "domain1"}, did), PersistentStorage::Status::PS_SUCCESS);
    ASSERT_EQ(ctx.mPS->Add(domain{0, "domain2"}, did), PersistentStorage::Status::PS_SUCCESS);
    ASSERT_EQ(ctx.mPS->Add(domain{0, "domain3"}, did), PersistentStorage::Status::PS_SUCCESS);

    ASSERT_EQ(ctx.mPS->Add(network{0, 0, "pan1", 1, 1, "1", "", 1}, nid), PersistentStorage::Status::PS_SUCCESS);
    ASSERT_EQ(ctx.mPS->Add(network{0, 1, "pan2", 2, 1, "1", "", 1}, nid), PersistentStorage::Status::PS_SUCCESS);
    ASSERT_EQ(ctx.mPS->Add(network{0, 2, "pan3", 3, 1, "1", "", 1}, nid), PersistentStorage::Status::PS_SUCCESS);

    ASSERT_EQ(ctx.mPS->Add(border_router{0, 0,
                                         BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1",
                                                     BorderAgent::State{0, 0, 0, 0, 0}, "", 0, "", "",
                                                     Timestamp{0, 0, 0}, 0, "", ByteArray{}, "", 0, 0, "", 0, 0x0F}},
                           rid),
              PersistentStorage::Status::PS_SUCCESS);
    ASSERT_EQ(ctx.mPS->Add(border_router{0, 1,
                                         BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1",
                                                     BorderAgent::State{0, 0, 0, 0, 0}, "", 0, "", "",
                                                     Timestamp{0, 0, 0}, 0, "", ByteArray{}, "", 0, 0, "", 0, 0x0F}},
                           rid),
              PersistentStorage::Status::PS_SUCCESS);
    ASSERT_EQ(ctx.mPS->Add(border_router{0, 2,
                                         BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1",
                                                     BorderAgent::State{0, 0, 0, 0, 0}, "", 0, "", "",
                                                     Timestamp{0, 0, 0}, 0, "", ByteArray{}, "", 0, 0, "", 0, 0x0F}},
                           rid),
              PersistentStorage::Status::PS_SUCCESS);

    Init(ctx, ".");

    uint8_t                camIdx                  = 0;
    CommissionerAppMockPtr commissionerAppMocks[3] = {CommissionerAppMockPtr{new CommissionerAppMock()},
                                                      CommissionerAppMockPtr{new CommissionerAppMock()},
                                                      CommissionerAppMockPtr{new CommissionerAppMock()}};

    ON_CALL(ctx.mCommissionerAppStaticExpecter, Create(_, _))
        .WillByDefault(
            DoAll(WithArg<0>([&](std::shared_ptr<CommissionerApp> &a) { a = commissionerAppMocks[camIdx++]; }),
                  Return(Error{})));

    EXPECT_EQ(ctx.mJobManager.PrepareJobs({"start"}, {0x1, 0x2, 0x3}, false).mCode, ErrorCode::kNone);
    EXPECT_EQ(camIdx, 0);
    EXPECT_EQ(ctx.mJobManager.mJobPool.size(), 0);
}

TEST_F(JobManagerTestSuite, BuildFinalResultString)
{
    TestContext ctx;
    SetInitialExpectations(ctx);

    // Formally set default PSKc
    ctx.mConf.mPSKc = {'1', '0'};

    using namespace ot::commissioner::persistent_storage;
    // Prepare test data
    network_id       nid;
    border_router_id rid;

    ASSERT_EQ(ctx.mPS->Add(network{0, 0, "pan1", 1, 1, "1", "", 0}, nid), PersistentStorage::Status::PS_SUCCESS);
    EXPECT_EQ(nid.id, 0);

    ASSERT_EQ(ctx.mPS->Add(border_router{0, nid,
                                         BorderAgent{"127.0.0.1", 20001, ByteArray{}, "1.1",
                                                     BorderAgent::State{0, 0, 0, 0, 0}, "", 0, "", "",
                                                     Timestamp{0, 0, 0}, 0, "", ByteArray{}, "", 0, 0, "", 0, 0x0F}},
                           rid),
              PersistentStorage::Status::PS_SUCCESS);
    EXPECT_EQ(rid.id, 0);

    ASSERT_EQ(ctx.mPS->Add(network{0, 0, "pan2", 2, 2, "2", "", 0}, nid), PersistentStorage::Status::PS_SUCCESS);
    EXPECT_EQ(nid.id, 1);

    ASSERT_EQ(ctx.mPS->Add(border_router{0, nid,
                                         BorderAgent{"127.0.0.1", 20002, ByteArray{}, "1.1",
                                                     BorderAgent::State{0, 0, 0, 0, 0}, "", 0, "", "",
                                                     Timestamp{0, 0, 0}, 0, "", ByteArray{}, "", 0, 0, "", 0, 0x0F}},
                           rid),
              PersistentStorage::Status::PS_SUCCESS);
    EXPECT_EQ(rid.id, 1);

    ASSERT_EQ(ctx.mPS->Add(network{0, 0, "pan3", 3, 3, "3", "", 0}, nid), PersistentStorage::Status::PS_SUCCESS);
    EXPECT_EQ(nid.id, 2);

    ASSERT_EQ(ctx.mPS->Add(border_router{0, nid,
                                         BorderAgent{"127.0.0.1", 20003, ByteArray{}, "1.1",
                                                     BorderAgent::State{0, 0, 0, 0, 0}, "", 0, "", "",
                                                     Timestamp{0, 0, 0}, 0, "", ByteArray{}, "", 0, 0, "", 0, 0x0F}},
                           rid),
              PersistentStorage::Status::PS_SUCCESS);
    EXPECT_EQ(rid.id, 2);

    Init(ctx, ".");

    uint8_t                camIdx                  = 0;
    CommissionerAppMockPtr commissionerAppMocks[3] = {CommissionerAppMockPtr{new CommissionerAppMock()},
                                                      CommissionerAppMockPtr{new CommissionerAppMock()},
                                                      CommissionerAppMockPtr{new CommissionerAppMock()}};

    EXPECT_CALL(ctx.mCommissionerAppStaticExpecter, Create(_, _))
        .Times(3)
        .WillRepeatedly(
            DoAll(WithArg<0>([&](std::shared_ptr<CommissionerApp> &a) { a = commissionerAppMocks[camIdx++]; }),
                  Return(Error{})));
    EXPECT_CALL(*commissionerAppMocks[0], IsActive()).WillOnce(Return(false)).WillRepeatedly(Return(true));
    EXPECT_CALL(*commissionerAppMocks[1], IsActive()).WillOnce(Return(false)).WillRepeatedly(Return(true));
    EXPECT_CALL(*commissionerAppMocks[2], IsActive()).WillRepeatedly(Return(false));

    EXPECT_EQ(ctx.mJobManager.PrepareJobs({"start"}, {0x1, 0x02, 0x03}, false).mCode, ErrorCode::kNone);

    EXPECT_CALL(*commissionerAppMocks[0], Start(_, _, _)).Times(1).WillOnce(Return(Error{}));
    EXPECT_CALL(*commissionerAppMocks[1], Start(_, _, _)).Times(1).WillOnce(Return(Error{}));
    EXPECT_CALL(*commissionerAppMocks[2], Start(_, _, _))
        .Times(1)
        .WillOnce(Return(Error{ErrorCode::kAborted, "Aborted"}));

    ctx.mJobManager.RunJobs();

    Interpreter::Value value = ctx.mJobManager.CollectJobsValue();
    nlohmann::json     json  = nlohmann::json::parse(value.ToString());

    EXPECT_TRUE(json.contains(xpan_id{1}.str()));
    EXPECT_TRUE(json.contains(xpan_id{2}.str()));
    EXPECT_FALSE(json.contains(xpan_id{3}.str()));
    ctx.mJobManager.CleanupJobs();

    // "active" command
    EXPECT_EQ(ctx.mJobManager.PrepareJobs({"active"}, {0x1, 0x2, 0x3}, false).mCode, ErrorCode::kNone);
    ctx.mJobManager.RunJobs();
    value = ctx.mJobManager.CollectJobsValue();
    json  = nlohmann::json::parse(value.ToString());
    EXPECT_TRUE(json.contains(xpan_id{1}.str()));
    EXPECT_TRUE(json.contains(xpan_id{2}.str()));
    EXPECT_TRUE(json.contains(xpan_id{3}.str()));
    EXPECT_TRUE(json[xpan_id{1}.str()]);
    EXPECT_TRUE(json[xpan_id{2}.str()]);
    EXPECT_FALSE(json[xpan_id{3}.str()]);
    ctx.mJobManager.CleanupJobs();

    // "sessionid" command
    EXPECT_EQ(ctx.mJobManager.PrepareJobs({"sessionid"}, {0x1, 0x2}, false).mCode, ErrorCode::kNone);
    EXPECT_CALL(*commissionerAppMocks[0], GetSessionId(_))
        .WillOnce(DoAll(WithArg<0>([](uint16_t &a) { a = 0; }), Return(Error{})));
    EXPECT_CALL(*commissionerAppMocks[1], GetSessionId(_))
        .WillOnce(DoAll(WithArg<0>([](uint16_t &a) { a = 1; }), Return(Error{})));
    ctx.mJobManager.RunJobs();
    value = ctx.mJobManager.CollectJobsValue();
    json  = nlohmann::json::parse(value.ToString());
    EXPECT_TRUE(json.contains(xpan_id{1}.str()));
    EXPECT_TRUE(json.contains(xpan_id{2}.str()));
    EXPECT_EQ(json[xpan_id{1}.str()], 0);
    EXPECT_EQ(json[xpan_id{2}.str()], 1);
    ctx.mJobManager.CleanupJobs();

    // "opdataset get active" command
    EXPECT_EQ(ctx.mJobManager.PrepareJobs({"opdataset", "get", "active"}, {0x1, 0x2}, false).mCode, ErrorCode::kNone);
    EXPECT_CALL(*commissionerAppMocks[0], GetActiveDataset(_, _))
        .WillOnce(DoAll(WithArgs<0>([](ActiveOperationalDataset &ods) {
                            ods.mPanId        = 1;
                            ods.mPresentFlags = ActiveOperationalDataset::kPanIdBit;
                        }),
                        Return(Error{})));
    EXPECT_CALL(*commissionerAppMocks[1], GetActiveDataset(_, _))
        .WillOnce(DoAll(WithArgs<0>([](ActiveOperationalDataset &ods) {
                            ods.mPanId        = 2;
                            ods.mPresentFlags = ActiveOperationalDataset::kPanIdBit;
                        }),
                        Return(Error{})));
    ctx.mJobManager.RunJobs();
    value = ctx.mJobManager.CollectJobsValue();
    json  = nlohmann::json::parse(value.ToString());
    EXPECT_TRUE(json.contains(xpan_id{1}.str()));
    EXPECT_TRUE(json.contains(xpan_id{2}.str()));
    EXPECT_TRUE(json[xpan_id{1}.str()].contains("PanId"));
    EXPECT_TRUE(json[xpan_id{2}.str()].contains("PanId"));
    EXPECT_STREQ("0x0001", json[xpan_id{1}.str()]["PanId"].get<std::string>().c_str());
    EXPECT_STREQ("0x0002", json[xpan_id{2}.str()]["PanId"].get<std::string>().c_str());
    ctx.mJobManager.CleanupJobs();

    // "opdataset set securitypolicy" command
    SecurityPolicy policies[2];
    EXPECT_EQ(ctx.mJobManager.PrepareJobs({"opdataset", "set", "securitypolicy", "3", "AB"}, {0x1, 0x2}, false).mCode,
              ErrorCode::kNone);
    EXPECT_CALL(*commissionerAppMocks[0], SetSecurityPolicy(_))
        .WillOnce(DoAll(WithArgs<0>([&policies](const SecurityPolicy &pol) { policies[0] = pol; }), Return(Error{})));
    EXPECT_CALL(*commissionerAppMocks[1], SetSecurityPolicy(_))
        .WillOnce(DoAll(WithArgs<0>([&policies](const SecurityPolicy &pol) { policies[1] = pol; }), Return(Error{})));
    ctx.mJobManager.RunJobs();
    value = ctx.mJobManager.CollectJobsValue();
    json  = nlohmann::json::parse(value.ToString());
    EXPECT_TRUE(json.contains(xpan_id{1}.str()));
    EXPECT_TRUE(json.contains(xpan_id{2}.str()));
    EXPECT_TRUE(json[xpan_id{1}.str()]);
    EXPECT_TRUE(json[xpan_id{2}.str()]);
    ctx.mJobManager.CleanupJobs();

    // "stop" command
    EXPECT_EQ(ctx.mJobManager.PrepareJobs({"stop"}, {0x1, 0x2}, false).mCode, ErrorCode::kNone);
    EXPECT_CALL(*commissionerAppMocks[0], Stop());
    EXPECT_CALL(*commissionerAppMocks[1], Stop());
    ctx.mJobManager.RunJobs();
    value = ctx.mJobManager.CollectJobsValue();
    json  = nlohmann::json::parse(value.ToString());
    EXPECT_TRUE(json.contains(xpan_id{1}.str()));
    EXPECT_TRUE(json.contains(xpan_id{2}.str()));
    EXPECT_TRUE(json[xpan_id{1}.str()]);
    EXPECT_TRUE(json[xpan_id{2}.str()]);
    ctx.mJobManager.CleanupJobs();
}
