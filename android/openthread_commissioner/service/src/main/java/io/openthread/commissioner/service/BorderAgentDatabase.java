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
import androidx.room.Database;
import androidx.room.Room;
import androidx.room.RoomDatabase;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

@Database(
    entities = {BorderAgentRecord.class},
    version = 1,
    exportSchema = false)
abstract class BorderAgentDatabase extends RoomDatabase {

  abstract BorderAgentDao borderAgentDao();

  private static volatile BorderAgentDatabase INSTANCE;
  private static final int NUMBER_OF_THREADS = 2;

  static final ExecutorService executor = Executors.newFixedThreadPool(NUMBER_OF_THREADS);

  public static synchronized BorderAgentDatabase getDatabase() {
    if (INSTANCE == null) {
      INSTANCE =
          Room.databaseBuilder(
                  CommissionerServiceApp.getContext(),
                  BorderAgentDatabase.class,
                  "network_credential_database")
              .build();
    }
    return INSTANCE;
  }

  public CompletableFuture<BorderAgentRecord> getBorderAgent(@NonNull String discriminator) {
    CompletableFuture<BorderAgentRecord> future =
        CompletableFuture.supplyAsync(() -> borderAgentDao().getBorderAgent(discriminator));
    return future;
  }

  public CompletableFuture<Void> insertBorderAgent(@NonNull BorderAgentRecord borderAgentRecord) {
    CompletableFuture<Void> future =
        CompletableFuture.runAsync(
            () -> {
              borderAgentDao().insert(borderAgentRecord);
            },
            executor);
    return future;
  }

  public CompletableFuture<Void> deleteBorderAgent(@NonNull String discriminator) {
    CompletableFuture<Void> future =
        CompletableFuture.runAsync(
            () -> {
              borderAgentDao().delete(discriminator);
            },
            executor);
    return future;
  }
}
