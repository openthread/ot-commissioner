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
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;
import androidx.annotation.NonNull;
import androidx.core.content.ContextCompat;
import androidx.fragment.app.Fragment;
import com.google.common.util.concurrent.FluentFuture;
import com.google.common.util.concurrent.FutureCallback;
import com.google.common.util.concurrent.ListenableFuture;

public class CommissioningFragment extends Fragment
    implements ThreadCommissionerServiceImpl.IntermediateStateCallback {

  private static final String TAG = CommissioningFragment.class.getSimpleName();

  TextView statusText;
  ProgressBar progressBar;
  Button cancelButton;
  Button doneButton;
  ImageView doneImage;
  ImageView errorImage;

  private final FragmentCallback fragmentCallback;
  private final ThreadNetworkInfoHolder networkInfoHolder;
  private final byte[] pskc;
  private final JoinerDeviceInfo joinerDeviceInfo;

  private ThreadCommissionerServiceImpl commissionerService;
  private ListenableFuture<Void> commissionFuture;

  public CommissioningFragment(
      FragmentCallback fragmentCallback,
      ThreadNetworkInfoHolder networkInfoHolder,
      byte[] pskc,
      JoinerDeviceInfo joinerDeviceInfo) {
    this.fragmentCallback = fragmentCallback;
    this.networkInfoHolder = networkInfoHolder;
    this.pskc = pskc;
    this.joinerDeviceInfo = joinerDeviceInfo;
  }

  @Override
  public View onCreateView(
      LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
    return inflater.inflate(R.layout.fragment_meshcop, container, false);
  }

  @Override
  public void onViewCreated(@NonNull View view, Bundle savedInstanceState) {
    super.onViewCreated(view, savedInstanceState);

    if (commissionerService == null) {
      commissionerService = ThreadCommissionerServiceImpl.newInstance(requireActivity(), this);
    }

    cancelButton = view.findViewById(R.id.cancel_button);
    doneButton = view.findViewById(R.id.done_button);
    doneImage = view.findViewById(R.id.done_image);
    errorImage = view.findViewById(R.id.error_image);
    statusText = view.findViewById(R.id.status_text);
    progressBar = view.findViewById(R.id.commissioning_progress);

    view.findViewById(R.id.cancel_button)
        .setOnClickListener(
            v -> {
              fragmentCallback.onAddDeviceResult(Activity.RESULT_CANCELED);
            });

    view.findViewById(R.id.done_button)
        .setOnClickListener(
            v -> {
              fragmentCallback.onAddDeviceResult(Activity.RESULT_OK);
            });
  }

  @Override
  public void onStart() {
    super.onStart();
    startMeshcop();
  }

  @Override
  public void onDestroy() {
    super.onDestroy();
    stopMeshcop();
  }

  private void startMeshcop() {
    BorderAgentInfo borderAgentInfo = networkInfoHolder.getBorderAgents().get(0);

    showInProgress("Petitioning...");

    commissionFuture =
        commissionerService.commissionJoinerDevice(borderAgentInfo, pskc, joinerDeviceInfo);
    FluentFuture.from(commissionFuture)
        .addCallback(
            new FutureCallback<Void>() {
              @Override
              public void onSuccess(Void result) {
                showCommissionDone(true, "Commission Succeed");
              }

              @Override
              public void onFailure(Throwable t) {
                showCommissionDone(false, t.getMessage());
              }
            },
            ContextCompat.getMainExecutor(getActivity()));
  }

  private void stopMeshcop() {
    if (commissionFuture != null) {
      commissionFuture.cancel(true);
      commissionFuture = null;
    }
  }

  private void showInProgress(String status) {
    if (status != null) {
      statusText.setText(status);
    }

    progressBar.setVisibility(View.VISIBLE);

    cancelButton.setVisibility(View.VISIBLE);
    doneImage.setVisibility(View.GONE);
    errorImage.setVisibility(View.GONE);
    doneButton.setVisibility(View.GONE);
  }

  private void showCommissionDone(Boolean success, String status) {
    if (status != null) {
      statusText.setText(status);
    }

    progressBar.setVisibility(View.GONE);
    cancelButton.setVisibility(View.GONE);
    doneButton.setVisibility(View.VISIBLE);

    if (success) {
      doneImage.setVisibility(View.VISIBLE);
      errorImage.setVisibility(View.GONE);
    } else {
      doneImage.setVisibility(View.GONE);
      errorImage.setVisibility(View.VISIBLE);
    }
  }

  @Override
  public void onPetitioned() {
    new Handler(Looper.getMainLooper())
        .post(
            () -> {
              showInProgress("Petition Done!\nWating for Joiner Device...");
            });
  }

  @Override
  public void onJoinerRequest(@NonNull byte[] joinerId) {
    new Handler(Looper.getMainLooper())
        .post(
            () -> {
              showInProgress(
                  String.format(
                      "New Device (ID=%s) is joining!", CommissionerUtils.getHexString(joinerId)));
            });
  }
}
