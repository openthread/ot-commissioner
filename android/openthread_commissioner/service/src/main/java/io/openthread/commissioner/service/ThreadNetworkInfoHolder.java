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

import android.os.Parcel;
import android.os.Parcelable;
import androidx.annotation.NonNull;
import java.util.ArrayList;

public class ThreadNetworkInfoHolder implements Parcelable {
  @NonNull private final ThreadNetworkInfo networkInfo;

  @NonNull private final ArrayList<BorderAgentInfo> borderAgents;

  public ThreadNetworkInfoHolder(BorderAgentInfo borderAgent) {
    networkInfo = new ThreadNetworkInfo(borderAgent.networkName, borderAgent.extendedPanId);
    borderAgents = new ArrayList<>();
    borderAgents.add(borderAgent);
  }

  protected ThreadNetworkInfoHolder(Parcel in) {
    networkInfo = in.readParcelable(ThreadNetworkInfo.class.getClassLoader());
    borderAgents = in.createTypedArrayList(BorderAgentInfo.CREATOR);
  }

  public static final Creator<ThreadNetworkInfoHolder> CREATOR =
      new Creator<ThreadNetworkInfoHolder>() {
        @Override
        public ThreadNetworkInfoHolder createFromParcel(Parcel in) {
          return new ThreadNetworkInfoHolder(in);
        }

        @Override
        public ThreadNetworkInfoHolder[] newArray(int size) {
          return new ThreadNetworkInfoHolder[size];
        }
      };

  public ThreadNetworkInfo getNetworkInfo() {
    return networkInfo;
  }

  public ArrayList<BorderAgentInfo> getBorderAgents() {
    return borderAgents;
  }

  @Override
  public int describeContents() {
    return 0;
  }

  @Override
  public void writeToParcel(Parcel dest, int flags) {
    dest.writeParcelable(networkInfo, flags);
    dest.writeParcelableArray((BorderAgentInfo[]) borderAgents.toArray(), flags);
  }
}
