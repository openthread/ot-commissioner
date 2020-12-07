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
import android.content.Context;
import android.util.AttributeSet;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.ViewGroup;
import androidx.annotation.RequiresPermission;
import com.google.android.gms.vision.CameraSource;
import com.google.android.gms.vision.Detector;
import com.google.android.gms.vision.barcode.Barcode;
import com.google.android.gms.vision.barcode.BarcodeDetector;
import java.io.IOException;

public class CameraSourceView extends ViewGroup {

  private static final String TAG = CameraSourceView.class.getSimpleName();

  private SurfaceView surfaceView;
  private boolean cameraStarted = false;
  private boolean surfaceAvailable;

  Context context;
  private CameraSource cameraSource;
  private BarcodeDetector barcodeDetector;

  public CameraSourceView(Context context, AttributeSet attrs) {
    super(context, attrs);

    this.context = context;
    surfaceView = new SurfaceView(context);
    surfaceView.getHolder().addCallback(new SurfaceCallback());
    addView(surfaceView);
  }

  public void initializeBarcodeDetectorAndCamera(Detector.Processor<Barcode> barcodeProcessor) {

    barcodeDetector = new BarcodeDetector.Builder(context).build();
    barcodeDetector.setProcessor(barcodeProcessor);

    cameraSource =
        new CameraSource.Builder(context, barcodeDetector)
            .setFacing(CameraSource.CAMERA_FACING_BACK)
            .setRequestedPreviewSize(1600, 1024)
            .setRequestedFps(30.0f)
            .setAutoFocusEnabled(true)
            .build();
  }

  /**
   * Attempts to start the camera source.
   *
   * @throws IOException if the camera doesn't work
   * @throws SecurityException if app doesn't have permission to access camera
   */
  @RequiresPermission(Manifest.permission.CAMERA)
  public void startCamera() {
    try {
      if (cameraSource != null && !cameraStarted && surfaceAvailable) {
        cameraSource.start(surfaceView.getHolder());
        cameraStarted = true;
      }
    } catch (SecurityException se) {
      Log.e(TAG, "Do not have permission to start the camera", se);
    } catch (IOException ioe) {
      Log.e(TAG, "Could not start camera source.", ioe);
    }
  }

  /** Stops the camera source. */
  public void stopCamera() {
    if (cameraSource != null && cameraStarted) {
      cameraSource.stop();
      cameraStarted = false;
    }
  }

  /** Releases the camera source. */
  public void releaseCamera() {
    if (cameraSource != null) {
      cameraSource.release();
      cameraStarted = false;
      cameraSource = null;
    }
  }

  @RequiresPermission(Manifest.permission.CAMERA)
  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    final int layoutWidth = right - left;
    final int layoutHeight = bottom - top;

    for (int position = 0; position < getChildCount(); ++position) {
      getChildAt(position).layout(0, 0, layoutWidth, layoutHeight);
    }

    startCamera();
  }

  /** Class to handle surface traits and availability */
  private class SurfaceCallback implements SurfaceHolder.Callback {

    @RequiresPermission(Manifest.permission.CAMERA)
    @Override
    public void surfaceCreated(SurfaceHolder surface) {
      surfaceAvailable = true;
      startCamera();
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder surface) {
      surfaceAvailable = false;
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {}
  }
}
