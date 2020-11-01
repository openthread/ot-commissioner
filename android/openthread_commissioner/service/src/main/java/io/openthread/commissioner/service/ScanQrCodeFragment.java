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

import android.Manifest;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.util.SparseArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import androidx.annotation.NonNull;
import androidx.annotation.RequiresPermission;
import androidx.core.app.ActivityCompat;
import androidx.fragment.app.Fragment;
import com.google.android.gms.vision.Detector;
import com.google.android.gms.vision.Detector.Detections;
import com.google.android.gms.vision.barcode.Barcode;

public class ScanQrCodeFragment extends Fragment implements Detector.Processor<Barcode> {

  private static final String TAG = ScanQrCodeFragment.class.getSimpleName();

  private static final String KEY_JOINER_EUI64 = "eui";
  private static final String KEY_JOINER_PSKD = "cc";

  // intent request code to handle updating play services if needed.
  private static final int RC_HANDLE_GMS = 9001;

  // permission request codes need to be < 256
  private static final int RC_HANDLE_CAMERA_PERM = 2;

  private CameraSourceView cameraSourceView;

  private FragmentCallback joinerInfoCallback;

  public ScanQrCodeFragment(FragmentCallback joinerInfoCallback) {
    this.joinerInfoCallback = joinerInfoCallback;
  }

  @Override
  public View onCreateView(
      LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
    // Inflate the layout for this fragment
    return inflater.inflate(R.layout.fragment_scan_qrcode, container, false);
  }

  public void onViewCreated(@NonNull View view, Bundle savedInstanceState) {
    super.onViewCreated(view, savedInstanceState);

    view.findViewById(R.id.cancel_button)
        .setOnClickListener(
            v -> {
              joinerInfoCallback.onJoinerInfoReceived(null);
            });

    cameraSourceView = view.findViewById(R.id.camera_view);
    if (hasCameraPermission()) {
      cameraSourceView.initializeBarcodeDetectorAndCamera(this);
    } else {
      requestCameraPermission();
    }
  }

  private void requestCameraPermission() {
    final String[] permissions = new String[] {Manifest.permission.CAMERA};

    requestPermissions(permissions, RC_HANDLE_CAMERA_PERM);
  }

  /** Restarts the camera. */
  @RequiresPermission(Manifest.permission.CAMERA)
  @Override
  public void onResume() {
    super.onResume();

    cameraSourceView.startCamera();
  }

  /** Stops the camera. */
  @Override
  public void onPause() {
    super.onPause();

    cameraSourceView.stopCamera();
  }

  /**
   * Releases the resources associated with the camera source, the associated detectors, and the
   * rest of the processing pipeline.
   */
  @Override
  public void onDestroy() {
    super.onDestroy();

    cameraSourceView.releaseCamera();
  }

  private boolean hasCameraPermission() {
    return ActivityCompat.checkSelfPermission(getActivity(), Manifest.permission.CAMERA)
        == PackageManager.PERMISSION_GRANTED;
  }

  @Override
  public void onRequestPermissionsResult(
      int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
    if (requestCode != RC_HANDLE_CAMERA_PERM) {
      Log.d(TAG, "Got unexpected permission result: " + requestCode);
      super.onRequestPermissionsResult(requestCode, permissions, grantResults);
      return;
    }

    if (grantResults.length != 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
      Log.d(TAG, "Camera permission granted - initialize the camera source");
      cameraSourceView.initializeBarcodeDetectorAndCamera(this);
      return;
    }

    Log.e(
        TAG,
        "Permission not granted: results len = "
            + grantResults.length
            + " Result code = "
            + (grantResults.length > 0 ? grantResults[0] : "(empty)"));

    DialogInterface.OnClickListener listener =
        new DialogInterface.OnClickListener() {
          public void onClick(DialogInterface dialog, int id) {
            joinerInfoCallback.onJoinerInfoReceived(null);
          }
        };

    AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
    builder
        .setTitle("Permission Denied")
        .setMessage("No camera permission!")
        .setPositiveButton("OK", listener)
        .show();
  }

  @Override
  public void release() {}

  @Override
  public void receiveDetections(Detections<Barcode> detections) {
    final SparseArray<Barcode> barcodes = detections.getDetectedItems();
    if (barcodes.size() == 0) {
      return;
    }

    Log.d(TAG, "barcode detected");

    Handler handler = new Handler(Looper.getMainLooper());
    handler.post(
        new Runnable() {
          @RequiresPermission(Manifest.permission.CAMERA)
          @Override
          public void run() {
            handleBarcode(barcodes.valueAt(0));
          }
        });
  }

  @RequiresPermission(Manifest.permission.CAMERA)
  private void handleBarcode(Barcode barcode) {

    // TODO(wgtdkp): handle Thread/CHIP QR code

    // Jump to next phrase (select network) if parsing succeed.
    // Otherwise, display a diag that the QR code is corrupted and ask
    // the user to retry or manually enter it.

    cameraSourceView.stopCamera();

    JoinerDeviceInfo joinerDeviceInfo = parseJoinerDeviceInfo(barcode);

    if (joinerDeviceInfo == null) {
      Log.e(TAG, "invalid QR code");
      cameraSourceView.startCamera();
    }

    joinerInfoCallback.onJoinerInfoReceived(joinerDeviceInfo);
  }

  private JoinerDeviceInfo parseJoinerDeviceInfo(Barcode barcode) {
    if (barcode.valueFormat != Barcode.TEXT) {
      Log.e(TAG, "broken QR code! expect QR code encode a URL string.");
      return null;
    }

    Uri joinerUrl = new Uri.Builder().encodedQuery(barcode.rawValue).build();
    byte[] eui64 = CommissionerUtils.getByteArray(joinerUrl.getQueryParameter(KEY_JOINER_EUI64));
    String pskd = joinerUrl.getQueryParameter(KEY_JOINER_PSKD);

    if (eui64 == null || pskd == null) {
      // TODO(wgtdkp): Broken QR code, ask the user to input manually.
      Log.e(
          TAG,
          String.format(
              "broken QR code! expect %s and %s in URL parameters.",
              KEY_JOINER_EUI64, KEY_JOINER_PSKD));
      return null;
    } else {
      return new JoinerDeviceInfo(eui64, pskd);
    }
  }
}
