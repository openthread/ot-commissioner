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

import android.Manifest.permission;
import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.nsd.NsdManager;
import android.net.nsd.NsdServiceInfo;
import android.net.wifi.WifiManager;
import android.util.Log;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresPermission;
import com.google.common.net.InetAddresses;
import java.net.Inet6Address;
import java.net.InetAddress;
import java.util.Map;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;

public class BorderAgentDiscoverer implements NsdManager.DiscoveryListener {

  private static final String TAG = BorderAgentDiscoverer.class.getSimpleName();

  private static final String SERVICE_TYPE = "_meshcop._udp";
  private static final String KEY_ID = "id";
  private static final String KEY_NETWORK_NAME = "nn";
  private static final String KEY_EXTENDED_PAN_ID = "xp";

  private final WifiManager.MulticastLock wifiMulticastLock;
  private final NsdManager nsdManager;
  private final ConnectivityManager connManager;
  private final BorderAgentListener borderAgentListener;

  private ExecutorService executor = Executors.newSingleThreadExecutor();
  private final BlockingQueue<NsdServiceInfo> unresolvedServices = new ArrayBlockingQueue<>(256);
  private final AtomicBoolean isResolvingService = new AtomicBoolean(false);

  private boolean isScanning = false;

  public interface BorderAgentListener {

    void onBorderAgentFound(BorderAgentInfo borderAgentInfo);

    void onBorderAgentLost(byte[] id);
  }

  @RequiresPermission(permission.INTERNET)
  public BorderAgentDiscoverer(Context context, BorderAgentListener borderAgentListener) {
    WifiManager wifi = (WifiManager) context.getSystemService(Context.WIFI_SERVICE);
    wifiMulticastLock = wifi.createMulticastLock("multicastLock");

    nsdManager = context.getSystemService(NsdManager.class);
    connManager = context.getSystemService(ConnectivityManager.class);

    this.borderAgentListener = borderAgentListener;
  }

  public void start() {
    if (isScanning) {
      Log.w(TAG, "the Border Agent discoverer is already running!");
      return;
    }

    isScanning = true;

    wifiMulticastLock.setReferenceCounted(true);
    wifiMulticastLock.acquire();

    startResolver();
    nsdManager.discoverServices(
        BorderAgentDiscoverer.SERVICE_TYPE, NsdManager.PROTOCOL_DNS_SD, this);
  }

  private void startResolver() {
    NsdManager.ResolveListener listener =
        new NsdManager.ResolveListener() {
          @Override
          public void onResolveFailed(NsdServiceInfo serviceInfo, int errorCode) {
            Log.e(
                TAG,
                String.format(
                    "failed to resolve service %s, error: %d, this=%s",
                    serviceInfo.toString(), errorCode, this));
            isResolvingService.set(false);
          }

          @Override
          public void onServiceResolved(NsdServiceInfo serviceInfo) {
            BorderAgentInfo borderAgent = getBorderAgentInfo(serviceInfo);
            if (borderAgent != null) {
              Log.d(TAG, "successfully resolved service: " + serviceInfo);
              Log.d(
                  TAG,
                  "successfully resolved service: " + serviceInfo.getHost().getCanonicalHostName());
              borderAgentListener.onBorderAgentFound(borderAgent);
            }
            isResolvingService.set(false);
          }
        };

    Log.d(TAG, "mDNS resolve listener is " + listener);

    if (executor.isTerminated()) {
      executor = Executors.newSingleThreadExecutor();
    }

    executor.submit(
        () -> {
          while (true) {
            if (!isResolvingService.get()) {
              NsdServiceInfo serviceInfo = unresolvedServices.take();

              isResolvingService.set(true);
              nsdManager.resolveService(serviceInfo, listener);
            }
          }
        });
  }

  private void stopResolver() {
    if (!executor.isTerminated()) {
      executor.shutdownNow();
    }
    isResolvingService.set(false);
    unresolvedServices.clear();
  }

  public void stop() {
    if (!isScanning) {
      Log.w(TAG, "the Border Agent discoverer has already been stopped!");
      return;
    }

    nsdManager.stopServiceDiscovery(this);
    stopResolver();

    if (wifiMulticastLock != null) {
      wifiMulticastLock.release();
    }

    isScanning = false;
  }

  @Override
  public void onDiscoveryStarted(String serviceType) {
    Log.d(TAG, "start discovering Border Agent");
  }

  @Override
  public void onDiscoveryStopped(String serviceType) {
    Log.d(TAG, "stop discovering Border Agent");
  }

  @Override
  public void onServiceFound(NsdServiceInfo nsdServiceInfo) {
    Log.d(TAG, "a Border Agent service found");

    unresolvedServices.offer(nsdServiceInfo);
  }

  @Override
  public void onServiceLost(NsdServiceInfo nsdServiceInfo) {
    byte[] id = getBorderAgentId(nsdServiceInfo);
    if (id != null) {
      Log.d(TAG, "a Border Agent service is gone: " + nsdServiceInfo.getServiceName());
      borderAgentListener.onBorderAgentLost(id);
    }
  }

  @Override
  public void onStartDiscoveryFailed(String serviceType, int errorCode) {
    Log.d(TAG, "start discovering Border Agent failed: " + errorCode);
  }

  @Override
  public void onStopDiscoveryFailed(String serviceType, int errorCode) {
    Log.d(TAG, "stop discovering Border Agent failed: " + errorCode);
  }

  @Nullable
  private BorderAgentInfo getBorderAgentInfo(NsdServiceInfo serviceInfo) {
    Map<String, byte[]> attrs = serviceInfo.getAttributes();
    byte[] id = getBorderAgentId(serviceInfo);

    if (!attrs.containsKey(KEY_NETWORK_NAME) || !attrs.containsKey(KEY_EXTENDED_PAN_ID)) {
      return null;
    }

    return new BorderAgentInfo(
        id,
        new String(attrs.get(KEY_NETWORK_NAME)),
        attrs.get(KEY_EXTENDED_PAN_ID),
        handleNsdServiceAddress(serviceInfo.getHost()),
        serviceInfo.getPort());
  }

  /**
   * Properly handles the {@link InetAddress} within a discovered {@link NsdServiceInfo}.
   *
   * <p>For example, adds the scope ID to an IPv6 link-local address to make is usable.
   */
  private InetAddress handleNsdServiceAddress(InetAddress address) {
    if (!(address instanceof Inet6Address)) {
      return address;
    }

    Inet6Address address6 = (Inet6Address) address;

    if (!address6.isLinkLocalAddress() || address6.getScopeId() != 0) {
      return address6;
    }

    // Sets the scope ID for IPv6 link-local address if it's missing. This can
    // happen before Android U.

    // Assume the mDNS service is discovered on the current active default network
    Network network = connManager.getActiveNetwork();
    if (network == null) {
      return address6;
    }
    String interfaceName = connManager.getLinkProperties(network).getInterfaceName();
    if (interfaceName == null) {
      return address6;
    }

    return InetAddresses.forString(address6.getHostAddress() + "%" + interfaceName);
  }

  @Nullable
  private byte[] getBorderAgentId(NsdServiceInfo serviceInfo) {
    Map<String, byte[]> attrs = serviceInfo.getAttributes();
    if (attrs.containsKey(KEY_ID)) {
      return attrs.get(KEY_ID).clone();
    }
    return null;
  }
}
