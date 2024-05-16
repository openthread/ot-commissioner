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
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.ListView;
import android.widget.TextView;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;
import androidx.fragment.app.Fragment;
import com.google.android.gms.threadnetwork.ThreadNetworkCredentials;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.common.util.concurrent.FluentFuture;
import com.google.common.util.concurrent.FutureCallback;
import io.openthread.commissioner.ByteArray;
import io.openthread.commissioner.Commissioner;
import io.openthread.commissioner.Error;
import io.openthread.commissioner.ErrorCode;

public class SelectNetworkFragment extends Fragment
    implements InputNetworkPasswordDialogFragment.PasswordDialogListener,
        FetchCredentialDialogFragment.CredentialListener,
        View.OnClickListener {

  private static final String TAG = SelectNetworkFragment.class.getSimpleName();

  private FragmentCallback networkInfoCallback;

  @Nullable private JoinerDeviceInfo joinerDeviceInfo;

  private NetworkAdapter networksAdapter;

  private ThreadNetworkInfoHolder selectedNetwork;
  private byte[] userInputPskc;
  private Button addDeviceButton;

  private BorderAgentDiscoverer borderAgentDiscoverer;

  public SelectNetworkFragment() {}

  public SelectNetworkFragment(
      @NonNull FragmentCallback networkInfoCallback, @Nullable JoinerDeviceInfo joinerDeviceInfo) {
    this.networkInfoCallback = networkInfoCallback;
    this.joinerDeviceInfo = joinerDeviceInfo;
  }

  @Override
  public void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    Log.d(TAG, "::onCreate");

    networksAdapter = new NetworkAdapter(getContext());
    borderAgentDiscoverer = new BorderAgentDiscoverer(getContext(), networksAdapter);
    borderAgentDiscoverer.start();
  }

  @Override
  public void onDestroy() {
    super.onDestroy();

    Log.d(TAG, "::onDestroy");

    borderAgentDiscoverer.stop();
  }

  @Override
  public void onResume() {
    super.onResume();

    Log.d(TAG, "::onResume");

    borderAgentDiscoverer.start();
  }

  @Override
  public void onPause() {
    super.onPause();

    Log.d(TAG, "::onPause");

    borderAgentDiscoverer.stop();
  }

  @Override
  public View onCreateView(
      LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
    // Inflate the layout for this fragment
    return inflater.inflate(R.layout.fragment_select_network, container, false);
  }

  @Override
  public void onViewCreated(@NonNull View view, Bundle savedInstanceState) {
    super.onViewCreated(view, savedInstanceState);

    // Hide the button
    addDeviceButton = view.findViewById(R.id.add_device_button);
    addDeviceButton.setVisibility(View.GONE);

    if (joinerDeviceInfo != null) {
      String deviceInfoString =
          String.format(
              "eui64: %s\npskd: %s",
              CommissionerUtils.getHexString(joinerDeviceInfo.getEui64()),
              joinerDeviceInfo.getPskd());
      TextView deviceInfoView = view.findViewById(R.id.device_info);
      deviceInfoView.setText(deviceInfoString);
    } else {
      view.findViewById(R.id.your_device).setVisibility(View.INVISIBLE);
      view.findViewById(R.id.device_info).setVisibility(View.INVISIBLE);
    }

    final ListView networkListView = view.findViewById(R.id.networks);
    networkListView.setAdapter(networksAdapter);

    networkListView.setOnItemClickListener(
        (AdapterView<?> adapterView, View v, int position, long id) -> {
          selectedNetwork = (ThreadNetworkInfoHolder) adapterView.getItemAtPosition(position);
          addDeviceButton.setVisibility(View.VISIBLE);
        });

    view.findViewById(R.id.add_device_button).setOnClickListener(this);
  }

  // Click listeners for network password dialog.

  @Override
  public void onPositiveClick(InputNetworkPasswordDialogFragment fragment, String password) {
    BorderAgentInfo selectedBorderAgent = selectedNetwork.getBorderAgents().get(0);
    userInputPskc = computePskc(selectedNetwork.getNetworkInfo(), password);

    networkInfoCallback.onNetworkSelected(selectedNetwork, userInputPskc);
  }

  @Override
  public void onNegativeClick(InputNetworkPasswordDialogFragment fragment) {
    networkInfoCallback.onNetworkSelected(selectedNetwork, null);
  }

  private byte[] computePskc(ThreadNetworkInfo threadNetworkInfo, String password) {
    ByteArray extendedPanId = new ByteArray(threadNetworkInfo.getExtendedPanId());
    ByteArray pskc = new ByteArray();
    Error error =
        Commissioner.generatePSKc(
            pskc, password, threadNetworkInfo.getNetworkName(), extendedPanId);
    if (error.getCode() != ErrorCode.kNone) {
      Log.e(
          TAG,
          String.format(
              "failed to generate PSKc: %s; network-name=%s, extended-pan-id=%s",
              error.toString(),
              threadNetworkInfo.getNetworkName(),
              CommissionerUtils.getHexString(threadNetworkInfo.getExtendedPanId())));
    } else {
      Log.d(
          TAG,
          String.format(
              "generated pskc=%s, network-name=%s, extended-pan-id=%s",
              CommissionerUtils.getHexString(pskc),
              threadNetworkInfo.getNetworkName(),
              CommissionerUtils.getHexString(threadNetworkInfo.getExtendedPanId())));
    }

    return CommissionerUtils.getByteArray(pskc);
  }

  @Override
  public void onClick(View view) {
    BorderAgentInfo selectedBorderAgent = selectedNetwork.getBorderAgents().get(0);
    if (selectedBorderAgent.id == null) {
      showAlertDialog("Invalid Border Router: no \"id\" associated");
      return;
    }

    ThreadCommissionerServiceImpl commissionerService =
        ThreadCommissionerServiceImpl.newInstance(null);

    FluentFuture.from(commissionerService.getThreadNetworkCredentials(selectedBorderAgent))
        .addCallback(
            new FutureCallback<ThreadNetworkCredentials>() {
              @Override
              public void onSuccess(ThreadNetworkCredentials credentials) {
                if (credentials != null) {
                  networkInfoCallback.onNetworkSelected(selectedNetwork, credentials.getPskc());
                } else {
                  // Ask the user to input Commissioner password.
                  new InputNetworkPasswordDialogFragment(SelectNetworkFragment.this)
                      .show(
                          getParentFragmentManager(),
                          InputNetworkPasswordDialogFragment.class.getSimpleName());
                }
              }

              @Override
              public void onFailure(Throwable t) {
                Log.e(TAG, "Failed to retrieve Thread network credentials from GMS", t);
                showAlertDialog(t.getMessage());
              }
            },
            ContextCompat.getMainExecutor(getActivity()));
  }

  private void showAlertDialog(String message) {
    new MaterialAlertDialogBuilder(getActivity(), R.style.ThreadNetworkAlertTheme)
        .setMessage(message)
        .setPositiveButton(
            "OK",
            ((dialog, which) -> {
              networkInfoCallback.onMeshcopResult(Activity.RESULT_CANCELED);
            }))
        .show();
  }

  @Override
  public void onCancelClick(FetchCredentialDialogFragment fragment) {
    // TODO:
  }

  @Override
  public void onConfirmClick(
      FetchCredentialDialogFragment fragment, ThreadNetworkCredentials credentials) {
    // TODO:
  }
}
