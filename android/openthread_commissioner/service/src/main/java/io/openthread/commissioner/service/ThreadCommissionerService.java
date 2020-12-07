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

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import java.util.concurrent.CompletableFuture;

public interface ThreadCommissionerService {

  // This method returns Credential of a given Thread Network.
  // If the Commissioner Service doesn't possess the Network
  // Credentials already, it will fetch the Credentials from
  // a Border Agent (assisting device) using the Thread
  // Commissioning protocol.
  //
  // The pskc is used to securely connect to the Border Agent device.
  // If no pskc is given, the user will be asked to input it.
  // The Network Credentials will be saved in the Commissioner
  // Service before returning.
  CompletableFuture<ThreadNetworkCredential> fetchThreadNetworkCredential(
      @NonNull BorderAgentInfo borderAgentInfo, @Nullable byte[] pskc);

  // This method returns Credential of a given Thread Network stored in the database on the phone.
  CompletableFuture<ThreadNetworkCredential> getThreadNetworkCredential(
      @NonNull BorderAgentInfo borderAgentInfo);

  // This method deletes Credential of a given Thread Network stored in the database on the phone.
  CompletableFuture<Void> deleteThreadNetworkCredential(@NonNull BorderAgentInfo borderAgentInfo);

  CompletableFuture<Void> commissionJoinerDevice(
      @NonNull BorderAgentInfo borderAgentInfo,
      @NonNull byte[] pskc,
      @NonNull JoinerDeviceInfo joinerDeviceInfo);
}
