package io.openthread.commissioner;

import static com.google.common.truth.Truth.assertThat;

import com.google.common.collect.ImmutableList;
import java.util.Arrays;
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
    entry1.setMasks(Arrays.asList(new Short[] {0x01, 0x02, 0x03, 0x04}));
    ChannelMaskEntry entry2 = new ChannelMaskEntry();
    entry2.setPage((short) 1);
    entry2.setMasks(Arrays.asList(new Short[] {0x05, 0x06, 0x07, 0x08}));
    dataset.setChannelMask(ImmutableList.of(entry1, entry2));

    assertThat(dataset.getChannelMask()).hasSize(2);
    assertThat(dataset.getChannelMask().get(0).getPage()).isEqualTo(0);
    assertThat(dataset.getChannelMask().get(0).getMasks())
        .containsExactly((short) 0x01, (short) 0x02, (short) 0x03, (short) 0x04)
        .inOrder();
    assertThat(dataset.getChannelMask().get(1).getPage()).isEqualTo(1);
    assertThat(dataset.getChannelMask().get(1).getMasks())
        .containsExactly((short) 0x05, (short) 0x06, (short) 0x07, (short) 0x08)
        .inOrder();
  }
}
