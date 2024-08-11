package io.openthread.commissioner;

import static com.google.common.truth.Truth.assertThat;

import java.util.stream.Collectors;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class ActiveOperationalDatasetTest {
  @Test
  public void channelMask_valuesAreCorrect() {
    ActiveOperationalDataset dataset = new ActiveOperationalDataset();
    ChannelMaskEntry entry1 = new ChannelMaskEntry();
    entry1.setPage((short) 0);
    entry1.setMasks(new ByteArray(new byte[] {0x01, 0x02, 0x03, 0x04}));
    ChannelMaskEntry entry2 = new ChannelMaskEntry();
    entry2.setPage((short) 1);
    entry2.setMasks(new ByteArray(new byte[] {0x05, 0x06, 0x07, 0x08}));
    ChannelMask maskEntries = new ChannelMask();
    maskEntries.add(entry1);
    maskEntries.add(entry2);
    dataset.setChannelMask(maskEntries);

    assertThat(dataset.getChannelMask()).hasSize(2);
    assertThat(dataset.getChannelMask().get(0).getPage()).isEqualTo(0);
    assertThat(dataset.getChannelMask().get(0).getMasks().stream().collect(Collectors.toList()))
        .containsExactly((byte) 0x01, (byte) 0x02, (byte) 0x03, (byte) 0x04);
    assertThat(dataset.getChannelMask().get(1).getPage()).isEqualTo(1);
    assertThat(dataset.getChannelMask().get(1).getMasks().stream().collect(Collectors.toList()))
        .containsExactly((byte) 0x05, (byte) 0x06, (byte) 0x07, (byte) 0x08);
  }
}
