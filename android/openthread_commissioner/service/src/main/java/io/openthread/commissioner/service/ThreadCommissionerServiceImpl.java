/*
 *    Copyright (c) 2020, The OpenThread Commissioner Authors.
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

package io.openthread.commissioner.service;

import android.os.ConditionVariable;
import android.util.Log;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import io.openthread.commissioner.ByteArray;
import io.openthread.commissioner.ChannelMask;
import io.openthread.commissioner.Commissioner;
import io.openthread.commissioner.CommissionerDataset;
import io.openthread.commissioner.CommissionerHandler;
import io.openthread.commissioner.Config;
import io.openthread.commissioner.Error;
import io.openthread.commissioner.ErrorCode;
import java.math.BigInteger;
import java.net.Inet6Address;
import java.net.InetAddress;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.CompletionException;
import java.util.concurrent.ExecutionException;

public class ThreadCommissionerServiceImpl extends CommissionerHandler
    implements ThreadCommissionerService {

  public interface IntermediateStateCallback {
    void onPetitioned();

    void onJoinerRequest(@NonNull byte[] joinerId);
  }

  private static final String TAG = ThreadCommissionerException.class.getSimpleName();

  private static final int SECONDS_WAIT_FOR_JOINER = 60;

  @Nullable private IntermediateStateCallback intermediateStateCallback;

  @NonNull private BorderAgentDatabase borderAgentDatabase = BorderAgentDatabase.getDatabase();

  @Nullable private JoinerDeviceInfo curJoinerInfo;
  private ConditionVariable curJoinerCommissioned = new ConditionVariable();

  public ThreadCommissionerServiceImpl(
      @Nullable IntermediateStateCallback intermediateStateCallback) {
    this.intermediateStateCallback = intermediateStateCallback;
  }

  @Override
  public CompletableFuture<Void> commissionJoinerDevice(
      @NonNull BorderAgentInfo borderAgentInfo,
      @NonNull byte[] pskc,
      @NonNull JoinerDeviceInfo joinerDeviceInfo) {
    CompletableFuture<Void> future =
        CompletableFuture.runAsync(
            () -> {
              try {
                doCommissionJoinerDevice(
                    borderAgentInfo, pskc, joinerDeviceInfo, SECONDS_WAIT_FOR_JOINER);
              } catch (ThreadCommissionerException e) {
                throw new CompletionException(e);
              }
            });
    return future;
  }

  @Override
  public CompletableFuture<ThreadNetworkCredential> fetchThreadNetworkCredential(
      @NonNull BorderAgentInfo borderAgentInfo, @Nullable byte[] pskc) {
    return getThreadNetworkCredential(borderAgentInfo)
        .thenApply(
            credential -> {
              if (credential != null) {
                return credential;
              }

              try {
                byte[] activeDataset = doFetchActiveDataset(borderAgentInfo, pskc);
                return new ThreadNetworkCredential(activeDataset);
              } catch (ThreadCommissionerException e) {
                throw new CompletionException(e);
              }
            });
  }

  @Override
  public CompletableFuture<ThreadNetworkCredential> getThreadNetworkCredential(
      @NonNull BorderAgentInfo borderAgentInfo) {
    return getBorderAgentRecord(borderAgentInfo)
        .thenApply(
            borderAgentRecord -> {
              if (borderAgentRecord == null
                  || borderAgentRecord.getActiveOperationalDataset() == null) {
                return null;
              }
              return new ThreadNetworkCredential(borderAgentRecord.getActiveOperationalDataset());
            });
  }

  CompletableFuture<BorderAgentRecord> getBorderAgentRecord(
      @NonNull BorderAgentInfo borderAgentInfo) {
    return borderAgentDatabase.getBorderAgent(borderAgentInfo.discriminator);
  }

  @Override
  public CompletableFuture<Void> deleteThreadNetworkCredential(
      @NonNull BorderAgentInfo borderAgentInfo) {
    return borderAgentDatabase.deleteBorderAgent(borderAgentInfo.discriminator);
  }

  // This method adds given Thread Network Credential into database on the phone.
  private CompletableFuture<Void> addThreadNetworkCredential(
      @NonNull BorderAgentInfo borderAgentInfo,
      @NonNull byte[] pskc,
      @Nullable ThreadNetworkCredential networkCredential) {
    BorderAgentRecord borderAgentRecord =
        new BorderAgentRecord(
            borderAgentInfo.discriminator,
            borderAgentInfo.networkName,
            borderAgentInfo.extendedPanId,
            pskc,
            networkCredential == null ? null : networkCredential.getEncoded());
    return borderAgentDatabase.insertBorderAgent(borderAgentRecord);
  }

  private void doCommissionJoinerDevice(
      @NonNull BorderAgentInfo borderAgentInfo,
      @NonNull byte[] pskc,
      @NonNull JoinerDeviceInfo joinerDeviceInfo,
      int secondsWaitForJoiner)
      throws ThreadCommissionerException {
    curJoinerInfo = joinerDeviceInfo;

    Commissioner nativeCommissioner = Commissioner.create(this);

    Config config = new Config();
    config.setId("TestComm");
    config.setDomainName("TestDomain");
    config.setEnableCcm(false);
    config.setEnableDtlsDebugLogging(false);
    config.setPSKc(new ByteArray(pskc));
    config.setLogger(new NativeCommissionerLogger());

    try {
      // Initialize the native commissioner.
      throwIfFail(nativeCommissioner.init(config));

      // Petition to be the active commissioner in the Thread Network.
      String[] existingCommissionerId = new String[1];
      throwIfFail(
          nativeCommissioner.petition(
              existingCommissionerId, borderAgentInfo.host.getHostAddress(), borderAgentInfo.port));

      // Save PSKc for current Border Agent once we have successfully connected to it.
      addThreadNetworkCredential(borderAgentInfo, pskc, null).get();

      if (intermediateStateCallback != null) {
        intermediateStateCallback.onPetitioned();
      }

      ByteArray steeringData = new ByteArray();
      ByteArray joinerId = getCurJoinerId();
      Commissioner.addJoiner(steeringData, joinerId);

      CommissionerDataset commDataset = new CommissionerDataset();
      commDataset.setSteeringData(steeringData);
      commDataset.setPresentFlags(
          commDataset.getPresentFlags() | CommissionerDataset.kSteeringDataBit);
      throwIfFail(nativeCommissioner.setCommissionerDataset(commDataset));

      for (int i = 0; i < secondsWaitForJoiner; ++i) {
        if (curJoinerCommissioned.block(1000)) {
          return;
        }
      }

      throw new ThreadCommissionerException(ErrorCode.kTimeout, "Cannot find a Joiner Device!");
    } catch (InterruptedException e) {
      throw new ThreadCommissionerException(ErrorCode.kUnknown, e.getMessage());
    } catch (ExecutionException e) {
      throw new ThreadCommissionerException(ErrorCode.kUnknown, e.getMessage());
    } finally {
      nativeCommissioner.resign();
    }
  }

  private byte[] doFetchActiveDataset(
      @NonNull BorderAgentInfo borderAgentInfo, @NonNull byte[] pskc)
      throws ThreadCommissionerException {
    Commissioner nativeCommissioner = Commissioner.create(this);

    Config config = new Config();
    config.setId("TestComm");
    config.setDomainName("TestDomain");
    config.setEnableCcm(false);
    config.setEnableDtlsDebugLogging(true);
    config.setPSKc(new ByteArray(pskc));
    config.setLogger(new NativeCommissionerLogger());

    try {
      // Initialize the native commissioner
      throwIfFail(nativeCommissioner.init(config));

      // Petition to be the active commissioner in the Thread Network.
      String[] existingCommissionerId = new String[1];
      throwIfFail(
          nativeCommissioner.petition(
              existingCommissionerId,
              getBorderAgentAddress(borderAgentInfo),
              borderAgentInfo.port));

      addThreadNetworkCredential(borderAgentInfo, pskc, null).get();

      if (intermediateStateCallback != null) {
        intermediateStateCallback.onPetitioned();
      }

      // Fetch Active Operational Dataset.
      ByteArray rawActiveDataset = new ByteArray();
      throwIfFail(nativeCommissioner.getRawActiveDataset(rawActiveDataset, 0xFFFF));
      return CommissionerUtils.getByteArray(rawActiveDataset);
    } catch (InterruptedException e) {
      throw new ThreadCommissionerException(ErrorCode.kUnknown, e.getMessage());
    } catch (ExecutionException e) {
      throw new ThreadCommissionerException(ErrorCode.kUnknown, e.getMessage());
    } finally {
      nativeCommissioner.resign();
    }
  }

  private void throwIfFail(Error error) throws ThreadCommissionerException {
    if (error.getCode() != ErrorCode.kNone) {
      throw new ThreadCommissionerException(error.getCode(), error.getMessage());
    }
  }

  @Override
  public String onJoinerRequest(ByteArray joinerId) {
    Log.d(
        TAG,
        String.format(
            "A joiner (ID=%s) is requesting commissioning",
            CommissionerUtils.getHexString(joinerId)));

    if (intermediateStateCallback != null) {
      intermediateStateCallback.onJoinerRequest(CommissionerUtils.getByteArray(joinerId));
    }

    if (matchJoinerId(getCurJoinerId(), joinerId)) {
      return curJoinerInfo.getPskd();
    }
    return "";
  }

  @Override
  public void onJoinerConnected(ByteArray joinerId, Error error) {
    Log.d(TAG, "A joiner is connected");
  }

  @Override
  public boolean onJoinerFinalize(
      ByteArray joinerId,
      String vendorName,
      String vendorModel,
      String vendorSwVersion,
      ByteArray vendorStackVersion,
      String provisioningUrl,
      ByteArray vendorData) {
    Log.d(
        TAG,
        String.format("A joiner (ID=%s) is finalizing", CommissionerUtils.getHexString(joinerId)));

    if (matchJoinerId(getCurJoinerId(), joinerId)) {
      curJoinerCommissioned.open();
    }

    return true;
  }

  @Override
  public void onKeepAliveResponse(Error error) {
    Log.d(TAG, "received keep-alive response: " + error.toString());
  }

  @Override
  public void onPanIdConflict(String peerAddr, ChannelMask channelMask, int panId) {
    Log.d(TAG, "received PAN ID CONFLICT report");
  }

  @Override
  public void onEnergyReport(String aPeerAddr, ChannelMask aChannelMask, ByteArray aEnergyList) {
    Log.d(TAG, "received ENERGY SCAN report");
  }

  @Override
  public void onDatasetChanged() {
    Log.d(TAG, "Thread Network Dataset chanaged");
  }

  private ByteArray getCurJoinerId() {
    if (curJoinerInfo == null) {
      return null;
    }

    return Commissioner.computeJoinerId(new BigInteger(curJoinerInfo.getEui64()));
  }

  private boolean matchJoinerId(ByteArray curJoinerId, ByteArray inputJoinerId) {
    if (curJoinerId == null
        || inputJoinerId == null
        || curJoinerId.size() != inputJoinerId.size()) {
      return false;
    }
    for (int i = 0; i < curJoinerId.size(); ++i) {
      if (curJoinerId.get(i) != inputJoinerId.get(i)) {
        return false;
      }
    }
    return true;
  }

  private static String getBorderAgentAddress(BorderAgentInfo borderAgentInfo) {
    InetAddress addr = borderAgentInfo.host;

    String ret = addr.getHostAddress();
    if (addr instanceof Inet6Address) {
      Inet6Address ipv6Addr = (Inet6Address) addr;
      if (ipv6Addr.isLinkLocalAddress()) {
        // FIXME(wgtdkp): This is just a workaround. The Border Agent address returned
        //  by the NSDManager should already include scope ID (or interface name) for
        //  link-local IPv6 Address.
        ret += "%wlan0";
      }
    }

    return ret;
  }
}
