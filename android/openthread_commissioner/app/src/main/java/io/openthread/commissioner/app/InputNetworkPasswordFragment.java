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
import android.app.AlertDialog;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import io.openthread.commissioner.ByteArray;
import io.openthread.commissioner.Commissioner;
import io.openthread.commissioner.Error;
import io.openthread.commissioner.ErrorCode;

public class InputNetworkPasswordFragment extends Fragment {
  private static final String TAG = InputNetworkPasswordFragment.class.getSimpleName();

  private final FragmentCallback fragmentCallback;
  private final BorderAgentInfo selectedBorderAgent;
  private EditText passwordText;

  public InputNetworkPasswordFragment(
      FragmentCallback fragmentCallback, BorderAgentInfo selectedBorderAgent) {
    this.fragmentCallback = fragmentCallback;
    this.selectedBorderAgent = selectedBorderAgent;
  }

  @Override
  public View onCreateView(
      LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
    return inflater.inflate(
        R.layout.fragment_input_network_password, container, /* attachToRoot= */ false);
  }

  @Override
  public void onViewCreated(@NonNull View view, Bundle savedInstanceState) {
    super.onViewCreated(view, savedInstanceState);

    LayoutInflater inflater = requireActivity().getLayoutInflater();
    View dialogView = inflater.inflate(R.layout.fragment_network_password_dialog, null);

    passwordText = dialogView.findViewById(R.id.network_password);

    new AlertDialog.Builder(requireActivity())
        .setTitle("Enter Password")
        .setView(dialogView)
        .setPositiveButton(R.string.password_connect, (dialog, which) -> onPasswordInputted())
        .setNegativeButton(
            R.string.password_cancel,
            (dialog, which) -> fragmentCallback.onAddDeviceResult(Activity.RESULT_CANCELED))
        .create()
        .show();
  }

  private void onPasswordInputted() {
    byte[] pskc =
        computePskc(
            selectedBorderAgent.networkName,
            selectedBorderAgent.extendedPanId,
            passwordText.getText().toString());

    if (pskc == null) {
      FragmentUtils.showAlertAndExit(
          getActivity(), fragmentCallback, "Bad Password", "Cannot compute PSKc!");
      return;
    }

    FragmentUtils.moveToNextFragment(
        this, new ScanQrCodeFragment(fragmentCallback, selectedBorderAgent, pskc));
  }

  private byte[] computePskc(String networkName, byte[] extendedPanId, String password) {
    ByteArray pskc = new ByteArray();
    Error error =
        Commissioner.generatePSKc(pskc, password, networkName, new ByteArray(extendedPanId));
    if (error.getCode() != ErrorCode.kNone) {
      Log.e(
          TAG,
          String.format(
              "failed to generate PSKc: %s; network-name=%s, extended-pan-id=%s",
              error, networkName, CommissionerUtils.getHexString(extendedPanId)));
      return null;
    }

    Log.d(
        TAG,
        String.format(
            "generated pskc=%s, network-name=%s, extended-pan-id=%s",
            CommissionerUtils.getHexString(pskc),
            networkName,
            CommissionerUtils.getHexString(extendedPanId)));
    return CommissionerUtils.getByteArray(pskc);
  }
}
