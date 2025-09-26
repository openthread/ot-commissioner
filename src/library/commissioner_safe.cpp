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
 *   The file implements the thread safe commissioner.
 */

#include "library/commissioner_safe.hpp"

#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "commissioner/commissioner.hpp"
#include "commissioner/defines.hpp"
#include "commissioner/error.hpp"
#include "commissioner/network_data.hpp"
#include "common/error_macros.hpp"
#include "common/logging.hpp"
#include "common/utils.hpp"
#include "event2/event.h"
#include "event2/thread.h"
#include "event2/util.h"
#include "library/commissioner_impl.hpp"

namespace ot {

namespace commissioner {

std::shared_ptr<Commissioner> Commissioner::Create(CommissionerHandler &aHandler)
{
    return std::make_shared<CommissionerSafe>(aHandler);
}

Error CommissionerSafe::Init(const Config &aConfig)
{
    Error                             error;
    std::shared_ptr<CommissionerImpl> impl;

    // The default timeout value (1 day) for non-IO events (events with fd < 0).
    constexpr struct timeval kDefaultNonIoEventTimeout = {3600 * 24, 0};

    VerifyOrExit(mEventBase.Get() != nullptr, error = ERROR_OUT_OF_MEMORY("failed to create event base"));

    error = ERROR_UNKNOWN("failed to initialize event base");
    VerifyOrExit(evthread_use_pthreads() == 0);
    VerifyOrExit(evthread_make_base_notifiable(mEventBase.Get()) == 0);
    VerifyOrExit(event_assign(&mInvokeEvent, mEventBase.Get(), -1, EV_PERSIST, Invoke, this) == 0);

    // We add the event with a timeout value so that the event loop will not
    // exit prematurely because of no events.
    VerifyOrExit(event_add(&mInvokeEvent, &kDefaultNonIoEventTimeout) == 0);

    impl = std::make_shared<CommissionerImpl>(mHandler, mEventBase.Get());
    SuccessOrExit(error = impl->Init(aConfig));
    mImpl = impl;

    StartEventLoopThread();

exit:
    return error;
}

CommissionerSafe::~CommissionerSafe()
{
    StopEventLoopThread();
}

const Config &CommissionerSafe::GetConfig() const
{
    // Config is read-only, no synchronization is needed.
    return mImpl->GetConfig();
}

void CommissionerSafe::StartEventLoopThread()
{
    ASSERT(!mEventThread.joinable());

    mEventThread = std::thread([this]() {
        LOG_INFO(LOG_REGION_MESHCOP, "event loop started in background thread");
        event_base_loop(mEventBase.Get(), 0);
    });
}

void CommissionerSafe::StopEventLoopThread(void)
{
    std::promise<void> pro;

    VerifyOrExit(mEventBase.Get() != nullptr && mImpl != nullptr);
    VerifyOrExit(mEventThread.joinable());

    // Send `Stop` to the event loop to break it from inside.
    // This makes sure the event loop has been started when we
    // trying to break it.
    PushAsyncRequest([&pro, this]() {
        event_base_loopbreak(mEventBase.Get());
        pro.set_value();
    });

    pro.get_future().wait();

    mEventThread.join();

exit:
    return;
}

void CommissionerSafe::Connect(ErrorHandler aHandler, const std::string &aAddr, uint16_t aPort)
{
    PushAsyncRequest([=]() { mImpl->Connect(aHandler, aAddr, aPort); });
}

Error CommissionerSafe::Connect(const std::string &aAddr, uint16_t aPort)
{
    std::promise<Error> pro;
    auto                wait = [&pro](Error aError) { pro.set_value(aError); };

    Connect(wait, aAddr, aPort);
    return pro.get_future().get();
}

void CommissionerSafe::Disconnect()
{
    PushAsyncRequest([=]() { mImpl->Disconnect(); });
}

/**
 * FIXME(wgtdkp): this is not thread safe.
 */
uint16_t CommissionerSafe::GetSessionId() const
{
    return mImpl->GetSessionId();
}

State CommissionerSafe::GetState() const
{
    return mImpl->GetState();
}

bool CommissionerSafe::IsActive() const
{
    return mImpl->IsActive();
}

bool CommissionerSafe::IsCcmMode() const
{
    return mImpl->IsCcmMode();
}

const std::string &CommissionerSafe::GetDomainName() const
{
    return mImpl->GetDomainName();
}

void CommissionerSafe::CancelRequests()
{
    PushAsyncRequest([=]() { mImpl->CancelRequests(); });
}

void CommissionerSafe::Petition(PetitionHandler aHandler, const std::string &aAddr, uint16_t aPort)
{
    PushAsyncRequest([=]() { mImpl->Petition(aHandler, aAddr, aPort); });
}

Error CommissionerSafe::Petition(std::string &aExistingCommissionerId, const std::string &aAddr, uint16_t aPort)
{
    std::promise<Error> pro;
    auto wait = [&pro, &aExistingCommissionerId](const std::string *existingCommissionerId, Error error) {
        if (existingCommissionerId != nullptr)
        {
            aExistingCommissionerId = *existingCommissionerId;
        }
        pro.set_value(error);
    };

    Petition(wait, aAddr, aPort);
    return pro.get_future().get();
}

void CommissionerSafe::Resign(ErrorHandler aHandler)
{
    PushAsyncRequest([=]() { mImpl->Resign(aHandler); });
}

Error CommissionerSafe::Resign()
{
    std::promise<Error> pro;
    auto                wait = [&pro](Error aError) { pro.set_value(aError); };

    Resign(wait);
    return pro.get_future().get();
}

void CommissionerSafe::GetCommissionerDataset(Handler<CommissionerDataset> aHandler, uint16_t aDatasetFlags)
{
    PushAsyncRequest([=]() { mImpl->GetCommissionerDataset(aHandler, aDatasetFlags); });
}

Error CommissionerSafe::GetCommissionerDataset(CommissionerDataset &aDataset, uint16_t aDatasetFlags)
{
    std::promise<Error> pro;
    auto                wait = [&pro, &aDataset](const CommissionerDataset *dataset, Error error) {
        if (dataset != nullptr)
        {
            aDataset = *dataset;
        }
        pro.set_value(error);
    };

    GetCommissionerDataset(wait, aDatasetFlags);
    return pro.get_future().get();
}

void CommissionerSafe::SetCommissionerDataset(ErrorHandler aHandler, const CommissionerDataset &aDataset)
{
    PushAsyncRequest([=]() { mImpl->SetCommissionerDataset(aHandler, aDataset); });
}

Error CommissionerSafe::SetCommissionerDataset(const CommissionerDataset &aDataset)
{
    std::promise<Error> pro;
    auto                wait = [&pro](Error aError) { pro.set_value(aError); };

    SetCommissionerDataset(wait, aDataset);
    return pro.get_future().get();
}

void CommissionerSafe::GetBbrDataset(Handler<BbrDataset> aHandler, uint16_t aDatasetFlags)
{
    PushAsyncRequest([=]() { mImpl->GetBbrDataset(aHandler, aDatasetFlags); });
}

Error CommissionerSafe::GetBbrDataset(BbrDataset &aDataset, uint16_t aDatasetFlags)
{
    std::promise<Error> pro;
    auto                wait = [&pro, &aDataset](const BbrDataset *dataset, Error error) {
        if (dataset != nullptr)
        {
            aDataset = *dataset;
        }
        pro.set_value(error);
    };

    GetBbrDataset(wait, aDatasetFlags);
    return pro.get_future().get();
}

void CommissionerSafe::SetBbrDataset(ErrorHandler aHandler, const BbrDataset &aDataset)
{
    PushAsyncRequest([=]() { mImpl->SetBbrDataset(aHandler, aDataset); });
}

Error CommissionerSafe::SetBbrDataset(const BbrDataset &aDataset)
{
    std::promise<Error> pro;
    auto                wait = [&pro](Error aError) { pro.set_value(aError); };

    SetBbrDataset(wait, aDataset);
    return pro.get_future().get();
}

void CommissionerSafe::GetActiveDataset(Handler<ActiveOperationalDataset> aHandler, uint16_t aDatasetFlags)
{
    PushAsyncRequest([=]() { mImpl->GetActiveDataset(aHandler, aDatasetFlags); });
}

Error CommissionerSafe::GetActiveDataset(ActiveOperationalDataset &aDataset, uint16_t aDatasetFlags)
{
    std::promise<Error> pro;
    auto                wait = [&pro, &aDataset](const ActiveOperationalDataset *dataset, Error error) {
        if (dataset != nullptr)
        {
            aDataset = *dataset;
        }
        pro.set_value(error);
    };

    GetActiveDataset(wait, aDatasetFlags);
    return pro.get_future().get();
}

void CommissionerSafe::GetRawActiveDataset(Handler<ByteArray> aHandler, uint16_t aDatasetFlags)
{
    PushAsyncRequest([=]() { mImpl->GetRawActiveDataset(aHandler, aDatasetFlags); });
}

Error CommissionerSafe::GetRawActiveDataset(ByteArray &aRawDataset, uint16_t aDatasetFlags)
{
    std::promise<Error> pro;
    auto                wait = [&pro, &aRawDataset](const ByteArray *rawDataset, Error error) {
        if (rawDataset != nullptr)
        {
            aRawDataset = *rawDataset;
        }
        pro.set_value(error);
    };

    GetRawActiveDataset(wait, aDatasetFlags);
    return pro.get_future().get();
}

void CommissionerSafe::SetActiveDataset(ErrorHandler aHandler, const ActiveOperationalDataset &aDataset)
{
    PushAsyncRequest([=]() { mImpl->SetActiveDataset(aHandler, aDataset); });
}

Error CommissionerSafe::SetActiveDataset(const ActiveOperationalDataset &aDataset)
{
    std::promise<Error> pro;
    auto                wait = [&pro](Error aError) { pro.set_value(aError); };

    SetActiveDataset(wait, aDataset);
    return pro.get_future().get();
}

void CommissionerSafe::GetPendingDataset(Handler<PendingOperationalDataset> aHandler, uint16_t aDatasetFlags)
{
    PushAsyncRequest([=]() { mImpl->GetPendingDataset(aHandler, aDatasetFlags); });
}

Error CommissionerSafe::GetPendingDataset(PendingOperationalDataset &aDataset, uint16_t aDatasetFlags)
{
    std::promise<Error> pro;
    auto                wait = [&pro, &aDataset](const PendingOperationalDataset *dataset, Error error) {
        if (dataset != nullptr)
        {
            aDataset = *dataset;
        }
        pro.set_value(error);
    };

    GetPendingDataset(wait, aDatasetFlags);
    return pro.get_future().get();
}

void CommissionerSafe::SetPendingDataset(ErrorHandler aHandler, const PendingOperationalDataset &aDataset)
{
    PushAsyncRequest([=]() { mImpl->SetPendingDataset(aHandler, aDataset); });
}

Error CommissionerSafe::SetPendingDataset(const PendingOperationalDataset &aDataset)
{
    std::promise<Error> pro;
    auto                wait = [&pro](Error aError) { pro.set_value(aError); };

    SetPendingDataset(wait, aDataset);
    return pro.get_future().get();
}

void CommissionerSafe::SetSecurePendingDataset(ErrorHandler                     aHandler,
                                               uint32_t                         aMaxRetrievalTimer,
                                               const PendingOperationalDataset &aDataset)
{
    PushAsyncRequest([=]() { mImpl->SetSecurePendingDataset(aHandler, aMaxRetrievalTimer, aDataset); });
}

Error CommissionerSafe::SetSecurePendingDataset(uint32_t aMaxRetrievalTimer, const PendingOperationalDataset &aDataset)
{
    std::promise<Error> pro;
    auto                wait = [&pro](Error aError) { pro.set_value(aError); };

    SetSecurePendingDataset(wait, aMaxRetrievalTimer, aDataset);
    return pro.get_future().get();
}

void CommissionerSafe::CommandReenroll(ErrorHandler aHandler, const std::string &aDstAddr)
{
    PushAsyncRequest([=]() { mImpl->CommandReenroll(aHandler, aDstAddr); });
}

Error CommissionerSafe::CommandReenroll(const std::string &aDstAddr)
{
    std::promise<Error> pro;
    auto                wait = [&pro](Error aError) { pro.set_value(aError); };

    CommandReenroll(wait, aDstAddr);
    return pro.get_future().get();
}

void CommissionerSafe::CommandDomainReset(ErrorHandler aHandler, const std::string &aDstAddr)
{
    PushAsyncRequest([=]() { mImpl->CommandDomainReset(aHandler, aDstAddr); });
}

Error CommissionerSafe::CommandDomainReset(const std::string &aDstAddr)
{
    std::promise<Error> pro;
    auto                wait = [&pro](Error aError) { pro.set_value(aError); };

    CommandDomainReset(wait, aDstAddr);
    return pro.get_future().get();
}

void CommissionerSafe::CommandMigrate(ErrorHandler       aHandler,
                                      const std::string &aDstAddr,
                                      const std::string &aDstNetworkName)
{
    PushAsyncRequest([=]() { mImpl->CommandMigrate(aHandler, aDstAddr, aDstNetworkName); });
}

Error CommissionerSafe::CommandMigrate(const std::string &aDstAddr, const std::string &aDesignatedNetwork)
{
    std::promise<Error> pro;
    auto                wait = [&pro](Error aError) { pro.set_value(aError); };

    CommandMigrate(wait, aDstAddr, aDesignatedNetwork);
    return pro.get_future().get();
}

void CommissionerSafe::RegisterMulticastListener(Handler<uint8_t>                aHandler,
                                                 const std::vector<std::string> &aMulticastAddrList,
                                                 uint32_t                        aTimeout)
{
    PushAsyncRequest([=]() { mImpl->RegisterMulticastListener(aHandler, aMulticastAddrList, aTimeout); });
}

Error CommissionerSafe::RegisterMulticastListener(uint8_t                        &aStatus,
                                                  const std::vector<std::string> &aMulticastAddrList,
                                                  uint32_t                        aTimeout)
{
    std::promise<Error> pro;
    auto                wait = [&pro, &aStatus](const uint8_t *status, Error error) {
        if (status != nullptr)
        {
            aStatus = *status;
        }
        pro.set_value(error);
    };

    RegisterMulticastListener(wait, aMulticastAddrList, aTimeout);
    return pro.get_future().get();
}

void CommissionerSafe::AnnounceBegin(ErrorHandler       aHandler,
                                     uint32_t           aChannelMask,
                                     uint8_t            aCount,
                                     uint16_t           aPeriod,
                                     const std::string &aDstAddr)
{
    PushAsyncRequest([=]() { mImpl->AnnounceBegin(aHandler, aChannelMask, aCount, aPeriod, aDstAddr); });
}
Error CommissionerSafe::AnnounceBegin(uint32_t           aChannelMask,
                                      uint8_t            aCount,
                                      uint16_t           aPeriod,
                                      const std::string &aDstAddr)
{
    std::promise<Error> pro;
    auto                wait = [&pro](Error error) { pro.set_value(error); };

    AnnounceBegin(wait, aChannelMask, aCount, aPeriod, aDstAddr);
    return pro.get_future().get();
}

void CommissionerSafe::PanIdQuery(ErrorHandler       aHandler,
                                  uint32_t           aChannelMask,
                                  uint16_t           aPanId,
                                  const std::string &aDstAddr)
{
    PushAsyncRequest([=]() { mImpl->PanIdQuery(aHandler, aChannelMask, aPanId, aDstAddr); });
}

Error CommissionerSafe::PanIdQuery(uint32_t aChannelMask, uint16_t aPanId, const std::string &aDstAddr)
{
    std::promise<Error> pro;
    auto                wait = [&pro](Error error) { pro.set_value(error); };

    PanIdQuery(wait, aChannelMask, aPanId, aDstAddr);
    return pro.get_future().get();
}

void CommissionerSafe::EnergyScan(ErrorHandler       aHandler,
                                  uint32_t           aChannelMask,
                                  uint8_t            aCount,
                                  uint16_t           aPeriod,
                                  uint16_t           aScanDuration,
                                  const std::string &aDstAddr)
{
    PushAsyncRequest([=]() { mImpl->EnergyScan(aHandler, aChannelMask, aCount, aPeriod, aScanDuration, aDstAddr); });
}

Error CommissionerSafe::EnergyScan(uint32_t           aChannelMask,
                                   uint8_t            aCount,
                                   uint16_t           aPeriod,
                                   uint16_t           aScanDuration,
                                   const std::string &aDstAddr)
{
    std::promise<Error> pro;
    auto                wait = [&pro](Error error) { pro.set_value(error); };

    EnergyScan(wait, aChannelMask, aCount, aPeriod, aScanDuration, aDstAddr);
    return pro.get_future().get();
}

void CommissionerSafe::RequestToken(Handler<ByteArray> aHandler, const std::string &aAddr, uint16_t aPort)
{
    PushAsyncRequest([=]() { mImpl->RequestToken(aHandler, aAddr, aPort); });
}

Error CommissionerSafe::RequestToken(ByteArray &aSignedToken, const std::string &aAddr, uint16_t aPort)
{
    std::promise<Error> pro;
    auto                wait = [&pro, &aSignedToken](const ByteArray *signedToken, Error error) {
        if (signedToken != nullptr)
        {
            aSignedToken = *signedToken;
        }
        pro.set_value(error);
    };

    RequestToken(wait, aAddr, aPort);
    return pro.get_future().get();
}

Error CommissionerSafe::SetToken(const ByteArray &aSignedToken)
{
    std::promise<Error> pro;
    PushAsyncRequest([&]() { pro.set_value(mImpl->SetToken(aSignedToken)); });
    return pro.get_future().get();
}

void CommissionerSafe::CommandDiagGetQuery(ErrorHandler aHandler, const std::string &aAddr, uint64_t aDiagDataFlags)
{
    PushAsyncRequest([=]() { mImpl->CommandDiagGetQuery(aHandler, aAddr, aDiagDataFlags); });
}

void CommissionerSafe::CommandDiagGetQuery(ErrorHandler aHandler, uint16_t aPeerAloc16, uint64_t aDiagDataFlags)
{
    PushAsyncRequest([=]() { mImpl->CommandDiagGetQuery(aHandler, aPeerAloc16, aDiagDataFlags); });
}


Error CommissionerSafe::CommandDiagGetQuery(uint16_t aPeerAloc16, uint64_t aDiagDataFlags)
{
    std::promise<Error> pro;
    auto                wait = [&pro](Error error) { pro.set_value(error); };
    CommandDiagGetQuery(wait, aPeerAloc16, aDiagDataFlags);
    return pro.get_future().get();
}

Error CommissionerSafe::CommandDiagGetQuery(const std::string &aAddr, uint64_t aDiagDataFlags)
{
    std::promise<Error> pro;
    auto                wait = [&pro](Error error) { pro.set_value(error); };
    CommandDiagGetQuery(wait, aAddr, aDiagDataFlags);
    return pro.get_future().get();
}

void CommissionerSafe::Invoke(evutil_socket_t, short, void *aContext)
{
    auto commissionerSafe = reinterpret_cast<CommissionerSafe *>(aContext);

    VerifyOrDie(commissionerSafe != nullptr);

    if (auto asyncReq = commissionerSafe->PopAsyncRequest())
    {
        asyncReq();
    }
}

void CommissionerSafe::CommandDiagReset(ErrorHandler aHandler, const std::string &aAddr, uint64_t aDiagDataFlags)
{
    PushAsyncRequest([=]() { mImpl->CommandDiagReset(aHandler, aAddr, aDiagDataFlags); });
}

Error CommissionerSafe::CommandDiagReset(const std::string &aAddr, uint64_t aDiagDataFlags)
{
    std::promise<Error> pro;
    auto                wait = [&pro](Error error) { pro.set_value(error); };
    CommandDiagReset(wait, aAddr, aDiagDataFlags);
    return pro.get_future().get();
}

void CommissionerSafe::PushAsyncRequest(AsyncRequest &&aAsyncRequest)
{
    std::lock_guard<std::mutex> _(mInvokeMutex);

    mAsyncRequestQueue.emplace(std::move(aAsyncRequest));

    // Notify for new async request
    event_active(&mInvokeEvent, 0, 0);
}

CommissionerSafe::AsyncRequest CommissionerSafe::PopAsyncRequest()
{
    std::lock_guard<std::mutex> _(mInvokeMutex);

    if (!mAsyncRequestQueue.empty())
    {
        auto ret = mAsyncRequestQueue.front();
        mAsyncRequestQueue.pop();
        return ret;
    }
    return nullptr;
}

CommissionerSafe::EventBaseHolder::EventBaseHolder()
    : mEventBase(event_base_new())
{
}

CommissionerSafe::EventBaseHolder::~EventBaseHolder()
{
    event_base_free(mEventBase);
}

struct event_base *CommissionerSafe::EventBaseHolder::Get()
{
    return mEventBase;
}

} // namespace commissioner

} // namespace ot
