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

package io.openthread.commissioner.app;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.TextView;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;
import io.openthread.commissioner.service.FragmentCallback;
import io.openthread.commissioner.service.JoinerDeviceInfo;
import io.openthread.commissioner.service.MeshcopFragment;
import io.openthread.commissioner.service.ScanQrCodeFragment;
import io.openthread.commissioner.service.SelectNetworkFragment;
import io.openthread.commissioner.service.ThreadNetworkInfoHolder;

public class MainActivity extends AppCompatActivity implements FragmentCallback {

  private static final String TAG = MainActivity.class.getSimpleName();

  @Nullable private ThreadNetworkInfoHolder selectedNetwork;

  @Nullable private byte[] pskc;

  @Nullable private JoinerDeviceInfo joinerDeviceInfo;

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_main);
    setSupportActionBar(findViewById(R.id.toolbar));

    TextView versionName = findViewById(R.id.version_name);
    versionName.setText(BuildConfig.VERSION_NAME);
    TextView gitHash = findViewById(R.id.git_hash);
    gitHash.setText(BuildConfig.GIT_HASH);

    if (savedInstanceState == null) {
      showFragment(new SelectNetworkFragment(this, null), false);
    }
  }

  @Override
  public boolean onCreateOptionsMenu(Menu menu) {
    // Inflate the menu; this adds items to the action bar if it is present.
    getMenuInflater().inflate(R.menu.menu_main, menu);
    return true;
  }

  @Override
  public boolean onOptionsItemSelected(MenuItem item) {
    // TODO(wgtdkp):
    // Handle action bar item clicks here. The action bar will
    // automatically handle clicks on the Home/Up button, so long
    // as you specify a parent activity in AndroidManifest.xml.
    int id = item.getItemId();

    if (id == R.id.action_settings) {
      return true;
    }

    return super.onOptionsItemSelected(item);
  }

  @Override
  protected void onActivityResult(int requestCode, int resultCode, Intent data) {
    super.onActivityResult(requestCode, resultCode, data);
  }

  private void showFragment(Fragment fragment, boolean addToBackStack) {
    FragmentTransaction transaction = getSupportFragmentManager().beginTransaction();
    transaction.replace(R.id.fragment_container, fragment, fragment.getClass().getSimpleName());
    if (addToBackStack) {
      transaction.addToBackStack(null);
    }
    transaction.commit();
  }

  private void clearFragmentsInBackStack() {
    getSupportFragmentManager().popBackStack(null, FragmentManager.POP_BACK_STACK_INCLUSIVE);
  }

  private void finishCommissioning(int result) {
    // TODO(wgtdkp): show a dialog to the user.
    selectedNetwork = null;
    pskc = null;
    joinerDeviceInfo = null;
    clearFragmentsInBackStack();
  }

  @Override
  public void onNetworkSelected(
      @Nullable ThreadNetworkInfoHolder networkInfoHolder, @Nullable byte[] pskc) {
    if (networkInfoHolder == null || pskc == null) {
      Log.e(TAG, "failed to get selected network or PSKc!");
      finishCommissioning(Activity.RESULT_CANCELED);
      return;
    }

    this.selectedNetwork = networkInfoHolder;
    this.pskc = pskc;
    showFragment(new ScanQrCodeFragment(this), true);
  }

  @Override
  public void onJoinerInfoReceived(@Nullable JoinerDeviceInfo joinerDeviceInfo) {
    if (joinerDeviceInfo == null) {
      // TODO(wgtdkp): show dialog
      Log.e(TAG, "failed to get Joiner Device info!");
      finishCommissioning(Activity.RESULT_CANCELED);
      return;
    }

    if (this.joinerDeviceInfo == null) {
      this.joinerDeviceInfo = joinerDeviceInfo;
      showFragment(new MeshcopFragment(this, selectedNetwork, pskc, joinerDeviceInfo), true);
    }
  }

  @Override
  public void onMeshcopResult(int result) {
    finishCommissioning(result);
  }
}
