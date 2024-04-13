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
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import io.openthread.commissioner.app.BorderAgentDiscoverer.BorderAgentListener;

public class GetAdminPasscodeFragment extends Fragment implements BorderAgentListener {

  // Indicates the flow of retrieving the dataset after getting the admin passcode
  public static int FLOW_RETRIEVE_DATASET = 0;

  // Indicates the flow of setting the dataset after getting the admin passcode
  public static int FLOW_SET_DATASET = 1;

  private static final String TAG = GetAdminPasscodeFragment.class.getSimpleName();

  private final FragmentCallback fragmentCallback;
  private final BorderAgentInfo borderAgentInfo;
  private final int flow;

  private BorderAgentDiscoverer meshcopEpskcDiscoverer;

  private TextView waitForMeshcopETextView;
  private TextView inputAdminPasscodeTextView;
  private LinearLayout adminPasscodeEditLayout;
  private EditText adminPasscodeEditText;
  private Button retrieveDatasetButton;

  @Nullable private Integer epskcPort;

  public GetAdminPasscodeFragment(
      FragmentCallback fragmentCallback, BorderAgentInfo borderAgentInfo, int flow) {
    this.fragmentCallback = fragmentCallback;
    this.borderAgentInfo = borderAgentInfo;
    this.flow = flow;
  }

  @Override
  public View onCreateView(
      LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
    // Inflate the layout for this fragment
    return inflater.inflate(R.layout.fragment_get_admin_passcode, container, false);
  }

  @Override
  public void onViewCreated(@NonNull View view, Bundle savedInstanceState) {
    super.onViewCreated(view, savedInstanceState);

    meshcopEpskcDiscoverer =
        new BorderAgentDiscoverer(
            requireContext(), BorderAgentDiscoverer.MESHCOP_E_SERVICE_TYPE, this);
    meshcopEpskcDiscoverer.start();

    waitForMeshcopETextView = view.findViewById(R.id.wait_for_device_text);
    inputAdminPasscodeTextView = view.findViewById(R.id.input_admin_passcode_text);
    inputAdminPasscodeTextView.setVisibility(View.INVISIBLE);

    adminPasscodeEditLayout = view.findViewById(R.id.admin_passcode_layout);
    adminPasscodeEditLayout.setVisibility(View.INVISIBLE);
    adminPasscodeEditText = view.findViewById(R.id.admin_passcode_edit);
    adminPasscodeEditText.addTextChangedListener(
        new TextWatcher() {
          @Override
          public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

          @Override
          public void onTextChanged(CharSequence s, int start, int before, int count) {}

          @Override
          public void afterTextChanged(Editable s) {
            if (s.length() >= 9) {
              retrieveDatasetButton.setVisibility(View.VISIBLE);
            } else {
              retrieveDatasetButton.setVisibility(View.INVISIBLE);
            }
          }
        });

    retrieveDatasetButton = view.findViewById(R.id.retrieve_dataset_button);
    retrieveDatasetButton.setText(
        flow == FLOW_RETRIEVE_DATASET ? "Retrieve Dataset" : "Set Dataset");
    retrieveDatasetButton.setVisibility(View.INVISIBLE);
    retrieveDatasetButton.setOnClickListener(this::onRetrieveDatasetClicked);
  }

  private void onRetrieveDatasetClicked(View v) {
    String passcode = adminPasscodeEditText.getText().toString().trim();
    if (passcode.length() != 9) {
      throw new AssertionError("Admin passcode should always be 9 digits");
    }

    if (flow == GetAdminPasscodeFragment.FLOW_RETRIEVE_DATASET) {
      FragmentUtils.moveToNextFragment(
          this,
          new RetrieveDatasetFragment(fragmentCallback, borderAgentInfo, passcode, epskcPort));
    } else if (flow == GetAdminPasscodeFragment.FLOW_SET_DATASET) {
      FragmentUtils.moveToNextFragment(
          this, new SetDatasetFragment(fragmentCallback, borderAgentInfo, passcode, epskcPort));
    } else {
      throw new AssertionError("Unknown Admin Passcode flow: " + flow);
    }
  }

  @Override
  public void onDestroy() {
    super.onDestroy();

    Log.d(TAG, "::onDestroy");

    meshcopEpskcDiscoverer.stop();
  }

  @Override
  public void onResume() {
    super.onResume();

    Log.d(TAG, "::onResume");

    meshcopEpskcDiscoverer.start();
  }

  @Override
  public void onPause() {
    super.onPause();

    Log.d(TAG, "::onPause");

    meshcopEpskcDiscoverer.stop();
  }

  @Override
  public void onBorderAgentFound(BorderAgentInfo meshcopEInfo) {
    if (meshcopEInfo.instanceName.equals(borderAgentInfo.instanceName)) {
      Log.i(TAG, "Found the MeshCoP-ePSKc service for " + borderAgentInfo.instanceName);

      epskcPort = meshcopEInfo.port;

      requireActivity()
          .runOnUiThread(
              () -> {
                waitForMeshcopETextView.setText(
                    "Device " + borderAgentInfo.instanceName + " is ready \u2713");
                inputAdminPasscodeTextView.setVisibility(View.VISIBLE);
                adminPasscodeEditLayout.setVisibility(View.VISIBLE);
              });
    } else {
      Log.i(TAG, "Found new MeshCoP-ePSKc service " + meshcopEInfo.instanceName);
    }
  }
}
