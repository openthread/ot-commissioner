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

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.Fragment;

public class CommissionerActivity extends AppCompatActivity implements FragmentCallback {
  private static final String TAG = CommissionerActivity.class.getSimpleName();

  public static final String ACTION_COMMISSION_DEVICE =
      "io.openthread.commissioner.action.COMMISSION_DEVICE";
  public static final String ACTION_FETCH_CREDENTIAL =
      "io.openthread.commissioner.action.FETCH_CREDENTIAL";

  public static final String KEY_JOINER_DISCERNER = "JOINER_DISCERNER";
  public static final String KEY_JOINER_DISCERNER_BIT_LENGTH = "JOINER_DISCERNER_BIT_LENGTH";
  public static final String KEY_JOINER_EUI64 = "JOINER_EUI64";
  public static final String KEY_JOINER_PSKD = "JOINER_PSKD";

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_commissioner);

    Intent intent = getIntent();
    if (intent == null) {
      Log.e(TAG, "no intent provided");
      finishCommissioning(Activity.RESULT_CANCELED);
    } else if (ACTION_COMMISSION_DEVICE.equals(intent.getAction())) {
      JoinerDeviceInfo joinerDeviceInfo = getJoinerDevice(intent);
      if (joinerDeviceInfo == null) {
        // TODO(wgtdkp): scan QR code of the joiner device.
        showFragment(new ScanQrCodeFragment(this));
      } else {
        showFragment(new SelectNetworkFragment(this, joinerDeviceInfo));
      }
    } else if (ACTION_FETCH_CREDENTIAL.equals(intent.getAction())) {
      // TODO(wgtdkp):
      //  0. Ask confirmation from the user of granting access of
      //  the Thread Network Credential to the requesting App.
      //  1. Query network credential from local database.
      //  2. Fetch network credential from given network when querying
      //     the database failed.
    } else {
      assert (false);
    }
  }

  private JoinerDeviceInfo getJoinerDevice(Intent intent) {
    byte[] eui64 = intent.getByteArrayExtra(KEY_JOINER_EUI64);
    String pskd = intent.getStringExtra(KEY_JOINER_PSKD);

    if (eui64 == null) {
      Log.i(TAG, "missing joiner discerner!");
      return null;
    } else if (pskd == null) {
      Log.i(TAG, "missing joiner pskd!");
      return null;
    } else {
      return new JoinerDeviceInfo(eui64, pskd);
    }
  }

  public void finishCommissioning(int resultCode) {
    Intent resultIntent = new Intent();
    setResult(resultCode, resultIntent);
    finish();
  }

  public void showFragment(Fragment fragment) {
    getSupportFragmentManager()
        .beginTransaction()
        .replace(R.id.commissioner_service_activity, fragment, fragment.getClass().getSimpleName())
        .addToBackStack(null)
        .commit();
  }

  @Override
  public void onJoinerInfoReceived(@Nullable JoinerDeviceInfo joinerDeviceInfo) {
    showFragment(new SelectNetworkFragment(this, joinerDeviceInfo));
  }

  @Override
  public void onNetworkSelected(
      @NonNull ThreadNetworkInfoHolder networkInfoHolder, @NonNull byte[] pskc) {
    // TODO(wgtdkp): goto commissioning fragment.
  }

  @Override
  public void onMeshcopResult(int result) {
    // TODO(wgtdkp):
  }
}
