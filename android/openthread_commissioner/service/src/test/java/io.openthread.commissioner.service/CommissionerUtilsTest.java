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

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import io.openthread.commissioner.ByteArray;
import org.junit.Test;

public class CommissionerUtilsTest {
  @Test
  public void testGetByteArrayFromHexString() {
    assertNull(CommissionerUtils.getByteArray("abcdeff"));
    assertNull(CommissionerUtils.getByteArray("abcdefgg"));

    byte[] emptyArray = CommissionerUtils.getByteArray("");
    assertNotNull(emptyArray);
    assertTrue(emptyArray.length == 0);

    assertArrayEquals(CommissionerUtils.getByteArray("0a0b0c0d"), new byte[] {0xa, 0xb, 0xc, 0xd});
    assertArrayEquals(CommissionerUtils.getByteArray("AABBCCDD"), new byte[] {(byte)0xaa, (byte)0xbb, (byte)0xcc, (byte)0xdd});
  }

  @Test
  public void testGetByteArrayFromNativeByteArray() {
    byte[] emptyArray = CommissionerUtils.getByteArray(new ByteArray());
    assertNotNull(emptyArray);
    assertTrue(emptyArray.length == 0);

    ByteArray nativeArray = new ByteArray(new byte[] {0xa, 0xb, 0xc, 0xd});
    assertArrayEquals(CommissionerUtils.getByteArray(nativeArray), new byte[] {0xa, 0xb, 0xc, 0xd});

    nativeArray = new ByteArray(new byte[] {(byte)0xaa, (byte)0xbb, (byte)0xcc, (byte)0xdd});
    assertArrayEquals(CommissionerUtils.getByteArray(nativeArray), new byte[] {(byte)0xaa, (byte)0xbb, (byte)0xcc, (byte)0xdd});
  }

  @Test
  public void testGetHexStringFromByteArray() {
    assertEquals(CommissionerUtils.getHexString(new byte[]{}), "");
    assertEquals(CommissionerUtils.getHexString(new byte[]{0xa, 0xb, 0xc, 0xd}), "0a0b0c0d");
  }

  @Test
  public void testGetHexStringFromNativeByteArray() {
    assertEquals(CommissionerUtils.getHexString(new ByteArray(new byte[]{})), "");
    assertEquals(CommissionerUtils.getHexString(new ByteArray(new byte[]{0xa, 0xb, 0xc, 0xd})), "0a0b0c0d");
  }
}
