package io.openthread.commissioner.app;

import static com.google.common.io.BaseEncoding.base16;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import com.google.android.gms.tasks.Task;
import com.google.android.gms.tasks.Tasks;
import com.google.android.gms.threadnetwork.ThreadBorderAgent;
import com.google.android.gms.threadnetwork.ThreadNetwork;
import com.google.android.gms.threadnetwork.ThreadNetworkCredentials;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.common.primitives.Bytes;
import io.openthread.commissioner.ByteArray;
import io.openthread.commissioner.Commissioner;
import io.openthread.commissioner.CommissionerHandler;
import io.openthread.commissioner.Config;
import io.openthread.commissioner.Error;
import io.openthread.commissioner.ErrorCode;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Retrieves Thread Active Operational Dataset from a Border Router device and save it into Google
 * Play Services.
 */
public class RetrieveDatasetFragment extends Fragment {
  private static final String TAG = GetAdminPasscodeFragment.class.getSimpleName();

  private final FragmentCallback fragmentCallback;
  private final BorderAgentInfo borderAgentInfo;
  private final String passcode;
  private final int epskcPort;

  private TextView connectStatusText;
  private TextView retrieveDatasetStatusText;
  private TextView saveDatasetStatusText;

  private Button showDatasetButton;

  @Nullable private ThreadNetworkCredentials retrievedDataset;

  private ExecutorService backgroundExecutor = Executors.newSingleThreadExecutor();

  private CommissionerHandler commissionerHandler =
      new CommissionerHandler() {
        // FIXME(wgtdkp): we need to provide an override to the JNI callback, otherwise, it will
        // crash
        @Override
        public void onKeepAliveResponse(Error error) {
          Log.d(TAG, "received keep-alive response: " + error.toString());
        }
      };

  public RetrieveDatasetFragment(
      FragmentCallback fragmentCallback,
      BorderAgentInfo borderAgentInfo,
      String passcode,
      int epskcPort) {
    this.fragmentCallback = fragmentCallback;
    this.borderAgentInfo = borderAgentInfo;
    this.passcode = passcode;
    this.epskcPort = epskcPort;
  }

  @Override
  public View onCreateView(
      LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
    // Inflate the layout for this fragment
    return inflater.inflate(R.layout.fragment_retrieve_dataset, container, false);
  }

  public void onViewCreated(@NonNull View view, Bundle savedInstanceState) {
    super.onViewCreated(view, savedInstanceState);

    connectStatusText = view.findViewById(R.id.connected_to_br_text);
    connectStatusText.setText("Connecting to " + borderAgentInfo.instanceName + "...");

    retrieveDatasetStatusText = view.findViewById(R.id.dataset_retrieved_text);
    retrieveDatasetStatusText.setVisibility(View.INVISIBLE);

    saveDatasetStatusText = view.findViewById(R.id.dataset_saved_text);
    saveDatasetStatusText.setVisibility(View.INVISIBLE);

    showDatasetButton = view.findViewById(R.id.show_dataset_button);
    showDatasetButton.setVisibility(View.INVISIBLE);
    showDatasetButton.setOnClickListener(v -> showDataset());

    view.findViewById(R.id.done_button)
        .setOnClickListener(v -> fragmentCallback.onRetrieveDatasetResult(Activity.RESULT_OK));

    backgroundExecutor.execute(this::retrieveDataset);
  }

  private void retrieveDataset() {
    // Create a commissioner candidate

    Commissioner commissioner = Commissioner.create(commissionerHandler);
    Config config = new Config();
    config.setId("ePSKc demo");

    if (passcode.getBytes(StandardCharsets.UTF_8).length != 9) {
      throw new IllegalStateException();
    }

    config.setPSKc(new ByteArray(passcode.getBytes()));
    config.setLogger(new NativeCommissionerLogger());
    config.setEnableCcm(false);
    config.setEnableDtlsDebugLogging(true);

    Error error = commissioner.init(config);
    if (error.getCode() != ErrorCode.kNone) {
      fail("Failed to create commissioner: " + error.toString());
      return;
    }

    // Connect to the Border Router

    String[] existingCommissionerId = new String[1];
    error =
        commissioner.petition(
            existingCommissionerId, borderAgentInfo.host.getHostAddress(), epskcPort);
    if (error.getCode() != ErrorCode.kNone) {
      fail("Failed to connect to Border Router: " + error.toString());
      return;
    }

    getActivity().runOnUiThread(() -> connectStatusText.setText("Border Router connected \u2713"));

    // Retrieve Active Dataset

    ByteArray datasetTlvs = new ByteArray();
    error = commissioner.getRawActiveDataset(datasetTlvs, 0xffff);
    if (error.getCode() != ErrorCode.kNone) {
      fail("Failed to retrieve Thread credentials: " + error.toString());
      return;
    }

    retrievedDataset =
        ThreadNetworkCredentials.fromActiveOperationalDataset(Bytes.toArray(datasetTlvs));

    getActivity()
        .runOnUiThread(
            () -> {
              retrieveDatasetStatusText.setVisibility(View.VISIBLE);
              showDatasetButton.setVisibility(View.VISIBLE);
            });

    // Save dataset to Google Play services

    Task<Void> task =
        ThreadNetwork.getClient(getActivity())
            .addCredentials(
                ThreadBorderAgent.newBuilder(borderAgentInfo.id).build(), retrievedDataset);

    try {
      Tasks.await(task);
    } catch (ExecutionException e) {
      throw new RuntimeException(e);
    } catch (InterruptedException e) {
      throw new RuntimeException(e);
    } finally {
      commissioner.resign();
    }

    getActivity()
        .runOnUiThread(
            () -> {
              saveDatasetStatusText.setVisibility(View.VISIBLE);
            });
  }

  private void showDataset() {
    if (retrievedDataset == null) {
      return;
    }

    String message =
        "Network Name: "
            + retrievedDataset.getNetworkName()
            + "\n"
            + "Extended PAN ID: "
            + base16().encode(retrievedDataset.getExtendedPanId())
            + "\n"
            + "PAN ID: "
            + retrievedDataset.getPanId()
            + "\n"
            + "Channel: "
            + retrievedDataset.getChannel()
            + "\n"
            + "Network Key: "
            + base16().encode(retrievedDataset.getNetworkKey())
            + "\n"
            + "PSKc: "
            + base16().encode(retrievedDataset.getPskc())
            + "\n"
            + "Mesh-local Prefix: "
            + base16().encode(retrievedDataset.getMeshLocalPrefix())
            + "\n"
            + "Raw TLV: "
            + base16().encode(retrievedDataset.getActiveOperationalDataset());

    new MaterialAlertDialogBuilder(getActivity())
        .setTitle("Thread Dataset")
        .setMessage(message)
        .setPositiveButton("OK", (dialog, which) -> {})
        .show();
  }

  private void fail(String errorMessage) {
    getActivity().runOnUiThread(() -> showAlertDialog(errorMessage));
  }

  private void showAlertDialog(String message) {
    new MaterialAlertDialogBuilder(getActivity(), R.style.ThreadNetworkAlertTheme)
        .setMessage(message)
        .setPositiveButton(
            "OK",
            (dialog, which) -> fragmentCallback.onRetrieveDatasetResult(Activity.RESULT_CANCELED))
        .show();
  }
}
