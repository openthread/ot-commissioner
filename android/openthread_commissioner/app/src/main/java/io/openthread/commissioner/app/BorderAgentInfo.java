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

package io.openthread.commissioner.app;

import android.os.Parcel;
import android.os.Parcelable;
import androidx.annotation.Nullable;
import com.google.common.net.InetAddresses;
import java.net.InetAddress;

public class BorderAgentInfo implements Parcelable {
  public final boolean isEpskcService;
  public final String instanceName;
  public final InetAddress host;
  public final int port;
  @Nullable public final byte[] id;
  @Nullable public final String networkName;
  @Nullable public final byte[] extendedPanId;
  @Nullable public final String vendorName;
  @Nullable public final String modelName;

  public BorderAgentInfo(
      boolean isEpskcService,
      String instanceName,
      InetAddress host,
      int port,
      @Nullable byte[] id,
      @Nullable String networkName,
      @Nullable byte[] extendedPanId,
      @Nullable String vendorName,
      @Nullable String modelName) {
    this.isEpskcService = isEpskcService;
    this.instanceName = instanceName;
    this.host = host;
    this.port = port;
    this.id = id == null ? null : id.clone();
    this.networkName = networkName;
    this.extendedPanId = extendedPanId == null ? null : extendedPanId.clone();
    this.vendorName = vendorName;
    this.modelName = modelName;
  }

  protected BorderAgentInfo(Parcel in) {
    isEpskcService = in.readInt() != 0;
    instanceName = in.readString();
    host = InetAddresses.forString(in.readString());
    port = in.readInt();
    id = in.createByteArray();
    networkName = in.readString();
    extendedPanId = in.createByteArray();
    vendorName = in.readString();
    modelName = in.readString();
  }

  @Override
  public void writeToParcel(Parcel dest, int flags) {
    dest.writeInt(isEpskcService ? 1 : 0);
    dest.writeString(instanceName);
    dest.writeString(host.getHostAddress());
    dest.writeInt(port);
    dest.writeByteArray(id);
    dest.writeString(networkName);
    dest.writeByteArray(extendedPanId);
    dest.writeString(vendorName);
    dest.writeString(modelName);
  }

  @Override
  public int describeContents() {
    return 0;
  }

  public static final Creator<BorderAgentInfo> CREATOR =
      new Creator<BorderAgentInfo>() {
        @Override
        public BorderAgentInfo createFromParcel(Parcel in) {
          return new BorderAgentInfo(in);
        }

        @Override
        public BorderAgentInfo[] newArray(int size) {
          return new BorderAgentInfo[size];
        }
      };
}
