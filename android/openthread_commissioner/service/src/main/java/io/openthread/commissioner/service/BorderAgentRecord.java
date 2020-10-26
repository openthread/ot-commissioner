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

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.room.ColumnInfo;
import androidx.room.Entity;
import androidx.room.PrimaryKey;

@Entity(tableName = "border_agent_table")
public class BorderAgentRecord {

  @PrimaryKey
  @NonNull
  @ColumnInfo(name = "discriminator")
  private String discriminator;

  @NonNull
  @ColumnInfo(name = "network_name")
  private String networkName;

  @NonNull
  @ColumnInfo(name = "extended_pan_id", typeAffinity = ColumnInfo.BLOB)
  private byte[] extendedPanId;

  @NonNull
  @ColumnInfo(name = "pskc", typeAffinity = ColumnInfo.BLOB)
  private byte[] pskc;

  @Nullable
  @ColumnInfo(name = "active_operational_dataset", typeAffinity = ColumnInfo.BLOB)
  private byte[] activeOperationalDataset;

  public BorderAgentRecord(
      @NonNull String discriminator,
      @NonNull String networkName,
      @NonNull byte[] extendedPanId,
      @NonNull byte[] pskc,
      @Nullable byte[] activeOperationalDataset) {
    this.discriminator = discriminator;
    this.networkName = networkName;
    this.extendedPanId = extendedPanId;
    this.pskc = pskc;
    this.activeOperationalDataset = activeOperationalDataset;
  }

  @NonNull
  public String getDiscriminator() {
    return discriminator;
  }

  @NonNull
  public String getNetworkName() {
    return networkName;
  }

  @NonNull
  public byte[] getExtendedPanId() {
    return extendedPanId;
  }

  @NonNull
  public byte[] getPskc() {
    return pskc;
  }

  @Nullable
  public byte[] getActiveOperationalDataset() {
    return activeOperationalDataset;
  }
}
