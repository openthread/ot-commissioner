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

import android.content.Intent;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.TextView;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;

public class MainActivity extends AppCompatActivity implements FragmentCallback {

  private static final String TAG = MainActivity.class.getSimpleName();

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
      showFragment(new SelectBorderRouterFragment(this), false);
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
    clearFragmentsInBackStack();
  }

  @Override
  public void onAddDeviceResult(int result) {
    finishCommissioning(result);
  }

  @Override
  public void onSetDatasetResult(int result) {
    finishCommissioning(result);
  }

  @Override
  public void onRetrieveDatasetResult(int result) {
    finishCommissioning(result);
  }

  /*
  public void onGetAdminPasscodeStarted(BorderAgentInfo borderAgentInfo, int adminPasscodeFlow) {
    showFragment(
        new GetAdminPasscodeFragment(this, borderAgentInfo, adminPasscodeFlow),
         true);
  }

  public void onAdminPasscodeReceived(
      BorderAgentInfo borderAgentInfo, int adminPasscodeFlow, String passcode, int epskcPort) {
    if (adminPasscodeFlow == GetAdminPasscodeFragment.FLOW_RETRIEVE_DATASET) {
      showFragment(
          new RetrieveDatasetFragment(this, borderAgentInfo, passcode, epskcPort),
           true);
    } else if (adminPasscodeFlow == GetAdminPasscodeFragment.FLOW_SET_DATASET) {
      showFragment(
          new SetDatasetFragment(this, borderAgentInfo, passcode, epskcPort),
           true);
    } else {
      throw new AssertionError("Unknown Admin Passcode flow: " + adminPasscodeFlow);
    }
  }

  public void onCredentialsRetrieved() {
    clearFragmentsInBackStack();
  }
  */
}
