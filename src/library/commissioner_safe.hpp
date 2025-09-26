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
 *   The file defines the thread safe commissioner implementation.
 */

#ifndef OT_COMM_LIBRARY_COMMISSIONER_SAFE_HPP_
#define OT_COMM_LIBRARY_COMMISSIONER_SAFE_HPP_

#include <mutex>
#include <thread>

#include <commissioner/commissioner.hpp>

#include "library/coap.hpp"
#include "library/coap_secure.hpp"
#include "library/commissioner_impl.hpp"
#include "library/dtls.hpp"
#include "library/event.hpp"
#include "library/timer.hpp"
#include "library/tlv.hpp"
#include "library/token_manager.hpp"

namespace ot {

namespace commissioner {

/**
 * This class implements the Commissioner interface.
 *
 * It is based on the event-driven implementation and runs the
 * event loop in a background thread. Accesses to the event-driven
 * implementation are synchronized between user thread and the
 * event-loop thread which means the user can safely call a
 * Commissioner API from a user thread. But it is not safe to
 * concurrently call a Commissioner API from multiple user threads.
 *
 */
class CommissionerSafe : public Commissioner
{
public:
    CommissionerSafe(CommissionerHandler &aHandler)
        : mHandler(aHandler)
    {
    }

    CommissionerSafe(const CommissionerSafe &aCommissioner)                  = delete;
    const CommissionerSafe &operator=(const CommissionerSafe &aCommissioner) = delete;

    Error Init(const Config &aConfig) override;

    ~CommissionerSafe() override;

    const Config &GetConfig() const override;

    uint16_t GetSessionId() const override;

    State GetState() const override;

    bool IsActive() const override;

    bool IsCcmMode() const override;

    const std::string &GetDomainName() const override;

    void CancelRequests() override;

    void  Connect(ErrorHandler aHandler, const std::string &aAddr, uint16_t aPort) override;
    Error Connect(const std::string &aAddr, uint16_t aPort) override;

    void Disconnect() override;

    void  Petition(PetitionHandler aHandler, const std::string &aAddr, uint16_t aPort) override;
    Error Petition(std::string &aExistingCommissionerId, const std::string &aAddr, uint16_t aPort) override;

    void  Resign(ErrorHandler aHandler) override;
    Error Resign() override;

    void  GetCommissionerDataset(Handler<CommissionerDataset> aHandler, uint16_t aDatasetFlags) override;
    Error GetCommissionerDataset(CommissionerDataset &aDataset, uint16_t aDatasetFlags) override;

    void  SetCommissionerDataset(ErrorHandler aHandler, const CommissionerDataset &aDataset) override;
    Error SetCommissionerDataset(const CommissionerDataset &aDataset) override;

    void  SetBbrDataset(ErrorHandler aHandler, const BbrDataset &aDataset) override;
    Error SetBbrDataset(const BbrDataset &aDataset) override;

    void  GetBbrDataset(Handler<BbrDataset> aHandler, uint16_t aDatasetFlags) override;
    Error GetBbrDataset(BbrDataset &aDataset, uint16_t aDatasetFlags) override;

    void  GetActiveDataset(Handler<ActiveOperationalDataset> aHandler, uint16_t aDatasetFlags) override;
    Error GetActiveDataset(ActiveOperationalDataset &aDataset, uint16_t aDatasetFlags) override;

    void  GetRawActiveDataset(Handler<ByteArray> aHandler, uint16_t aDatasetFlags) override;
    Error GetRawActiveDataset(ByteArray &aRawDataset, uint16_t aDatasetFlags) override;

    void  SetActiveDataset(ErrorHandler aHandler, const ActiveOperationalDataset &aDataset) override;
    Error SetActiveDataset(const ActiveOperationalDataset &aDataset) override;

    void  GetPendingDataset(Handler<PendingOperationalDataset> aHandler, uint16_t aDatasetFlags) override;
    Error GetPendingDataset(PendingOperationalDataset &aDataset, uint16_t aDatasetFlags) override;

    void  SetPendingDataset(ErrorHandler aHandler, const PendingOperationalDataset &aDataset) override;
    Error SetPendingDataset(const PendingOperationalDataset &aDataset) override;

    void  SetSecurePendingDataset(ErrorHandler                     aHandler,
                                  uint32_t                         aMaxRetrievalTimer,
                                  const PendingOperationalDataset &aDataset) override;
    Error SetSecurePendingDataset(uint32_t aMaxRetrievalTimer, const PendingOperationalDataset &aDataset) override;

    void  CommandReenroll(ErrorHandler aHandler, const std::string &aDstAddr) override;
    Error CommandReenroll(const std::string &aDstAddr) override;

    void  CommandDomainReset(ErrorHandler aHandler, const std::string &aDstAddr) override;
    Error CommandDomainReset(const std::string &aDstAddr) override;

    void  CommandDiagGetQuery(ErrorHandler aHandler, const std::string &aAddr, uint64_t aDiagDataFlags) override;
    void  CommandDiagGetQuery(ErrorHandler aHandler, uint16_t aPeerAloc16, uint64_t aDiagDataFlags) override;
    Error CommandDiagGetQuery(uint16_t aPeerAloc16, uint64_t aDiagDataFlags) override;
    Error CommandDiagGetQuery(const std::string &aAddr, uint64_t aDiagDataFlags) override;
    void  CommandDiagReset(ErrorHandler aHandler, const std::string &aAddr, uint64_t aDiagDataFlags) override;
    Error CommandDiagReset(const std::string &aAddr, uint64_t aDiagDataFlags) override;

    void  CommandMigrate(ErrorHandler       aHandler,
                         const std::string &aDstAddr,
                         const std::string &aDstNetworkName) override;
    Error CommandMigrate(const std::string &aDstAddr, const std::string &aDesignatedNetwork) override;

    void  RegisterMulticastListener(Handler<uint8_t>                aHandler,
                                    const std::vector<std::string> &aMulticastAddrList,
                                    uint32_t                        aTimeout) override;
    Error RegisterMulticastListener(uint8_t                        &aStatus,
                                    const std::vector<std::string> &aMulticastAddrList,
                                    uint32_t                        aTimeout) override;

    void  AnnounceBegin(ErrorHandler       aHandler,
                        uint32_t           aChannelMask,
                        uint8_t            aCount,
                        uint16_t           aPeriod,
                        const std::string &aDstAddr) override;
    Error AnnounceBegin(uint32_t aChannelMask, uint8_t aCount, uint16_t aPeriod, const std::string &aDstAddr) override;

    void  PanIdQuery(ErrorHandler       aHandler,
                     uint32_t           aChannelMask,
                     uint16_t           aPanId,
                     const std::string &aDstAddr) override;
    Error PanIdQuery(uint32_t aChannelMask, uint16_t aPanId, const std::string &aDstAddr) override;

    void  EnergyScan(ErrorHandler       aHandler,
                     uint32_t           aChannelMask,
                     uint8_t            aCount,
                     uint16_t           aPeriod,
                     uint16_t           aScanDuration,
                     const std::string &aDstAddr) override;
    Error EnergyScan(uint32_t           aChannelMask,
                     uint8_t            aCount,
                     uint16_t           aPeriod,
                     uint16_t           aScanDuration,
                     const std::string &aDstAddr) override;

    void  RequestToken(Handler<ByteArray> aHandler, const std::string &aAddr, uint16_t aPort) override;
    Error RequestToken(ByteArray &aSignedToken, const std::string &aAddr, uint16_t aPort) override;

    Error SetToken(const ByteArray &aSignedToken) override;

private:
    using AsyncRequest = std::function<void()>;

    static void Invoke(evutil_socket_t aFd, short aFlags, void *aContext);

    void         PushAsyncRequest(AsyncRequest &&aAsyncRequest);
    AsyncRequest PopAsyncRequest();

    void StartEventLoopThread();
    void StopEventLoopThread();

private:
    class EventBaseHolder
    {
    public:
        EventBaseHolder();
        ~EventBaseHolder();
        struct event_base *Get();

    private:
        struct event_base *mEventBase;
    };

    CommissionerHandler &mHandler;

    // The EventBaseHolder needs to be the first member so that
    // it is constructed before any other members and destructed
    // after any other members.
    EventBaseHolder mEventBase;

    std::shared_ptr<CommissionerImpl> mImpl;

    // The event used to synchronize between the mEventThread
    // and user thread. It will be activated by user calls
    // and the callback will be run in mEventThread to do the
    // actual work.
    struct event mInvokeEvent;

    // Used for synchronization with event loop in background.
    std::mutex mInvokeMutex;

    // The schedule queue of all async requests.
    std::queue<AsyncRequest> mAsyncRequestQueue;

    // The even loop thread running in background.
    std::thread mEventThread;
};

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_LIBRARY_COMMISSIONER_SAFE_HPP_
