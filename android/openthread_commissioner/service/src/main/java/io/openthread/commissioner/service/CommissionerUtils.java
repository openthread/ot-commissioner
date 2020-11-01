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

import androidx.annotation.Nullable;
import io.openthread.commissioner.ByteArray;

public class CommissionerUtils {

  public static byte[] getByteArray(@Nullable ByteArray byteArray) {
    if (byteArray == null) {
      return null;
    }
    byte[] ret = new byte[byteArray.size()];
    for (int i = 0; i < byteArray.size(); ++i) {
      ret[i] = byteArray.get(i);
    }
    return ret;
  }

  public static byte[] getByteArray(@Nullable String hexString) {
    if (hexString == null || (hexString.length() % 2 != 0)) {
      return null;
    }
    int len = hexString.length();
    byte[] data = new byte[len / 2];
    for (int i = 0; i < len; i += 2) {
      int firstDecimal = Character.digit(hexString.charAt(i), 16);
      int secondDecimal = Character.digit(hexString.charAt(i + 1), 16);
      if (firstDecimal == -1 || secondDecimal == -1) {
        return null;
      }
      data[i / 2] = (byte) ((firstDecimal << 4) + secondDecimal);
    }
    return data;
  }

  public static String getHexString(@Nullable byte[] byteArray) {
    if (byteArray == null) {
      return null;
    }

    StringBuilder strbuilder = new StringBuilder();
    for (byte b : byteArray) {
      strbuilder.append(String.format("%02x", b));
    }
    return strbuilder.toString();
  }

  public static String getHexString(ByteArray byteArray) {
    return getHexString(getByteArray(byteArray));
  }
}
