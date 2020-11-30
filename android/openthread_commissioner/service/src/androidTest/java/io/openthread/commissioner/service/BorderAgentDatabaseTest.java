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

import static org.junit.Assert.*;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import java.util.concurrent.ExecutionException;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class BorderAgentDatabaseTest {

  @Test
  public void simpleBorderAgentRecord() {
    BorderAgentDatabase db = BorderAgentDatabase.getDatabase();
    BorderAgentRecord record =
        new BorderAgentRecord(
            "12345", "test-net", new byte[] {0x01}, new byte[] {0x02}, new byte[] {0x03});

    try {
      db.insertBorderAgent(record).get();
      db.deleteBorderAgent("54321").get();
      record = db.getBorderAgent("12345").get();

      assertNotNull(record);
      assertEquals(record.getDiscriminator(), "12345");
      assertEquals(record.getNetworkName(), "test-net");
      assertArrayEquals(record.getExtendedPanId(), new byte[] {0x01});
      assertArrayEquals(record.getPskc(), new byte[] {0x02});
      assertArrayEquals(record.getActiveOperationalDataset(), new byte[] {0x03});

      db.deleteBorderAgent("12345").get();
      record = db.getBorderAgent("12345").get();
      assertNull(record);
    } catch (InterruptedException | ExecutionException e) {
      fail();
    }
  }
}
