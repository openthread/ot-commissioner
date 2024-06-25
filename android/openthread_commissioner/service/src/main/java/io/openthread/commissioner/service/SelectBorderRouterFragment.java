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

package io.openthread.commissioner.service;

import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.ListView;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

public class SelectBorderRouterFragment extends Fragment {

  private static final String TAG = SelectBorderRouterFragment.class.getSimpleName();

  private final FragmentCallback networkInfoCallback;

  @Nullable
  private final JoinerDeviceInfo joinerDeviceInfo;

  private BorderAgentAdapter borderAgentAdapter;

  private Button retrieveDatasetButton;
  private Button setDatasetButton;

  private BorderAgentDiscoverer meshcopDiscoverer;
  private BorderAgentDiscoverer meshcopEpskcDiscoverer;

  private BorderAgentInfo selectedBorderAgent;

  public SelectBorderRouterFragment(FragmentCallback networkInfoCallback) {
    this(networkInfoCallback, /* joinerDeviceInfo= */ null);
  }

  public SelectBorderRouterFragment(FragmentCallback networkInfoCallback, @Nullable JoinerDeviceInfo joinerDeviceInfo) {
    this.networkInfoCallback = networkInfoCallback;
    this.joinerDeviceInfo = joinerDeviceInfo;
  }

  @Override
  public void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    Log.d(TAG, "::onCreate");

    borderAgentAdapter = new BorderAgentAdapter(requireContext());
    meshcopDiscoverer = new BorderAgentDiscoverer(requireContext(),
        BorderAgentDiscoverer.MESHCOP_SERVICE_TYPE, borderAgentAdapter);
    meshcopDiscoverer.start();
    meshcopEpskcDiscoverer = new BorderAgentDiscoverer(requireContext(),
        BorderAgentDiscoverer.MESHCOP_E_SERVICE_TYPE, borderAgentAdapter);
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
    // Inflate the layout for this fragment
    return inflater.inflate(R.layout.fragment_select_border_router, container, false);
  }

  @Override
  public void onViewCreated(@NonNull View view, Bundle savedInstanceState) {
    super.onViewCreated(view, savedInstanceState);

    // Hide the button
    retrieveDatasetButton = view.findViewById(R.id.retrieve_dataset_button);
    retrieveDatasetButton.setVisibility(View.GONE);
    setDatasetButton = view.findViewById(R.id.set_dataset_button);
    setDatasetButton.setVisibility(View.GONE);

    final ListView borderAgentListView = view.findViewById(R.id.networks);
    borderAgentListView.setAdapter(borderAgentAdapter);

    borderAgentListView.setOnItemClickListener(
        (AdapterView<?> adapterView, View v, int position, long id) -> {
          selectedBorderAgent = (BorderAgentInfo) adapterView.getItemAtPosition(position);
          retrieveDatasetButton.setVisibility(View.VISIBLE);
          setDatasetButton.setVisibility(View.VISIBLE);
        });

    retrieveDatasetButton.setOnClickListener(
        v -> networkInfoCallback.onGetAdminPasscodeStarted(selectedBorderAgent,
            GetAdminPasscodeFragment.FLOW_RETRIEVE_DATASET));
    setDatasetButton.setOnClickListener(
        v -> networkInfoCallback.onGetAdminPasscodeStarted(selectedBorderAgent,
            GetAdminPasscodeFragment.FLOW_SET_DATASET));
  }
}
