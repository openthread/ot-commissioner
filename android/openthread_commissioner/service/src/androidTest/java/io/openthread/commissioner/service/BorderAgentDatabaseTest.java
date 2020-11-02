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
