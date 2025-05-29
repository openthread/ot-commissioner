/*
 *    Copyright (c) 2024, The OpenThread Commissioner Authors.
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

import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.ListView;
import androidx.annotation.NonNull;
import androidx.core.content.ContextCompat;
import androidx.fragment.app.Fragment;
import com.google.android.gms.threadnetwork.ThreadNetworkCredentials;
import com.google.common.util.concurrent.FluentFuture;
import com.google.common.util.concurrent.FutureCallback;

public class SelectBorderRouterFragment extends Fragment {

  private static final String TAG = SelectBorderRouterFragment.class.getSimpleName();

  private final FragmentCallback fragmentCallback;

  private BorderAgentAdapter borderAgentAdapter;

  private Button retrieveDatasetButton;
  private Button setDatasetButton;
  private Button addDeviceButton;

  private BorderAgentDiscoverer meshcopDiscoverer;
  private BorderAgentDiscoverer meshcopEpskcDiscoverer;

  private BorderAgentInfo selectedBorderAgent;

  public SelectBorderRouterFragment(FragmentCallback fragmentCallback) {
    this.fragmentCallback = fragmentCallback;
  }

  @Override
  public void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    Log.d(TAG, "::onCreate");

    borderAgentAdapter = new BorderAgentAdapter(requireContext());
    meshcopDiscoverer =
        new BorderAgentDiscoverer(
            requireContext(), BorderAgentDiscoverer.MESHCOP_SERVICE_TYPE, borderAgentAdapter);
    meshcopDiscoverer.start();
    meshcopEpskcDiscoverer =
        new BorderAgentDiscoverer(
            requireContext(), BorderAgentDiscoverer.MESHCOP_E_SERVICE_TYPE, borderAgentAdapter);
    meshcopEpskcDiscoverer.start();
  }

  @Override
  public void onDestroy() {
    super.onDestroy();

    Log.d(TAG, "::onDestroy");

    meshcopDiscoverer.stop();
    meshcopEpskcDiscoverer.stop();
    borderAgentAdapter.clear();
  }

  @Override
  public void onResume() {
    super.onResume();

    Log.d(TAG, "::onResume");

    meshcopDiscoverer.start();
    meshcopEpskcDiscoverer.start();
  }

  @Override
  public void onPause() {
    super.onPause();

    Log.d(TAG, "::onPause");

    meshcopDiscoverer.stop();
    meshcopEpskcDiscoverer.stop();
    borderAgentAdapter.clear();
  }

  @Override
  public View onCreateView(
      LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
    return inflater.inflate(R.layout.fragment_select_border_router, container, false);
  }

  @Override
  public void onViewCreated(@NonNull View view, Bundle savedInstanceState) {
    super.onViewCreated(view, savedInstanceState);

    // Hide the buttons
    retrieveDatasetButton = view.findViewById(R.id.retrieve_dataset_button);
    retrieveDatasetButton.setVisibility(View.INVISIBLE);
    setDatasetButton = view.findViewById(R.id.set_dataset_button);
    setDatasetButton.setVisibility(View.INVISIBLE);
    addDeviceButton = view.findViewById(R.id.add_device_button);
    addDeviceButton.setVisibility(View.INVISIBLE);

    final ListView borderAgentListView = view.findViewById(R.id.networks);
    borderAgentListView.setAdapter(borderAgentAdapter);

    borderAgentListView.setOnItemClickListener(
        (AdapterView<?> adapterView, View v, int position, long id) -> {
          selectedBorderAgent = (BorderAgentInfo) adapterView.getItemAtPosition(position);
          retrieveDatasetButton.setVisibility(View.VISIBLE);
          setDatasetButton.setVisibility(View.VISIBLE);
          addDeviceButton.setVisibility(View.VISIBLE);
        });

    retrieveDatasetButton.setOnClickListener(
        v -> {
          if (validateSelectedBorderAgent()) {
            FragmentUtils.moveToNextFragment(
                this,
                new GetAdminPasscodeFragment(
                    fragmentCallback,
                    selectedBorderAgent,
                    GetAdminPasscodeFragment.FLOW_RETRIEVE_DATASET));
          }
        });
    setDatasetButton.setOnClickListener(
        v -> {
          if (validateSelectedBorderAgent()) {
            FragmentUtils.moveToNextFragment(
                this,
                new GetAdminPasscodeFragment(
                    fragmentCallback,
                    selectedBorderAgent,
                    GetAdminPasscodeFragment.FLOW_SET_DATASET));
          }
        });

    addDeviceButton.setOnClickListener(v -> onAddDeviceButtonClicked());
  }

  private void onAddDeviceButtonClicked() {
    if (!validateSelectedBorderAgent()) {
      return;
    }

    ThreadCommissionerService commissionerService =
        ThreadCommissionerServiceImpl.newInstance(
            getActivity(), /* intermediateStateCallback= */ null);

    FluentFuture.from(commissionerService.getThreadNetworkCredentials(selectedBorderAgent))
        .addCallback(
            new FutureCallback<ThreadNetworkCredentials>() {
              @Override
              public void onSuccess(ThreadNetworkCredentials credentials) {
                if (credentials != null) {
                  FragmentUtils.moveToNextFragment(
                      SelectBorderRouterFragment.this,
                      new ScanQrCodeFragment(
                          fragmentCallback, selectedBorderAgent, credentials.getPskc()));
                } else {
                  FragmentUtils.moveToNextFragment(
                      SelectBorderRouterFragment.this,
                      new InputNetworkPasswordFragment(fragmentCallback, selectedBorderAgent));
                }
              }

              @Override
              public void onFailure(Throwable t) {
                Log.e(TAG, "Failed to retrieve Thread network credentials from GMS", t);
                FragmentUtils.showAlertAndExit(
                    getActivity(),
                    fragmentCallback,
                    "Retrieve Thread Credentials Error",
                    "Failed to retrieve Thread network credentials from GMS: " + t.getMessage());
              }
            },
            ContextCompat.getMainExecutor(getActivity()));
  }

  private boolean validateSelectedBorderAgent() {
    if (selectedBorderAgent.id == null) {
      FragmentUtils.showAlertAndExit(
          getActivity(),
          fragmentCallback,
          "Invalid Border Router",
          "No \"id\" TXT entry found in Border Router mDNS service");
      return false;
    }

    if (selectedBorderAgent.extendedPanId == null) {
      FragmentUtils.showAlertAndExit(
          getActivity(),
          fragmentCallback,
          "Invalid Border Router",
          "No \"xp\" TXT entry found in Border Router mDNS service");
      return false;
    }

    return true;
  }
}
