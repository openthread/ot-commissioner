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

public class ThreadNetworkInfo implements Parcelable {

  @NonNull private final String networkName;

  @NonNull private final byte[] extendedPanId;

  public ThreadNetworkInfo(@NonNull String networkName, @NonNull byte[] extendedPanId) {
    this.networkName = networkName;
    this.extendedPanId = extendedPanId;
  }

  protected ThreadNetworkInfo(Parcel in) {
    networkName = in.readString();
    extendedPanId = in.createByteArray();
  }

  public String getNetworkName() {
    return networkName;
  }

  public byte[] getExtendedPanId() {
    return extendedPanId;
  }

  public static final Creator<ThreadNetworkInfo> CREATOR =
      new Creator<ThreadNetworkInfo>() {
        @Override
        public ThreadNetworkInfo createFromParcel(Parcel in) {
          return new ThreadNetworkInfo(in);
        }

        @Override
        public ThreadNetworkInfo[] newArray(int size) {
          return new ThreadNetworkInfo[size];
        }
      };

  @Override
  public int describeContents() {
    return 0;
  }

  @Override
  public void writeToParcel(Parcel parcel, int flags) {
    parcel.writeString(networkName);
    parcel.writeByteArray(extendedPanId);
  }

  @Override
  public String toString() {
    return String.format(
        "{name=%s, extendedPanId=%s}", networkName, CommissionerUtils.getHexString(extendedPanId));
  }
}
