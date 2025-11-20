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
 *   The file defines the interface of a Thread Commissioner.
 */

#ifndef OT_COMM_COMMISSIONER_HPP_
#define OT_COMM_COMMISSIONER_HPP_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <commissioner/defines.hpp>
#include <commissioner/error.hpp>
#include <commissioner/network_data.hpp>
#include <commissioner/network_diag_data.hpp>

namespace ot {

namespace commissioner {

/**
 * @brief State of a commissioner.
 */
enum class State : uint8_t
{
    kDisabled = 0,
    kConnected,
    kPetitioning,
    kActive,
};

/**
 * @brief Logging Level.
 */
enum class LogLevel : uint8_t
{
    kOff = 0,
    kCritical,
    kError,
    kWarn,
    kInfo,
    kDebug
};

/**
 * @brief The Commissioner logger.
 *
 */
class Logger
{
public:
    virtual ~Logger() = default;

    /**
     * @brief The function write a single log message.
     *
     * @param[in] aLevel   A logging level.
     * @param[in] aRegion  A logging region.
     * @param[in] aMsg     A logging message.
     *
     */
    virtual void Log(LogLevel aLevel, const std::string &aRegion, const std::string &aMsg) = 0;
};

/**
 * @brief Configuration of a commissioner.
 */
struct Config
{
    bool mEnableCcm = true; ///< If enable CCM feature.

    // Allowed range: [30, 45] seconds.
    uint32_t mKeepAliveInterval = 40;  ///< The interval of keep-alive message. In seconds.
    uint32_t mMaxConnectionNum  = 100; ///< Max number of parallel connection from joiner.

    std::shared_ptr<Logger> mLogger;
    bool                    mEnableDtlsDebugLogging = false;

    // Mandatory for CCM Thread network.
    std::string mDomainName = "Thread"; ///< The domain name of connecting Thread network.

    // Maximum allowed length is 64 bytes.
    std::string mId = "OT-Commissioner"; ///< The readable commissioner ID.

    // Mandatory for non-CCM Thread network.
    ByteArray mPSKc; ///< The pre-shared commissioner key.

    // Mandatory for CCM Thread network.
    ByteArray mPrivateKey; ///< The private EC key.

    // Mandatory for CCM Thread network.
    ByteArray mCertificate; ///< The certificate signed by domain registrar.

    // Mandatory for CCM Thread network.
    ByteArray mTrustAnchor; ///< The trust anchor of 'mCertificate'.

    // Optional for CCM Thread network.
    ByteArray mCommissionerToken; ///< COM_TOK

    // Thread Security Materials Root path
    std::string mThreadSMRoot;
};

/**
 * @brief The base class defines Handlers of commissioner events.
 *
 * Application should inherit this class and override the virtual
 * functions to provide specific handler.
 *
 * @note Those handlers will be called in another threads and synchronization
 *       is needed if user data is accessed there.
 * @note No more than one handler will be called concurrently.
 * @note Keep the handlers simple and light, no heavy jobs or blocking operations
 *       (e.g. those synchronized APIs provided by the Commissioner) should be
 *       executed in those handlers.
 *
 */
class CommissionerHandler
{
public:
    /**
     * The function notifies the start of a joining request from given joiner.
     *
     * @param[in]  aJoinerId  A joiner ID.
     *
     * @return PSKd of the joiner. An empty PSKd indicates that the joiner is not
     *         enabled.
     *
     */
    virtual std::string OnJoinerRequest(const ByteArray &aJoinerId)
    {
        (void)aJoinerId;
        return "";
    }

    /**
     * This function notifies a joiner DTLS session is connected or not.
     *
     * @param[in] aJoinerId  The joiner ID.
     * @param[in] aError     The error during connecting; the joiner connection
     *                       is successfully established if error is Error::kNone;
     *                       otherwise, it failed to connect.
     *
     */
    virtual void OnJoinerConnected(const ByteArray &aJoinerId, Error aError)
    {
        (void)aJoinerId;
        (void)aError;
    }

    /**
     * This function notifies the receiving of JOIN_FIN.req message and asks for
     * vendor-specific provisioning if required.
     *
     * @param[in]  aJoinerId           The joiner ID.
     * @param[in]  aVendorName         A human-readable product vendor name string in utf-8 format.
     * @param[in]  aVendorModel        A human-readable product model string.
     * @param[in]  aVendorSwVersion    A utf-8 string that specifies the product software version.
     * @param[in]  aVendorStackVersion A vendor stack version of fixed length (5 bytes). See section
     *                                 8.10.3.6 of Thread spec for detail.
     * @param[in]  aProvisioningUrl    A URL encoded as a utf-8 string provided by the Joiner
     *                                 to communicate to the user which Commissioning application
     *                                 is best suited to properly provision it to the appropriate
     *                                 service. Empty if the joiner doesn't provide it.
     * @param[in]  aVendorData         A product vendor-defined data structure to guide
     *                                 vendor-specific provisioning. Empty if the joiner doesn't provide it.
     *
     * @return  A boolean indicates whether the joiner is accepted.
     *
     * @note This will be called when A well-formed JOIN_FIN.req has been received.
     *
     */
    virtual bool OnJoinerFinalize(const ByteArray   &aJoinerId,
                                  const std::string &aVendorName,
                                  const std::string &aVendorModel,
                                  const std::string &aVendorSwVersion,
                                  const ByteArray   &aVendorStackVersion,
                                  const std::string &aProvisioningUrl,
                                  const ByteArray   &aVendorData)
    {
        (void)aJoinerId;
        (void)aVendorName;
        (void)aVendorModel;
        (void)aVendorSwVersion;
        (void)aVendorStackVersion;
        (void)aProvisioningUrl;
        (void)aVendorData;
        return false;
    }

    /**
     * This funtions notifies the response of a keep-alive message.
     *
     * @param[in] aError  An error indicates whether the keep-alive message
     *                    was accepted by the leader.
     *
     */
    virtual void OnKeepAliveResponse(Error aError) { (void)aError; }

    /**
     * This function notifies the receiving a PAN ID conflict answer.
     *
     * @param[in] aPeerAddr     A peer address that sent the MGMT_PANID_CONFLICT.ans request.
     * @param[in] aChannelMask  A channel mask the peer scanned with.
     * @param[in] aPanId        The PAN ID that has a conflict.
     *
     */
    virtual void OnPanIdConflict(const std::string &aPeerAddr, const ChannelMask &aChannelMask, uint16_t aPanId)
    {
        (void)aPeerAddr;
        (void)aChannelMask;
        (void)aPanId;
    }

    /**
     * This function notifies the receiving of a energy scan report.
     *
     * @param[in] aPeerAddr     A peer address that sent the MGMT_PANID_CONFLICT.ans request.
     * @param[in] aChannelMask  A channel mask the peer scanned with.
     * @param[in] aEnergyList   A list of measured energy level in dBm.
     *
     */
    virtual void OnEnergyReport(const std::string &aPeerAddr,
                                const ChannelMask &aChannelMask,
                                const ByteArray   &aEnergyList)
    {
        (void)aPeerAddr;
        (void)aChannelMask;
        (void)aEnergyList;
    }

    /**
     * This function notifies the receiving the queried Diagnostic TLVs by DIAG_GET.ans command.
     *
     * @param[in] aPeerAddr     The destination address of the DIAG_GET.ans command.
     * @param[in] aDiagAnsMsg   Parsed network diag data.
     *
     */
    virtual void OnDiagGetAnswerMessage(const std::string &aPeerAddr, const ot::commissioner::NetDiagData &aDiagAnsMsg)
    {
        (void)aPeerAddr;
        (void)aDiagAnsMsg;
    }

    /**
     * This function notifies that the operational dataset has been changed.
     *
     * It is typical for the handler to request latest operational dataset by
     * calling GetActiveDataset and GetPendingDataset.
     *
     */
    virtual void OnDatasetChanged() {}

    virtual ~CommissionerHandler() = default;
};

/**
 * @brief The interface of a Thread commissioner.
 *
 */
class Commissioner
{
public:
    /**
     * The response handler of a general TMF request.
     *
     * @param[in] aError  An error code.
     *
     * @note Those handlers will be called in another threads and synchronization
     *       is needed if user data is accessed there.
     * @note No more than one handler will be called concurrently.
     * @note Keep the handlers simple and light, no heavy jobs or blocking operations
     *       (e.g. those synchronized APIs provided by the Commissioner) should be
     *       executed in those handlers.
     *
     */
    using ErrorHandler = std::function<void(Error aError)>;

    /**
     * The response handler of a general TMF request.
     *
     * @param[in] aResponseData  A response data. nullable.
     * @param[in] aError         An error code.
     *
     * @note @p aResponseData is guaranteed to be not null only when @p aError == Error::kNone.
     *       Otherwise, @p aResponseData should never be accessed.
     * @note Those handlers will be called in another threads and synchronization
     *       is needed if user data is accessed there.
     * @note No more than one handler will be called concurrently.
     * @note Keep the handlers simple and light, no heavy jobs or blocking operations
     *       (e.g. those synchronized APIs provided by the Commissioner) should be
     *       executed in those handlers.
     *
     */
    template <typename T> using Handler = std::function<void(const T *aResponseData, Error aError)>;

    /**
     * The petition result handler.
     *
     * @param[in] aExistingCommissionerId  The Existing commissioner Id. nullable.
     * @param[in] aError                   An error code.
     *
     * @note There is an exiting active commissioner if @p aError != Error::kNone
     *       and @p aExistingCommissionerId is not null.
     * @note Those handlers will be called in another threads and synchronization
     *       is needed if user data is accessed there.
     * @note No more than one handler will be called concurrently.
     * @note Keep the handlers simple and light, no heavy jobs or blocking operations
     *       (e.g. those synchronized APIs provided by the Commissioner) should be
     *       executed in those handlers.
     *
     */
    using PetitionHandler = std::function<void(const std::string *aExistingCommissionerId, Error aError)>;

    /**
     * @brief Create an instance of the commissioner.
     *
     * @param[in]  aHandler  A handler of commissioner events.
     *
     * @return A shared_ptr of the created Commissioner instance.
     *
     * @note Before being initialized with Commissioner::Init, the Commissioner
     *       instance has the default configuration created by Config().
     *
     */
    static std::shared_ptr<Commissioner> Create(CommissionerHandler &aHandler);

    virtual ~Commissioner() = default;

    /**
     * @brief Initialize with given configuration.
     *
     * @param[in]  aConfig  A Commissioner configuration.
     *
     * @retval Error::kNone  Successfully initialized the Commissioner.
     * @retval ...           Failed to initialize the Commissioner.
     *
     */
    virtual Error Init(const Config &aConfig) = 0;

    /**
     * @brief Get the configuration.
     *
     * @return The configuration.
     */
    virtual const Config &GetConfig() const = 0;

    /**
     * @brief Asynchronously connect to a Thread network.
     *
     * This method connects to a Thread network with specified border agent address and port.
     * It always returns immediately without waiting for the completion.
     *
     * @param[in, out] aHandler  A handler of any error during the connecting; Guaranteed to be
     *                           called.
     * @param[in]      aAddr     A border agent address.
     * @param[in]      aPort     A border agent port.
     *
     * @note This just try to connect to the border agent but not pertition to be a active
     *       commissioner. Call Petition() to petition.
     */
    virtual void Connect(ErrorHandler aHandler, const std::string &aAddr, uint16_t aPort) = 0;

    /**
     * @brief Synchronously connect to a Thread network.
     *
     * This method connects to a Thread network with specified border agent address and port.
     * It will not return until errors happened, timeouted or got connected.
     *
     * @param[in] aAddr  A border agent address.
     * @param[in] aPort  A border agent port.
     *
     * @return Error::kNone, succeed; otherwise, failed;
     */
    virtual Error Connect(const std::string &aAddr, uint16_t aPort) = 0;

    /**
     * @brief Disconnect from current Thread network.
     *
     */
    virtual void Disconnect() = 0;

    /**
     * @brief Get the Session Id.
     *
     * @return The session ID.
     *
     * @note The return value is meaningful only when this commissioner is active.
     */
    virtual uint16_t GetSessionId() const = 0;

    /**
     * @brief Get the commissioner state.
     *
     * @return the commissioner state.
     */
    virtual State GetState() const = 0;

    /**
     * @brief Decide if this commissioner is active.
     *
     * @return true
     * @return false
     */
    virtual bool IsActive() const = 0;

    /**
     * @brief Decide if the commissioner is running in CCM mode.
     *
     * @return true
     * @return false
     */
    virtual bool IsCcmMode() const = 0;

    /**
     * @brief Get the Domain Name.
     *
     * @return The domain name.
     *
     * @note The return value is meaningful only in CCM mode.
     */
    virtual const std::string &GetDomainName() const = 0;

    /**
     * @brief Cancel all outstanding requests.
     *
     */
    virtual void CancelRequests() = 0;

    /**
     * @brief Asynchronously petition to a Thread network.
     *
     * This method petition to a Thread network with specified border agent address and port,
     * by sending COMM_PET.req message.
     * If succeed, a keep-alive message will be periodically sent to keep itself active.
     * It always returns immediately without waiting for the completion.
     *
     * @param[in, out] aHandler A handler of any error during the petitioning; Guaranteed to be
     *                          called.
     * @param[in] aAddr         A border agent address.
     * @param[in] aPort         A border agent port.
     *
     * @note The commissioner will be first connected to the network if it is not.
     */
    virtual void Petition(PetitionHandler aHandler, const std::string &aAddr, uint16_t aPort) = 0;

    /**
     * @brief Synchronously petition to a Thread network.
     *
     * This method petitions to a Thread network with specified border agent address and port.
     * If succeed, a keep-alive message will be periodically sent to keep itself active.
     * It will not return until errors happened, timeouted or succeed.
     *
     * @param[out] aExistingCommissionerId  The existing active commissioner ID.
     * @param[in]  aAddr                    A border agent address.
     * @param[in]  aPort                    A border agent port.
     *
     * @return Error::kNone, succeed; otherwise, failed;
     *
     * @note @p aExistingCommissionerId is valid only when return value
     *       is not Error::kNone and itself is not empty. Otherwise,
     *       its content should never be accessed.
     *
     */
    virtual Error Petition(std::string &aExistingCommissionerId, const std::string &aAddr, uint16_t aPort) = 0;

    /**
     * @brief Asynchronously resign from the commissioner role.
     *
     * This method gracefully leave a Thread network by sending a
     * keep-alive message with the state TLV set to `Reject`. Eventually,
     * the connection will be closed.
     * It always returns immediately without waiting for the completion.
     *
     * @param[in, out] aHandler  A handler of any error during the petitioning; Guaranteed to be
     *                           called.
     */
    virtual void Resign(ErrorHandler aHandler) = 0;

    /**
     * @brief Synchronously resign from the commissioner role.
     *
     * This method leaves a Thread network by sending a keep-alive message with the state TLV set
     * to `Reject`. Eventually, the connection will be closed.
     *
     * @return Error::kNone, succeed; otherwise, failed;
     */
    virtual Error Resign() = 0;

    /**
     * @brief Asynchronously get the Commissioner Dataset.
     *
     * This method request Commissioner Dataset from the leader of the Thread network
     * by sending MGMT_COMMISSIONER_GET.req message.
     * It always returns immediately without waiting for the completion.
     *
     * @param[in, out] aHandler       A handler of the response and errors; Guaranteed to be called.
     * @param[in]      aDatasetFlags  Commissioner Dataset flags indicate which TLVs are wanted.
     */
    virtual void GetCommissionerDataset(Handler<CommissionerDataset> aHandler, uint16_t aDatasetFlags) = 0;

    /**
     * @brief Synchronously get the Commissioner Dataset.
     *
     * This method request Commissioner Dataset from the leader of the Thread network
     * by sending MGMT_COMMISSIONER_GET.req message.
     * It will not return until errors happened, timeouted or succeed.
     *
     * @param[out] aDataset       A Commissioner Dataset returned by the leader.
     * @param[in]  aDatasetFlags  Commissioner Dataset flags indicate which TLVs are wanted.
     *
     * @return Error::kNone, succeed; otherwise, failed;
     */
    virtual Error GetCommissionerDataset(CommissionerDataset &aDataset, uint16_t aDatasetFlags) = 0;

    /**
     * @brief Asynchronously set the Commissioner Dataset.
     *
     * This method set Commissioner Dataset of the Thread network
     * by sending MGMT_COMMISSIONER_SET.req message.
     * It always returns immediately without waiting for the completion.
     *
     * @param[in, out] aHandler  A handler of all errors; Guaranteed to be called.
     * @param[in]      aDataset  A Commissioner Dataset to be set.
     */
    virtual void SetCommissionerDataset(ErrorHandler aHandler, const CommissionerDataset &aDataset) = 0;

    /**
     * @brief Synchronously set the Commissioner Dataset.
     *
     * This method set Commissioner Dataset of the Thread network
     * by sending MGMT_COMMISSIONER_SET.req message.
     * It will not return until errors happened, timeouted or succeed.
     *
     * @param[in] aDataset  A Commissioner Dataset to be set.
     *
     * @return Error::kNone, succeed; otherwise, failed;
     */
    virtual Error SetCommissionerDataset(const CommissionerDataset &aDataset) = 0;

    /**
     * @brief Asynchronously set the Backbone Router Dataset.
     *
     * This method set Backbone Router Dataset of the primary backbone router
     * by sending MGMT_BBR_SET.req message.
     * It always returns immediately without waiting for the completion.
     *
     * @param[in, out] aHandler  A handler of all errors; Guaranteed to be called.
     * @param[in]      aDataset  A Backbone Router Dataset to be set.
     */
    virtual void SetBbrDataset(ErrorHandler aHandler, const BbrDataset &aDataset) = 0;

    /**
     * @brief Synchronously set the Backbone Router Dataset.
     *
     * This method set Backbone Router Dataset of the primary backbone router
     * by sending MGMT_BBR_SET.req message.
     * It will not return until errors happened, timeouted or succeed.
     *
     * @param[in] aDataset  A Backbone Router Dataset to be set.
     *
     * @return Error::kNone, succeed; otherwise, failed;
     */
    virtual Error SetBbrDataset(const BbrDataset &aDataset) = 0;

    /**
     * @brief Asynchronously get the Backbone Router Dataset.
     *
     * This method request Backbone Router Dataset of the primary backbone router
     * by sending MGMT_BBR_GET.req message.
     * It always returns immediately without waiting for the completion.
     *
     * @param[in, out] aHandler       A handler of the response and errors; Guaranteed to be called.
     * @param[in]      aDatasetFlags  Backbone Router Dataset flags indicate which TLVs are wanted.
     */
    virtual void GetBbrDataset(Handler<BbrDataset> aHandler, uint16_t aDatasetFlags) = 0;

    /**
     * @brief Synchronously get the Backbone Router Dataset.
     *
     * This method request Backbone Router Dataset from the leader of the primary backbone router
     * by sending MGMT_BBR_GET.req message.
     * It will not return until errors happened, timeouted or succeed.
     *
     * @param[out] aDataset       A Backbone Router Dataset returned by the leader.
     * @param[in]  aDatasetFlags  Backbone Router Dataset flags indicate which TLVs are wanted.
     *
     * @return Error::kNone, succeed; otherwise, failed;
     */
    virtual Error GetBbrDataset(BbrDataset &aDataset, uint16_t aDatasetFlags) = 0;

    /**
     * @brief Asynchronously get the Active Operational Dataset.
     *
     * This method request Active Operational Dataset of the Thread network
     * by sending MGMT_ACTIVE_GET.req message.
     * It always returns immediately without waiting for the completion.
     *
     * @param[in, out] aHandler       A handler of the response and errors; Guaranteed to be called.
     * @param[in]      aDatasetFlags  Active Operational Dataset flags indicate which TLVs are wanted.
     */
    virtual void GetActiveDataset(Handler<ActiveOperationalDataset> aHandler, uint16_t aDatasetFlags) = 0;

    /**
     * @brief Synchronously get the Active Operational Dataset.
     *
     * This method request Active Operational Dataset of the Thread network
     * by sending MGMT_ACTIVE_GET.req message.
     * It will not return until errors happened, timeouted or succeed.
     *
     * @param[out] aDataset       A Active Operational Dataset returned by the leader.
     * @param[in]  aDatasetFlags  Active Operational Dataset flags indicate which TLVs are wanted.
     *
     * @return Error::kNone, succeed; otherwise, failed;
     */
    virtual Error GetActiveDataset(ActiveOperationalDataset &aDataset, uint16_t aDatasetFlags) = 0;

    /**
     * @brief Asynchronously get the raw Active Operational Dataset as a binary blob (in format of Thread TLV).
     *
     * Get the uninterpreted Active Operational Dataset in MGMT_ACTIVE_GET.rsp.
     *
     * This method requests Active Operational Dataset of the Thread network
     * by sending MGMT_ACTIVE_GET.req message.
     * It always returns immediately without waiting for the completion.
     *
     * @param[in, out] aHandler       A handler of the response and errors; Guaranteed to be called.
     * @param[in]      aDatasetFlags  Active Operational Dataset flags indicating which TLVs are wanted.
     */
    virtual void GetRawActiveDataset(Handler<ByteArray> aHandler, uint16_t aDatasetFlags) = 0;

    /**
     * @brief Synchronously get the raw Active Operational Dataset as a binary blob (in format of Thread TLV).
     *
     * Get the uninterpreted Active Operational Dataset in MGMT_ACTIVE_GET.rsp.
     *
     * This method requests Active Operational Dataset of the Thread network
     * by sending MGMT_ACTIVE_GET.req message.
     * It will not return until errors happened, timeouted or succeed.
     *
     * @param[out] aDataset       A Active Operational Dataset returned by the leader.
     * @param[in]  aDatasetFlags  Active Operational Dataset flags indicate which TLVs are wanted.
     *
     * @return Error::kNone, succeed; otherwise, failed;
     */
    virtual Error GetRawActiveDataset(ByteArray &aRawDataset, uint16_t aDatasetFlags) = 0;

    /**
     * @brief Asynchronously set the Active Operational Dataset.
     *
     * This method set Active Operational Dataset of the Thread network
     * by sending MGMT_ACTIVE_SET.req message.
     * It always returns immediately without waiting for the completion.
     *
     * @param[in, out] aHandler  A handler of all errors; Guaranteed to be called.
     * @param[in]      aDataset  A Active Operational Dataset to be set.
     */
    virtual void SetActiveDataset(ErrorHandler aHandler, const ActiveOperationalDataset &aActiveDataset) = 0;

    /**
     * @brief Synchronously set the Active Operational Dataset.
     *
     * This method set Active Operational Dataset of the Thread network
     * by sending MGMT_ACTIVE_SET.req message.
     * It will not return until errors happened, timeouted or succeed.
     *
     * @param[in] aDataset  A Active Operational Dataset to be set.
     *
     * @return Error::kNone, succeed; otherwise, failed;
     */
    virtual Error SetActiveDataset(const ActiveOperationalDataset &aActiveDataset) = 0;

    /**
     * @brief Asynchronously get the Pending Operational Dataset.
     *
     * This method request Pending Operational Dataset of the Thread network
     * by sending MGMT_PENDING_GET.req message.
     * It always returns immediately without waiting for the completion.
     *
     * @param[in, out] aHandler       A handler of the response and errors; Guaranteed to be called.
     * @param[in]      aDatasetFlags  Pending Operational Dataset flags indicate which TLVs are
     *                                wanted.
     */
    virtual void GetPendingDataset(Handler<PendingOperationalDataset> aHandler, uint16_t aDatasetFlags) = 0;

    /**
     * @brief Synchronously get the Pending Operational Dataset.
     *
     * This method request Pending Operational Dataset of the Thread network
     * by sending MGMT_PENDING_GET.req message.
     * It will not return until errors happened, timeouted or succeed.
     *
     * @param[out] aDataset       A Pending Operational Dataset returned by the leader.
     * @param[in]  aDatasetFlags  Pending Operational Dataset flags indicate which TLVs are wanted.
     *
     * @return Error::kNone, succeed; otherwise, failed;
     */
    virtual Error GetPendingDataset(PendingOperationalDataset &aDataset, uint16_t aDatasetFlags) = 0;

    /**
     * @brief Asynchronously set the Pending Operational Dataset.
     *
     * This method set Pending Operational Dataset of the Thread network
     * by sending MGMT_PENDING_SET.req message.
     * It always returns immediately without waiting for the completion.
     *
     * @param[in, out] aHandler  A handler of all errors; Guaranteed to be called.
     * @param[in]      aDataset  A Pending Operational Dataset to be set.
     */
    virtual void SetPendingDataset(ErrorHandler aHandler, const PendingOperationalDataset &aPendingDataset) = 0;

    /**
     * @brief Synchronously set the Pending Operational Dataset.
     *
     * This method set Pending Operational Dataset of the Thread network
     * by sending MGMT_PENDING_SET.req message.
     * It will not return until errors happened, timeouted or succeed.
     *
     * @param[in] aDataset  A Pending Operational Dataset to be set.
     *
     * @return Error::kNone, succeed; otherwise, failed;
     */
    virtual Error SetPendingDataset(const PendingOperationalDataset &aPendingDataset) = 0;

    /**
     * @brief Asynchronously, securely set the Pending Operational Dataset.
     *
     * This method set Pending Operational Dataset which is to be securely disseminated
     * by sending MGMT_SEC_PENDING_SET.req message.
     * It always returns immediately without waiting for the completion.
     *
     * @param[in, out] aHandler            A handler of all errors; Guaranteed to be called.
     * @param[in]      aMaxRetrievalTimer  A Maximum Retrieval Timer.
     * @param[in]      aDataset            A Pending Operational Dataset to be set.
     */
    virtual void SetSecurePendingDataset(ErrorHandler                     aHandler,
                                         uint32_t                         aMaxRetrievalTimer,
                                         const PendingOperationalDataset &aDataset) = 0;

    /**
     * @brief Synchronously set the Pending Operational Dataset.
     *
     * This method set Pending Operational Dataset which is to be securely disseminated
     * by sending MGMT_SEC_PENDING_SET.req message.
     * It will not return until errors happened, timeouted or succeed.
     *
     * @param[in] aMaxRetrievalTimer  A Maximum Retrieval Timer.
     * @param[in] aDataset            A Pending Operational Dataset to be set.
     *
     * @return Error::kNone, succeed; otherwise, failed;
     */
    virtual Error SetSecurePendingDataset(uint32_t aMaxRetrievalTimer, const PendingOperationalDataset &aDataset) = 0;

    /**
     * @brief Asynchronously command a Thread device to reenroll.
     *
     * This method commands a Thread device to reenroll
     * by sending MGMT_REENROLL.req message to the device.
     * It always returns immediately without waiting for the completion.
     *
     * @param[in, out] aHandler  A handler of all errors; Guaranteed to be called.
     * @param[in]      aDstAddr  A destination device address.
     *
     * @note Even Error::kNone is returned, it's not guaranteed that the device has successfully
     *       reenrolled.
     */
    virtual void CommandReenroll(ErrorHandler aHandler, const std::string &aDstAddr) = 0;

    /**
     * @brief Synchronously command a Thread device to reenroll.
     *
     * This method commands a Thread device to reenroll
     * by sending MGMT_REENROLL.req message to the device.
     * It will not return until errors happened, timeouted or succeed.
     *
     * @param[in] aDstAddr  A destination device address.
     *
     * @return Error::kNone, the request & response has been successfully
     *         processed without guarantee that the device has successfully reenrolled;
     *         Otherwise, failed;
     */
    virtual Error CommandReenroll(const std::string &aDstAddr) = 0;

    /**
     * @brief Asynchronously command a Thread device to reset from current domain.
     *
     * This method commands a Thread device to reset
     * by sending MGMT_DOMAIN_RESET.req message to the device.
     * It always returns immediately without waiting for the completion.
     *
     * @param[in, out] aHandler  A handler of all errors; Guaranteed to be called.
     * @param[in]      aDstAddr  A destination device address.
     *
     * @note Even Error::kNone is returned, it's not guaranteed that the device has successfully
     *       reset.
     */
    virtual void CommandDomainReset(ErrorHandler aHandler, const std::string &aDstAddr) = 0;

    /**
     * @brief Synchronously command a Thread device to reset from current domain.
     *
     * This method commands a Thread device to reset
     * by sending MGMT_DOMAIN_RESET.req message to the device.
     * It will not return until errors happened, timeouted or succeed.
     *
     * @param[in] aDstAddr  A destination device address.
     *
     * @return Error::kNone, the request & response has been successfully
     *         processed without guarantee that the device has successfully reset;
     *         Otherwise, failed;
     */
    virtual Error CommandDomainReset(const std::string &aDstAddr) = 0;

    /**
     * @brief Asynchronously command a Thread device to migrate to another designated network.
     *
     * This method commands a Thread device to migrate to another designated network
     * by sending MGMT_NET_MIGRATE.req message to the device.
     * It always returns immediately without waiting for the completion.
     *
     * @param[in, out] aHandler            A handler of all errors; Guaranteed to be called.
     * @param[in]      aDstAddr            A destination device address.
     * @param[in]      aDesignatedNetwork  A designated network to migrate to.
     *
     * @note Even Error::kNone is returned, it's not guaranteed that the device has successfully
     *       migrated.
     */
    virtual void CommandMigrate(ErrorHandler       aHandler,
                                const std::string &aDstAddr,
                                const std::string &aDesignatedNetwork) = 0;

    /**
     * @brief Synchronously command a Thread device to migrate to another designated network.
     *
     * This method commands a Thread device to migrate to another designated network
     * by sending MGMT_NET_MIGRATE.req message to the device.
     * It will not return until errors happened, timeouted or succeed.
     *
     * @param[in] aDstAddr            A destination device address.
     * @param[in] aDesignatedNetwork  A designated network to migrate to.
     *
     * @return Error::kNone, the request & response has been successfully
     *         processed without guarantee that the device has successfully migrated;
     *         Otherwise, failed;
     */
    virtual Error CommandMigrate(const std::string &aDstAddr, const std::string &aDesignatedNetwork) = 0;

    /**
     * @brief Asynchronously initiate Operational Dataset Announcements.
     *
     * This method initiates Operational Dataset Announcements
     * by sending MGMT_ANNOUNCE_BEGIN.ntf message to one or more Thread devices.
     * It always returns immediately without waiting for the completion.
     *
     * @param[in, out] aHandler      A handler of all errors; Guaranteed to be called.
     * @param[in]      aChannelMask  The set of channels used to transmit MLE Announce messages.
     * @param[in]      aCount        The number of MLE Announce transmissions per channel.
     * @param[in]      aPeriod       A time period between successive MLE Announce transmissions;
     *                               in milliseconds;
     * @param[in]      aDstAddr      The destination address of the MGMT_ANNOUNCE_BEGIN.ntf message;
     *                               Can be either IPv6 unicast, IPv6 multicast, RLOC16 or ALOC16 string.
     *
     * @note If @p aDstAddr is a multicast address, this method won't wait for the ACK and it doesn't
     *       indicate that the message has been successfully handled by the Thread device even if the
     *       return value is Error::kNone.
     */
    virtual void AnnounceBegin(ErrorHandler       aHandler,
                               uint32_t           aChannelMask,
                               uint8_t            aCount,
                               uint16_t           aPeriod,
                               const std::string &aDstAddr) = 0;

    /**
     * @brief Synchronously initiate Operational Dataset Announcements.
     *
     * This method initiates Operational Dataset Announcements
     * by sending MGMT_ANNOUNCE_BEGIN.ntf message to one or more Thread devices.
     * It will not return until errors happened, timeouted or succeed.
     *
     * @param[in] aChannelMask  The set of channels used to transmit MLE Announce messages.
     * @param[in] aCount        The number of MLE Announce transmissions per channel.
     * @param[in] aPeriod       A time period between successive MLE Announce transmissions;
     *                          in milliseconds;
     * @param[in] aDstAddr      The destination address of the MGMT_ANNOUNCE_BEGIN.ntf message;
     *                          Can be either IPv6 unicast, IPv6 multicast, RLOC16 or ALOC16 string.
     *
     * @return Error::kNone: if @p aDstAddr is a multicast address, the message has been successfully
     *         sent; if @p aDstAddr is a unicast address, the message & ACK has been successfully
     *         processed; Otherwise; failed;
     */
    virtual Error AnnounceBegin(uint32_t           aChannelMask,
                                uint8_t            aCount,
                                uint16_t           aPeriod,
                                const std::string &aDstAddr) = 0;

    /**
     * @brief Asynchronously command a Thread device to detect PAN ID conflicts.
     *
     * This method requests one or more Thread devices to detect PAN ID conflicts
     * by sending MGMT_PANID_QUERY.qry message.
     * It always returns immediately without waiting for the completion.
     *
     * @param[in, out] aHandler      A handler of all errors; Guaranteed to be called.
     * @param[in]      aChannelMask  Specifies the ScanChannels and ChannelPage when
     *                               performing an IEEE 802.15.4 Active Scan.
     * @param[in]      aPanId        A PAN ID used to check for conflicts.
     * @param[in]      aDstAddr      The destination address of the MGMT_ANNOUNCE_BEGIN.ntf message;
     *                               Can be either IPv6 unicast, IPv6 multicast, RLOC16 or ALOC16 string.
     *
     * @note If @p aDstAddr is a multicast address, this method won't wait for the ACK and it doesn't
     *       indicate that the message has been successfully handled by the Thread device even if the
     *       return value is Error::kNone.
     *
     * @note A successful MGMT_PANID_QUERY.qry query will cause devices sending
     *       MGMT_PANID_CONFLICT.ans to the commissioner. And if set, the PanIdConflictHandler will
     *       be called.
     */
    virtual void PanIdQuery(ErrorHandler       aHandler,
                            uint32_t           aChannelMask,
                            uint16_t           aPanId,
                            const std::string &aDstAddr) = 0;

    /**
     * @brief Synchronously command a Thread device to detect PAN ID conflicts.
     *
     * This method requests one or more Thread devices to detect PAN ID conflicts
     * by sending MGMT_PANID_QUERY.qry message.
     * It will not return until errors happened, timeouted or succeed.
     *
     * @param[in] aChannelMask  Specifies the ScanChannels and ChannelPage when
     *                          performing an IEEE 802.15.4 Active Scan.
     * @param[in] aPanId        A PAN ID used to check for conflicts.
     * @param[in] aDstAddr      The destination address of the MGMT_PANID_QUERY.qry message;
     *                          Can be either IPv6 unicast, IPv6 multicast, RLOC16 or ALOC16 string.
     *
     * @return Error::kNone: if @p aDstAddr is a multicast address, the message has been successfully
     *         sent; if @p aDstAddr is a unicast address, the message & ACK has been successfully
     *         processed; Otherwise; failed;
     *
     * @note A successful MGMT_PANID_QUERY.qry query will cause devices sending
     *       MGMT_PANID_CONFLICT.ans to the commissioner. And if set, the PanIdConflictHandler will
     *       be called.
     */
    virtual Error PanIdQuery(uint32_t aChannelMask, uint16_t aPanId, const std::string &aDstAddr) = 0;

    /**
     * @brief Asynchronously command a Thread device to perform energy scanning.
     *
     * This method requests one or more Thread devices to perform energy scanning
     * by sending MGMT_ED_SCAN.qry message.
     * It always returns immediately without waiting for the completion.
     *
     * @param[in, out] aHandler       A handler of all errors; Guaranteed to be called.
     * @param[in]      aChannelMask   Specifies the ScanChannels and ChannelPage when
     *                                performing an IEEE 802.15.4 Active Scan.
     * @param[in]      aCount         The number of IEEE 802.15.4 ED Scans per channel.
     * @param[in]      aPeriod        The time period between successive IEEE 802.15.4 ED Scans;
     *                                in milliseconds.
     * @param[in]      aScanDuration  The IEEE 802.15.4 ScanDuration to use when performing an IEEE
     *                                802.15.4 ED Scan; In milliseconds.
     * @param[in]      aDstAddr       The destination address of the MGMT_ED_SCAN.qry message;
     *                                Can be either IPv6 unicast, IPv6 multicast, RLOC16 or ALOC16 string.
     *
     * @note If @p aDstAddr is a multicast address, this method won't wait for the ACK and it doesn't
     *       indicate that the message has been successfully handled by the Thread device even if the
     *       return value is Error::kNone.
     *
     * @note A successful MGMT_ED_SCAN.qry query will cause devices sending MGMT_ED_REPORT.ans to the
     *       commissioner. And if set, the EnergyReportHandler will be called.
     */
    virtual void EnergyScan(ErrorHandler       aHandler,
                            uint32_t           aChannelMask,
                            uint8_t            aCount,
                            uint16_t           aPeriod,
                            uint16_t           aScanDuration,
                            const std::string &aDstAddr) = 0;

    /**
     * @brief Synchronously command a Thread device to perform energy scanning.
     *
     * This method requests one or more Thread devices to perform energy scanning
     * by sending MGMT_ED_SCAN.qry message.
     * It will not return until errors happened, timeouted or succeed.
     *
     * @param[in] aChannelMask   Specifies the ScanChannels and ChannelPage when
     *                           performing an IEEE 802.15.4 Active Scan.
     * @param[in] aCount         The number of IEEE 802.15.4 ED Scans per channel.
     * @param[in] aPeriod        The time period between successive IEEE 802.15.4 ED Scans;
     *                           in milliseconds.
     * @param[in] aScanDuration  The IEEE 802.15.4 ScanDuration to use when performing an IEEE
     *                           802.15.4 ED Scan; In milliseconds.
     * @param[in] aDstAddr       The destination address of the MGMT_ED_SCAN.qry message;
     *                           Can be either IPv6 unicast, IPv6 multicast, RLOC16 or ALOC16 string.
     *
     * @return Error::kNone: if @p aDstAddr is a multicast address, the message has been successfully
     *         sent; if @p aDstAddr is a unicast address, the message & ACK has been successfully
     *         processed; Otherwise; failed;
     *
     * @note A successful MGMT_ED_SCAN.qry query will cause devices sending MGMT_ED_REPORT.ans to the
     *       commissioner. And if set, the EnergyReportHandler will be called.
     */
    virtual Error EnergyScan(uint32_t           aChannelMask,
                             uint8_t            aCount,
                             uint16_t           aPeriod,
                             uint16_t           aScanDuration,
                             const std::string &aDstAddr) = 0;

    /**
     * @brief Asynchronously register a multicast address for listening.
     *
     * This method commands a Thread device to register a multicast address for listening
     * by sending MLR.req message to the Primary Backbone Router.
     * It always returns immediately without waiting for the completion.
     *
     * @param[in, out] aHandler            A handler of all errors; Guaranteed to be called.
     * @param[in]      aMulticastAddrList  A list of multicast address to be registered.
     * @param[in]      aTimeout            A time period after which the registration as a
     *                                     listener to the included multicast group(s) expires;
     *                                     in seconds.
     */
    virtual void RegisterMulticastListener(Handler<uint8_t>                aHandler,
                                           const std::vector<std::string> &aMulticastAddrList,
                                           uint32_t                        aTimeout) = 0;

    /**
     * @brief Synchronously register a multicast address for listening.
     *
     * This method commands a Thread device to register a multicast address for listening
     * by sending MLR.req message to the Primary Backbone Router.
     * It will not return until errors happened, timeouted or succeed.
     *
     * @param[out] aStatus             The Status.
     * @param[in]  aMulticastAddrList  A list of multicast address to be registered.
     * @param[in]  aTimeout            A time period after which the registration as a
     *                                 listener to the included multicast group(s) expires; In seconds.
     * @return Error::kNone, succeed, the address has been successfully registered; Otherwise, failed;
     */
    virtual Error RegisterMulticastListener(uint8_t                        &aStatus,
                                            const std::vector<std::string> &aMulticastAddrList,
                                            uint32_t                        aTimeout) = 0;

    /**
     * @brief Asynchronously request Commissioner Token from domain registrar.
     *
     * This method requests Commissioner Token from domain registrar
     * by sending COMM_TOK.req message.
     * It always returns immediately without waiting for the completion.
     *
     * @param[in, out] aHandler  A handler of all errors; Guaranteed to be called.
     * @param[in]      aAddr     A registrar address.
     * @param[in]      aPort     A registrar port.
     *
     * @note The commissioner's local cache of the Token will be automatically
     *       overridded by the returned COSE-signed Token.
     */
    virtual void RequestToken(Handler<ByteArray> aHandler, const std::string &aAddr, uint16_t aPort) = 0;

    /**
     * @brief Synchronously request Commissioner Token from domain registrar.
     *
     * This method requests Commissioner Token from domain registrar
     * by sending COMM_TOK.req message.
     * It will not return until errors happened, timeouted or succeed.
     *
     * @param[in, out] aHandler  A handler of all errors; Guaranteed to be called.
     * @param[in] aAddr          A registrar address.
     * @param[in] aPort          A registrar port.
     *
     * @return Error::kNone, succeed; Otherwise, failed.
     *
     * @note The commissioner's local cache of the Token will be automatically
     *       overridded by the returned COSE-signed Token.
     */
    virtual Error RequestToken(ByteArray &aSignedToken, const std::string &aAddr, uint16_t aPort) = 0;

    /**
     * @brief Set the Commissioner Token.
     *
     * Sets the Commissioner Token after verifying it against the trust anchor if
     * `OT_COMM_CONFIG_REFERENCE_DEVICE_ENABLE` is disabled. Otherwise, always accept
     * the Commissioner Token.
     *
     * @param[in] aSignedToken  A COSE_SIGN1 signed Commissioner Token.
     *
     * @return Error::kNone, succeed; Otherwise, failed.
     */
    virtual Error SetToken(const ByteArray &aSignedToken) = 0;

    /**
     * @brief Generate PSKc by given passphrase, networkname and extended PAN ID.
     *
     * @param[out] aPSKc            A output PSKc.
     * @param[in]  aPassphrase      A thread defined commissioning password.
     * @param[in]  aNetworkName     A target networkname the PSKc will be used for.
     * @param[in]  aExtendedPanId   An extended PAN ID.
     *
     * @return Error::kNone, succeed; Otherwise, failed;
     */
    static Error GeneratePSKc(ByteArray         &aPSKc,
                              const std::string &aPassphrase,
                              const std::string &aNetworkName,
                              const ByteArray   &aExtendedPanId);

    /**
     * @brief Compute joiner ID with its IEEE EUI-64 value.
     *
     * @param[in] aEui64  A IEEE EUI-64 value of the joiner.
     *
     * @return The joiner ID.
     */
    static ByteArray ComputeJoinerId(uint64_t aEui64);

    /**
     * @brief Add the joiner to specific steering data with bloom filter.
     *
     * @param[in, out] aSteeringData  The steering data encoded to.
     * @param[in]      aJoinerId      A Joiner ID.
     */
    static void AddJoiner(ByteArray &aSteeringData, const ByteArray &aJoinerId);

    /**
     * @brief Return the commissioner version.
     *
     * @return A version string in format of <MAJOR>.<MINOR>.<PATCH>[+<GIT_REVISION>].
     *         The GIT_REVISION is included only when the commissioner is built in a
     *         git repository.
     */
    static std::string GetVersion(void);

    /**
     * @brief Asynchronously query diagnostic decoded data from a Thread device.
     *
     * This method sends a DIAG_GET.qry message to the specified Thread device,
     * requesting the set of diagnostic data indicated by `aDiagDataFlags`.
     * The ACK, or any errors encountered, will be delivered to the provided `aHandler`,
     * and the diag data will be obtained by the callback of OnDiagGetAnswerMessage.
     *
     * @param[in, out] aHandler        A handler to process the response or any errors.
     *                                 This handler is guaranteed to be called.
     * @param[in]      aAddr           Unicast mesh local address of the target Thread device,
     *                                 the leader ALOC will be set by default if it is empty.
     * @param[in]      aDiagDataFlags  Diagnostic data flags indicate which TLVs are wanted.
     *
     */
    virtual void CommandDiagGetQuery(ErrorHandler aHandler, const std::string &aAddr, uint64_t aDiagDataFlags) = 0;

    /**
     * @brief Synchronously query diagnostic decoded data from a Thread device.
     *
     * This method sends a DIAG_GET.qry message to the specified Thread device,
     * requesting the set of diagnostic data indicated by `aDiagDataFlags`.
     * The method blocks until a ack is received, an error occurs, and the diag data
     * will be obtained by the callback OnDiagGetAnswerMessage of CommissionerHandler.
     *
     * @param[in]  aAddr            Unicast mesh local address of the target Thread device,
     *                              the leader ALOC will be set by default if it is empty.
     * @param[in]  aDiagDataFlags   Diagnostic data flags indicate which TLVs are wanted.
     *
     * @return Error::kNone, succeed; Otherwise, failed.
     *
     */
    virtual Error CommandDiagGetQuery(const std::string &aAddr, uint64_t aDiagDataFlags) = 0;

    /**
     * @brief Asynchronously reset dedicated diagnostic TLV(s) on a Thread device.
     *
     * This method sends a DIAG_RST.ntf message to the specified Thread device,
     * resetting the diagnostic TLVs such as MacCounters indicated by `aDiagDataFlags`.
     * The response, or any errors encountered, will be delivered to the provided `aHandler`.
     *
     * @param[in, out] aHandler        A handler to process the response or any errors.
     *                                 This handler is guaranteed to be called.
     * @param[in]      aAddr           Unicast mesh local address of the target Thread device,
     *                                 the leader ALOC will be used if it is empty.
     * @param[in]      aDiagDataFlags  Diagnostic data flags indicate which TLVs are wanted.
     *
     */
    virtual void CommandDiagReset(ErrorHandler aHandler, const std::string &aAddr, uint64_t aDiagDataFlags) = 0;

    /**
     * @brief Synchronously reset dedicated diagnostic TLVs on a Thread device.
     *
     * This method sends a DIAG_RST.ntf message to the specified Thread device,
     * resetting the diagnostic TLVs such as MacCounters indicated by `aDiagDataFlags`.
     * The method blocks until a response is received or an error occurs.
     *
     * @param[in]      aAddr          Unicast mesh local address of the target Thread device,
     *                                the leader ALOC will be used if it is empty.
     * @param[in]  aDiagDataFlags     Diagnostic data flags indicate which TLVs are wanted.
     *
     * @return Error::kNone, succeed; Otherwise, failed.
     *
     */
    virtual Error CommandDiagReset(const std::string &aAddr, uint64_t aDiagDataFlags) = 0;
};

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_COMMISSIONER_HPP_
