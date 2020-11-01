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

public class CHIPDeviceInfo implements Parcelable {

  @NonNull private int version;

  @NonNull private int vendorId;

  @NonNull private int productId;

  @NonNull private int discriminator;

  @NonNull private long setupPinCode;

  protected CHIPDeviceInfo(Parcel in) {
    version = in.readInt();
    vendorId = in.readInt();
    productId = in.readInt();
    discriminator = in.readInt();
    setupPinCode = in.readLong();
  }

  public static final Creator<CHIPDeviceInfo> CREATOR =
      new Creator<CHIPDeviceInfo>() {
        @Override
        public CHIPDeviceInfo createFromParcel(Parcel in) {
          return new CHIPDeviceInfo(in);
        }

        @Override
        public CHIPDeviceInfo[] newArray(int size) {
          return new CHIPDeviceInfo[size];
        }
      };

  public int getDiscriminator() {
    return discriminator;
  }

  public long getSetupPinCode() {
    return setupPinCode;
  }

  @Override
  public int describeContents() {
    return 0;
  }

  @Override
  public void writeToParcel(Parcel dest, int flags) {
    dest.writeInt(version);
    dest.writeInt(vendorId);
    dest.writeInt(productId);
    dest.writeInt(discriminator);
    dest.writeLong(setupPinCode);
  }
}
