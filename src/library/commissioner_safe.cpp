/*
 *    Copyright (c) 2019, The OpenThread Authors.
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

#include <future>

#include "library/coap.hpp"
#include "library/cose.hpp"
#include "library/logging.hpp"
#include "library/openthread/bloom_filter.hpp"
#include "library/uri.hpp"

namespace ot {

namespace commissioner {

std::shared_ptr<Commissioner> Commissioner::Create(const Config &aConfig, struct event_base *aEventBase)
{
    if (aEventBase == nullptr)
    {
        auto comm = std::make_shared<CommissionerSafe>();
        return (comm->Init(aConfig) == Error::kNone) ? comm : nullptr;
    }
    else
    {
        auto comm = std::make_shared<CommissionerImpl>(aEventBase);
        return (comm->Init(aConfig) == Error::kNone) ? comm : nullptr;
    }
}

CommissionerSafe::CommissionerSafe()
    : mImpl(mEventBase.Get())
    , mEventThread(nullptr)
{
}

Error CommissionerSafe::Init(const Config &aConfig)
{
    Error error = Error::kFailed;

    SuccessOrExit(error = mImpl.Init(aConfig));

    VerifyOrExit(mEventBase.Get() != nullptr);
    VerifyOrExit(evthread_use_pthreads() == 0);
    VerifyOrExit(evthread_make_base_notifiable(mEventBase.Get()) == 0);

    VerifyOrExit(event_assign(&mInvokeEvent, mEventBase.Get(), -1, EV_PERSIST, Invoke, this) == 0);
    VerifyOrExit(event_add(&mInvokeEvent, nullptr) == 0);
exit:
    return error;
}

CommissionerSafe::~CommissionerSafe()
{
    Stop();
}

const Config &CommissionerSafe::GetConfig() const
{
    // Config is read-only, no synchronization is needed.
    return mImpl.GetConfig();
}

Error CommissionerSafe::Start()
{
    Error error = Error::kNone;

    VerifyOrExit(mEventThread == nullptr, error = Error::kAlready);

    mEventThread = std::make_shared<std::thread>([this]() { IgnoreError(mImpl.Start()); });

exit:
    return error;
}

// Stop the commissioner running in background.
void CommissionerSafe::Stop()
{
    mImpl.Stop();

    if (mEventThread && mEventThread->joinable())
    {
        mEventThread->join();
        mEventThread = nullptr;
    }
    return;
}

void CommissionerSafe::Connect(ErrorHandler aHandler, const std::string &aAddr, uint16_t aPort)
{
    PushAsyncRequest([=]() { mImpl.Connect(aHandler, aAddr, aPort); });
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
    PushAsyncRequest([=]() { mImpl.Disconnect(); });
}

/**
 * FIXME(wgtdkp): this is not thread safe.
 */
uint16_t CommissionerSafe::GetSessionId() const
{
    return mImpl.GetSessionId();
}

State CommissionerSafe::GetState() const
{
    return mImpl.GetState();
}

bool CommissionerSafe::IsActive() const
{
    return mImpl.IsActive();
}

bool CommissionerSafe::IsCcmMode() const
{
    return mImpl.IsCcmMode();
}

const std::string &CommissionerSafe::GetDomainName() const
{
    return mImpl.GetDomainName();
}

void CommissionerSafe::AbortRequests()
{
    PushAsyncRequest([=]() { mImpl.AbortRequests(); });
}

void CommissionerSafe::Petition(PetitionHandler aHandler, const std::string &aAddr, uint16_t aPort)
{
    PushAsyncRequest([=]() { mImpl.Petition(aHandler, aAddr, aPort); });
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
    PushAsyncRequest([=]() { mImpl.Resign(aHandler); });
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
    PushAsyncRequest([=]() { mImpl.GetCommissionerDataset(aHandler, aDatasetFlags); });
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
    PushAsyncRequest([=]() { mImpl.SetCommissionerDataset(aHandler, aDataset); });
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
    PushAsyncRequest([=]() { mImpl.GetBbrDataset(aHandler, aDatasetFlags); });
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
    PushAsyncRequest([=]() { mImpl.SetBbrDataset(aHandler, aDataset); });
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
    PushAsyncRequest([=]() { mImpl.GetActiveDataset(aHandler, aDatasetFlags); });
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

void CommissionerSafe::SetActiveDataset(ErrorHandler aHandler, const ActiveOperationalDataset &aDataset)
{
    PushAsyncRequest([=]() { mImpl.SetActiveDataset(aHandler, aDataset); });
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
    PushAsyncRequest([=]() { mImpl.GetPendingDataset(aHandler, aDatasetFlags); });
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
    PushAsyncRequest([=]() { mImpl.SetPendingDataset(aHandler, aDataset); });
}

Error CommissionerSafe::SetPendingDataset(const PendingOperationalDataset &aDataset)
{
    std::promise<Error> pro;
    auto                wait = [&pro](Error aError) { pro.set_value(aError); };

    SetPendingDataset(wait, aDataset);
    return pro.get_future().get();
}

void CommissionerSafe::SetSecurePendingDataset(ErrorHandler                     aHandler,
                                               const std::string &              aPbbrAddr,
                                               uint32_t                         aMaxRetrievalTimer,
                                               const PendingOperationalDataset &aDataset)
{
    PushAsyncRequest([=]() { mImpl.SetSecurePendingDataset(aHandler, aPbbrAddr, aMaxRetrievalTimer, aDataset); });
}

Error CommissionerSafe::SetSecurePendingDataset(const std::string &              aPbbrAddr,
                                                uint32_t                         aMaxRetrievalTimer,
                                                const PendingOperationalDataset &aDataset)
{
    std::promise<Error> pro;
    auto                wait = [&pro](Error aError) { pro.set_value(aError); };

    SetSecurePendingDataset(wait, aPbbrAddr, aMaxRetrievalTimer, aDataset);
    return pro.get_future().get();
}

void CommissionerSafe::CommandReenroll(ErrorHandler aHandler, const std::string &aDstAddr)
{
    PushAsyncRequest([=]() { mImpl.CommandReenroll(aHandler, aDstAddr); });
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
    PushAsyncRequest([=]() { mImpl.CommandDomainReset(aHandler, aDstAddr); });
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
    PushAsyncRequest([=]() { mImpl.CommandMigrate(aHandler, aDstAddr, aDstNetworkName); });
}

Error CommissionerSafe::CommandMigrate(const std::string &aDstAddr, const std::string &aDesignatedNetwork)
{
    std::promise<Error> pro;
    auto                wait = [&pro](Error aError) { pro.set_value(aError); };

    CommandMigrate(wait, aDstAddr, aDesignatedNetwork);
    return pro.get_future().get();
}

void CommissionerSafe::RegisterMulticastListener(Handler<uint8_t>                aHandler,
                                                 const std::string &             aPbbrAddr,
                                                 const std::vector<std::string> &aMulticastAddrList,
                                                 uint32_t                        aTimeout)
{
    PushAsyncRequest([=]() { mImpl.RegisterMulticastListener(aHandler, aPbbrAddr, aMulticastAddrList, aTimeout); });
}

Error CommissionerSafe::RegisterMulticastListener(uint8_t &                       aStatus,
                                                  const std::string &             aPbbrAddr,
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

    RegisterMulticastListener(wait, aPbbrAddr, aMulticastAddrList, aTimeout);
    return pro.get_future().get();
}

void CommissionerSafe::AnnounceBegin(ErrorHandler       aHandler,
                                     uint32_t           aChannelMask,
                                     uint8_t            aCount,
                                     uint16_t           aPeriod,
                                     const std::string &aDstAddr)
{
    PushAsyncRequest([=]() { mImpl.AnnounceBegin(aHandler, aChannelMask, aCount, aPeriod, aDstAddr); });
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
    PushAsyncRequest([=]() { mImpl.PanIdQuery(aHandler, aChannelMask, aPanId, aDstAddr); });
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
    PushAsyncRequest([=]() { mImpl.EnergyScan(aHandler, aChannelMask, aCount, aPeriod, aScanDuration, aDstAddr); });
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
    PushAsyncRequest([=]() { mImpl.RequestToken(aHandler, aAddr, aPort); });
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

Error CommissionerSafe::SetToken(const ByteArray &aSignedToken, const ByteArray &aSignerCert)
{
    std::promise<Error> pro;
    PushAsyncRequest([&]() { pro.set_value(mImpl.SetToken(aSignedToken, aSignerCert)); });
    return pro.get_future().get();
}

// It is not safe to call this after starting the commissioner.
void CommissionerSafe::SetJoinerInfoRequester(JoinerInfoRequester aJoinerInfoRequester)
{
    mImpl.SetJoinerInfoRequester(aJoinerInfoRequester);
}

// It is not safe to call this after starting the commissioner.
void CommissionerSafe::SetCommissioningHandler(CommissioningHandler aCommissioningHandler)
{
    mImpl.SetCommissioningHandler(aCommissioningHandler);
}

// It is not safe to call this after starting the commissioner.
void CommissionerSafe::SetDatasetChangedHandler(ErrorHandler aHandler)
{
    mImpl.SetDatasetChangedHandler(aHandler);
}

// It is not safe to call this after starting the commissioner.
void CommissionerSafe::SetPanIdConflictHandler(PanIdConflictHandler aHandler)
{
    mImpl.SetPanIdConflictHandler(aHandler);
}

// It is not safe to call this after starting the commissioner.
void CommissionerSafe::SetEnergyReportHandler(EnergyReportHandler aHandler)
{
    mImpl.SetEnergyReportHandler(aHandler);
}

void CommissionerSafe::Invoke(evutil_socket_t, short, void *aContext)
{
    auto commissionerSafe = reinterpret_cast<CommissionerSafe *>(aContext);

    ASSERT(commissionerSafe != nullptr);

    if (auto asyncReq = commissionerSafe->PopAsyncRequest())
    {
        asyncReq();
    }
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
    if (mEventBase == nullptr)
    {
        throw std::bad_alloc();
    }
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
