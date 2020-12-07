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

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.TextView;
import io.openthread.commissioner.service.BorderAgentDiscoverer.BorderAgentListener;
import java.util.Arrays;
import java.util.Vector;

public class NetworkAdapter extends BaseAdapter implements BorderAgentListener {
  private Vector<ThreadNetworkInfoHolder> networks;

  private LayoutInflater inflater;

  NetworkAdapter(Context context) {
    inflater = (LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
    networks = new Vector<>();
  }

  public void addBorderAgent(BorderAgentInfo borderAgent) {
    boolean hasExistingNetwork = false;
    for (ThreadNetworkInfoHolder networkInfoHolder : networks) {
      if (networkInfoHolder.getNetworkInfo().getNetworkName().equals(borderAgent.networkName)
          && Arrays.equals(
              networkInfoHolder.getNetworkInfo().getExtendedPanId(), borderAgent.extendedPanId)) {
        networkInfoHolder.getBorderAgents().add(borderAgent);
        hasExistingNetwork = true;
      }
    }

    if (!hasExistingNetwork) {
      networks.add(new ThreadNetworkInfoHolder(borderAgent));
    }

    new Handler(Looper.getMainLooper()).post(() -> notifyDataSetChanged());
  }

  public void removeBorderAgent(String lostBorderAgentDisciminator) {
    for (ThreadNetworkInfoHolder networkInfoHolder : networks) {
      for (BorderAgentInfo borderAgent : networkInfoHolder.getBorderAgents()) {
        if (borderAgent.discriminator.equals(lostBorderAgentDisciminator)) {
          networkInfoHolder.getBorderAgents().remove(borderAgent);
          if (networkInfoHolder.getBorderAgents().isEmpty()) {
            networks.remove(networkInfoHolder);
          }

          new Handler(Looper.getMainLooper()).post(() -> notifyDataSetChanged());
          return;
        }
      }
    }
  }

  @Override
  public int getCount() {
    return networks.size();
  }

  @Override
  public Object getItem(int position) {
    return networks.get(position);
  }

  @Override
  public long getItemId(int position) {
    return position;
  }

  @Override
  public View getView(int position, View convertView, ViewGroup container) {
    if (convertView == null) {
      convertView = inflater.inflate(R.layout.network_list_item, container, false);
    }
    TextView networkNameText = convertView.findViewById(R.id.network_name);
    networkNameText.setText(networks.get(position).getNetworkInfo().getNetworkName());

    TextView borderAgentIpAddrText = convertView.findViewById(R.id.border_agent_ip_addr);
    borderAgentIpAddrText.setText(
        networks.get(position).getBorderAgents().get(0).host.getHostAddress());
    return convertView;
  }

  @Override
  public void onBorderAgentFound(BorderAgentInfo borderAgentInfo) {
    addBorderAgent(borderAgentInfo);
  }

  @Override
  public void onBorderAgentLost(String discriminator) {
    removeBorderAgent(discriminator);
  }
}
