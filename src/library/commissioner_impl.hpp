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

#ifndef OT_COMM_LIBRARY_COMMISSIONER_IMPL_HPP_
#define OT_COMM_LIBRARY_COMMISSIONER_IMPL_HPP_

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "commissioner/commissioner.hpp"
#include "commissioner/defines.hpp"
#include "commissioner/error.hpp"
#include "commissioner/network_data.hpp"
#include "commissioner/network_diag_data.hpp"
#include "common/error_macros.hpp"
#include "common/time.hpp"
#include "event2/event.h"
#include "library/coap.hpp"
#include "library/coap_secure.hpp"
#include "library/joiner_session.hpp"
#include "library/timer.hpp"
#include "library/tlv.hpp"
#include "library/token_manager.hpp"
#include "library/udp_proxy.hpp"

namespace ot {

namespace commissioner {

// This is the implementation of the Commissioner interface with
// event-driven model. The commissioner is not running unless the
// the event loop has been started. Starting the event loop will
// block current thread and it's not safe to calling any API in
// a separate thread without synchronization. This requires the
// user of this class making everything event-driven, which is
// not feasible for general applications. A solution is to run
// the event loop in a background thread and synchronize each call
// of the API with the event loop thread (see commissioner_safe.hpp).
//
// This class provides the implementation of the core Thread
// Commissioner features without consideration of usability.
//
// Note: for requests, only async APIs are implemented.
//
class CommissionerImpl : public Commissioner
{
    friend class JoinerSession;

public:
    explicit CommissionerImpl(CommissionerHandler &aHandler, struct event_base *aEventBase);

    CommissionerImpl(const CommissionerImpl &aCommissioner)                  = delete;
    const CommissionerImpl &operator=(const CommissionerImpl &aCommissioner) = delete;

    Error Init(const Config &aConfig) override;

    ~CommissionerImpl() override = default;

    const Config &GetConfig() const override;

    uint16_t GetSessionId() const override;

    State GetState() const override;

    bool IsActive() const override;

    bool IsCcmMode() const override;

    const std::string &GetDomainName() const override;

    void CancelRequests() override;

    void  Connect(ErrorHandler aHandler, const std::string &aAddr, uint16_t aPort) override;
    Error Connect(const std::string &, uint16_t) override { return ERROR_UNIMPLEMENTED(""); }

    void Disconnect() override;

    void  Petition(PetitionHandler aHandler, const std::string &aAddr, uint16_t aPort) override;
    Error Petition(std::string &, const std::string &, uint16_t) override { return ERROR_UNIMPLEMENTED(""); }

    void  Resign(ErrorHandler aHandler) override;
    Error Resign() override { return ERROR_UNIMPLEMENTED(""); }

    void  GetCommissionerDataset(Handler<CommissionerDataset> aHandler, uint16_t aDatasetFlags) override;
    Error GetCommissionerDataset(CommissionerDataset &, uint16_t) override { return ERROR_UNIMPLEMENTED(""); }

    void  SetCommissionerDataset(ErrorHandler aHandler, const CommissionerDataset &aDataset) override;
    Error SetCommissionerDataset(const CommissionerDataset &) override { return ERROR_UNIMPLEMENTED(""); }

    void  SetBbrDataset(ErrorHandler aHandler, const BbrDataset &aDataset) override;
    Error SetBbrDataset(const BbrDataset &) override { return ERROR_UNIMPLEMENTED(""); }

    void  GetBbrDataset(Handler<BbrDataset> aHandler, uint16_t aDatasetFlags) override;
    Error GetBbrDataset(BbrDataset &, uint16_t) override { return ERROR_UNIMPLEMENTED(""); }

    void  GetActiveDataset(Handler<ActiveOperationalDataset> aHandler, uint16_t aDatasetFlags) override;
    Error GetActiveDataset(ActiveOperationalDataset &, uint16_t) override { return ERROR_UNIMPLEMENTED(""); }

    void  GetRawActiveDataset(Handler<ByteArray> aHandler, uint16_t aDatasetFlags) override;
    Error GetRawActiveDataset(ByteArray &, uint16_t) override { return ERROR_UNIMPLEMENTED(""); }

    void  SetActiveDataset(ErrorHandler aHandler, const ActiveOperationalDataset &aActiveDataset) override;
    Error SetActiveDataset(const ActiveOperationalDataset &) override { return ERROR_UNIMPLEMENTED(""); }

    void  GetPendingDataset(Handler<PendingOperationalDataset> aHandler, uint16_t aDatasetFlags) override;
    Error GetPendingDataset(PendingOperationalDataset &, uint16_t) override { return ERROR_UNIMPLEMENTED(""); }

    void  SetPendingDataset(ErrorHandler aHandler, const PendingOperationalDataset &aPendingDataset) override;
    Error SetPendingDataset(const PendingOperationalDataset &) override { return ERROR_UNIMPLEMENTED(""); }

    void  RegisterMulticastListener(Handler<uint8_t>                aHandler,
                                    const std::vector<std::string> &aMulticastAddrList,
                                    uint32_t                        aTimeout) override;
    Error RegisterMulticastListener(uint8_t &, const std::vector<std::string> &, uint32_t) override
    {
        return ERROR_UNIMPLEMENTED("");
    }

    void  AnnounceBegin(ErrorHandler       aHandler,
                        uint32_t           aChannelMask,
                        uint8_t            aCount,
                        uint16_t           aPeriod,
                        const std::string &aDstAddr) override;
    Error AnnounceBegin(uint32_t, uint8_t, uint16_t, const std::string &) override { return ERROR_UNIMPLEMENTED(""); }

    void  PanIdQuery(ErrorHandler       aHandler,
                     uint32_t           aChannelMask,
                     uint16_t           aPanId,
                     const std::string &aDstAddr) override;
    Error PanIdQuery(uint32_t, uint16_t, const std::string &) override { return ERROR_UNIMPLEMENTED(""); }

    void  EnergyScan(ErrorHandler       aHandler,
                     uint32_t           aChannelMask,
                     uint8_t            aCount,
                     uint16_t           aPeriod,
                     uint16_t           aScanDuration,
                     const std::string &aDstAddr) override;
    Error EnergyScan(uint32_t, uint8_t, uint16_t, uint16_t, const std::string &) override
    {
        return ERROR_UNIMPLEMENTED("");
    }

    // Commercial Commissioning features.

    void  SetSecurePendingDataset(ErrorHandler                     aHandler,
                                  uint32_t                         aMaxRetrievalTimer,
                                  const PendingOperationalDataset &aDataset) override;
    Error SetSecurePendingDataset(uint32_t, const PendingOperationalDataset &) override
    {
        return ERROR_UNIMPLEMENTED("");
    }

    void  CommandReenroll(ErrorHandler aHandler, const std::string &aDstAddr) override;
    Error CommandReenroll(const std::string &) override { return ERROR_UNIMPLEMENTED(""); }

    void  CommandDomainReset(ErrorHandler aHandler, const std::string &aDstAddr) override;
    Error CommandDomainReset(const std::string &) override { return ERROR_UNIMPLEMENTED(""); }

    void  CommandMigrate(ErrorHandler       aHandler,
                         const std::string &aDstAddr,
                         const std::string &aDstNetworkName) override;
    Error CommandMigrate(const std::string &, const std::string &) override { return ERROR_UNIMPLEMENTED(""); }

    void  RequestToken(Handler<ByteArray> aHandler, const std::string &aAddr, uint16_t aPort) override;
    Error RequestToken(ByteArray &, const std::string &, uint16_t) override { return ERROR_UNIMPLEMENTED(""); }

    void  CommandDiagGetQuery(ErrorHandler aHandler, const std::string &aAddr, uint64_t aDiagDataFlags) override;
    Error CommandDiagGetQuery(const std::string &, uint64_t) override { return ERROR_UNIMPLEMENTED(""); }
    void  CommandDiagGetQuery(ErrorHandler aHandler, uint16_t aPeerAloc16, uint64_t aDiagDataFlags) override;
    Error CommandDiagGetQuery(uint16_t, uint64_t) override { return ERROR_UNIMPLEMENTED(""); }
    void  CommandDiagReset(ErrorHandler aHandler, const std::string &aAddr, uint64_t aDiagDataFlags) override;
    Error CommandDiagReset(const std::string &, uint64_t) override { return ERROR_UNIMPLEMENTED(""); }

    Error SetToken(const ByteArray &aSignedToken) override;

    struct event_base *GetEventBase() { return mEventBase; }

private:
    using AsyncRequest = std::function<void()>;

    static Error ValidateConfig(const Config &aConfig);
    void         LoggingConfig();

    static Error CheckCoapResponseCode(const coap::Response &aResponse);
    static Error HandleStateResponse(const coap::Response *aResponse, Error aError, bool aStateTlvIsMandatory = true);

    static ByteArray GetActiveOperationalDatasetTlvs(uint16_t aDatasetFlags);
    static ByteArray GetPendingOperationalDatasetTlvs(uint16_t aDatasetFlags);

    static Error DecodeActiveOperationalDataset(ActiveOperationalDataset &aDataset, const ByteArray &aPayload);
    static Error DecodePendingOperationalDataset(PendingOperationalDataset &aDataset, const coap::Response &aResponse);
    static Error DecodeChannelMask(ChannelMask &aChannelMask, const ByteArray &aBuf);
    static Error EncodeActiveOperationalDataset(coap::Request &aRequest, const ActiveOperationalDataset &aDataset);
    static Error EncodePendingOperationalDataset(coap::Request &aRequest, const PendingOperationalDataset &aDataset);
    static Error EncodeChannelMask(ByteArray &aBuf, const ChannelMask &aChannelMask);

#if OT_COMM_CONFIG_CCM_ENABLE
    static Error     DecodeBbrDataset(BbrDataset &aDataset, const coap::Response &aResponse);
    static Error     EncodeBbrDataset(coap::Request &aRequest, const BbrDataset &aDataset);
    static ByteArray GetBbrDatasetTlvs(uint16_t aDatasetFlags);
#endif

    static Error     DecodeCommissionerDataset(CommissionerDataset &aDataset, const coap::Response &aResponse);
    static Error     EncodeCommissionerDataset(coap::Request &aRequest, const CommissionerDataset &aDataset);
    static ByteArray GetCommissionerDatasetTlvs(uint16_t aDatasetFlags);

    static ByteArray GetNetDiagTlvTypes(uint64_t aDiagDataFlags);

    void SendPetition(PetitionHandler aHandler);

    // Set @p aKeepAlive to false to resign the commissioner role.
    void SendKeepAlive(Timer &aTimer, bool aKeepAlive = true);

#if OT_COMM_CONFIG_CCM_ENABLE
    Error SignRequest(coap::Request &aRequest, tlv::Scope aScope = tlv::Scope::kMeshCoP, bool aAppendToken = true);
#endif

    Duration GetKeepAliveInterval() const
    {
        return std::chrono::seconds(mConfig.mKeepAliveInterval);
    };

    void SendProxyMessage(ErrorHandler aHandler, const std::string &aDstAddr, const std::string &aUriPath);

    void HandleDatasetChanged(const coap::Request &aRequest);
    void HandlePanIdConflict(const coap::Request &aRequest);
    void HandleEnergyReport(const coap::Request &aRequest);

    static Error MakeChannelMask(ByteArray &aBuf, uint32_t aChannelMask);

    void HandleRlyRx(const coap::Request &aRlyRx);

    void HandleJoinerSessionTimer(Timer &aTimer);

    void HandleDiagGetAnswer(const coap::Request &aRequest);

    bool IsActiveOrConnected() const
    {
        return (mState == State::kActive || mState == State::kConnected);
    }

private:
    State    mState;
    uint16_t mSessionId; ///< The Commissioner Session ID.

private:
    /*
     * Implementation data.
     */

    CommissionerHandler &mCommissionerHandler;
    struct event_base   *mEventBase;

    Config mConfig;

    Timer mKeepAliveTimer;

    coap::CoapSecure mBrClient;

    std::map<ByteArray, JoinerSession> mJoinerSessions;
    Timer                              mJoinerSessionTimer;

    coap::Resource mResourceUdpRx;
    coap::Resource mResourceRlyRx;

    ProxyClient mProxyClient;

#if OT_COMM_CONFIG_CCM_ENABLE
    TokenManager mTokenManager;
#endif

    coap::Resource mResourceDatasetChanged;
    coap::Resource mResourcePanIdConflict;
    coap::Resource mResourceEnergyReport;

    coap::Resource mResourceDiagAns;
    NetDiagData    mDiagAnsTlvs;
};

/*
 * CoAP message and tlv related utilities
 */
Error       AppendTlv(coap::Message &aMessage, const tlv::Tlv &aTlv);
Error       GetTlvSet(tlv::TlvSet &aTlvSet, const coap::Message &aMessage, tlv::Scope aScope = tlv::Scope::kMeshCoP);
tlv::TlvPtr GetTlv(tlv::Type aTlvType, const coap::Message &aMessage, tlv::Scope aScope = tlv::Scope::kMeshCoP);

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_LIBRARY_COMMISSIONER_IMPL_HPP_
