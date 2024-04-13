package io.openthread.commissioner.app;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.util.ArrayMap;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.TextView;
import androidx.annotation.Nullable;
import io.openthread.commissioner.app.BorderAgentDiscoverer.BorderAgentListener;
import java.net.Inet6Address;
import java.util.Map;
import java.util.Vector;

public class BorderAgentAdapter extends BaseAdapter implements BorderAgentListener {

  private final Vector<BorderAgentInfo> borderAgentServices = new Vector<>();
  private final Map<String, BorderAgentInfo> epskcServices = new ArrayMap<>();

  private final LayoutInflater inflater;

  BorderAgentAdapter(Context context) {
    inflater = (LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
  }

  public void addBorderAgent(BorderAgentInfo serviceInfo) {
    if (serviceInfo.isEpskcService) {
      epskcServices.put(serviceInfo.instanceName, serviceInfo);
      notifyDataSetChanged();
      return;
    }

    boolean hasExistingBorderRouter = false;
    for (int i = 0; i < borderAgentServices.size(); i++) {
      if (borderAgentServices.get(i).instanceName.equals(serviceInfo.instanceName)) {
        borderAgentServices.set(i, serviceInfo);
        hasExistingBorderRouter = true;
      }
    }

    if (!hasExistingBorderRouter) {
      borderAgentServices.add(serviceInfo);
    }

    notifyDataSetChanged();
  }

  public void removeBorderAgent(boolean isEpskcService, String instanceName) {
    if (isEpskcService) {
      epskcServices.remove(instanceName);
    } else {
      borderAgentServices.removeIf(serviceInfo -> serviceInfo.instanceName.equals(instanceName));
    }
    notifyDataSetChanged();
  }

  public void clear() {
    borderAgentServices.clear();
    epskcServices.clear();
    notifyDataSetChanged();
  }

  @Override
  public int getCount() {
    return borderAgentServices.size();
  }

  @Override
  public Object getItem(int position) {
    return borderAgentServices.get(position);
  }

  @Override
  public long getItemId(int position) {
    return position;
  }

  @Override
  public View getView(int position, View convertView, ViewGroup container) {
    if (convertView == null) {
      convertView = inflater.inflate(R.layout.border_agent_list_item, container, false);
    }

    BorderAgentInfo borderAgentInfo = borderAgentServices.get(position);

    TextView instanceNameText = convertView.findViewById(R.id.border_agent_instance_name);
    instanceNameText.setText(borderAgentInfo.instanceName);

    TextView vendorNameText = convertView.findViewById(R.id.border_agent_vendor_name);
    vendorNameText.setText(borderAgentInfo.vendorName);

    TextView modelNameText = convertView.findViewById(R.id.border_agent_model_name);
    modelNameText.setText(borderAgentInfo.modelName);

    TextView adminModeText = convertView.findViewById(R.id.border_agent_admin_mode);
    adminModeText.setText(
        "In Administration Mode: " + (inAdministrationMode(borderAgentInfo) ? "YES" : "NO"));

    TextView borderAgentIpAddrText = convertView.findViewById(R.id.border_agent_ip_addr);
    int port =
        inAdministrationMode(borderAgentInfo)
            ? getEpskcService(borderAgentInfo).port
            : borderAgentInfo.port;
    String socketAddress;
    if (borderAgentInfo.host instanceof Inet6Address) {
      socketAddress = "[" + borderAgentInfo.host.getHostAddress() + "]:" + port;
    } else {
      socketAddress = borderAgentInfo.host.getHostAddress() + ":" + port;
    }
    borderAgentIpAddrText.setText(socketAddress);
    return convertView;
  }

  private boolean inAdministrationMode(BorderAgentInfo borderAgentInfo) {
    return epskcServices.containsKey(borderAgentInfo.instanceName);
  }

  @Override
  public void onBorderAgentFound(BorderAgentInfo borderAgentInfo) {
    new Handler(Looper.getMainLooper()).post(() -> addBorderAgent(borderAgentInfo));
  }

  @Override
  public void onBorderAgentLost(boolean isEpskcService, String instanceName) {
    new Handler(Looper.getMainLooper()).post(() -> removeBorderAgent(isEpskcService, instanceName));
  }

  /**
   * Returns the _meshcop-e._udp service which is associated with the given _meshcop._udp service,
   * or {@code null} if such service doesn't exist.
   */
  @Nullable
  private BorderAgentInfo getEpskcService(BorderAgentInfo meshcopService) {
    return epskcServices.get(meshcopService.instanceName);
  }
}
