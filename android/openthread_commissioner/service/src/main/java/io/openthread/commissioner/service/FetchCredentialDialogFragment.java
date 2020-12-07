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

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;
import androidx.annotation.NonNull;
import androidx.fragment.app.DialogFragment;

public class FetchCredentialDialogFragment extends DialogFragment
    implements DialogInterface.OnClickListener {

  private CredentialListener credentialListener;
  private TextView statusText;

  private AlertDialog dialog;

  BorderAgentInfo borderAgentInfo;
  byte[] pskc;
  private ThreadNetworkCredential credential;

  public interface CredentialListener {
    void onCancelClick(FetchCredentialDialogFragment fragment);

    void onConfirmClick(FetchCredentialDialogFragment fragment, ThreadNetworkCredential credential);
  }

  public FetchCredentialDialogFragment(
      @NonNull BorderAgentInfo borderAgentInfo,
      @NonNull byte[] pskc,
      @NonNull CredentialListener credentialListener) {
    this.borderAgentInfo = borderAgentInfo;
    this.pskc = pskc;
    this.credentialListener = credentialListener;
  }

  @Override
  public Dialog onCreateDialog(Bundle savedInstanceState) {
    AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
    LayoutInflater inflater = requireActivity().getLayoutInflater();
    View view = inflater.inflate(R.layout.fragment_fetch_credential_dialog, null);

    statusText = view.findViewById(R.id.fetch_credential_status);

    builder.setTitle("Fetching Credential");
    builder.setView(view);
    builder.setPositiveButton(R.string.fetch_credential_done, this);
    builder.setNegativeButton(R.string.fetch_credential_cancel, this);

    dialog = builder.create();
    return dialog;
  }

  @Override
  public void onStart() {
    super.onStart();

    dialog.getButton(AlertDialog.BUTTON_POSITIVE).setEnabled(false);

    // TODO(wgtdkp):

    startFetching();
  }

  @Override
  public void onStop() {
    super.onStop();

    stopFetching();
  }

  @Override
  public void onClick(DialogInterface dialogInterface, int which) {
    if (which == DialogInterface.BUTTON_POSITIVE) {
      credentialListener.onConfirmClick(this, credential);
    } else {
      stopFetching();
      credentialListener.onCancelClick(this);
    }
  }

  private void startFetching() {
    Thread thread =
        new Thread(
            () -> {
              new Handler(Looper.getMainLooper())
                  .post(
                      () -> {
                        statusText.setText("petitioning...");
                      });

              String status;

              // TODO(wgtdkp):
              final String statusCopy = "";

              new Handler(Looper.getMainLooper())
                  .post(
                      () -> {
                        statusText.setText(statusCopy);
                        dialog.getButton(AlertDialog.BUTTON_POSITIVE).setEnabled(true);
                      });
            });
    thread.start();
  }

  private void stopFetching() {
    // TODO(wgtdkp):
  }
}
