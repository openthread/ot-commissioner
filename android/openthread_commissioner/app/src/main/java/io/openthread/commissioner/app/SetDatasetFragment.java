package io.openthread.commissioner.app;

import android.app.Activity;
import android.content.IntentSender;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.IntentSenderRequest;
import androidx.activity.result.contract.ActivityResultContracts.StartIntentSenderForResult;
import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import com.google.android.gms.threadnetwork.ThreadNetwork;
import com.google.android.gms.threadnetwork.ThreadNetworkCredentials;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.common.base.Preconditions;
import com.google.common.primitives.Bytes;
import io.openthread.commissioner.ByteArray;
import io.openthread.commissioner.Commissioner;
import io.openthread.commissioner.CommissionerHandler;
import io.openthread.commissioner.Config;
import io.openthread.commissioner.Error;
import io.openthread.commissioner.ErrorCode;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.time.Instant;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Retrieves Thread Active Operational Dataset from a Border Router device and save it into Google
 * Play Services.
 */
public class SetDatasetFragment extends Fragment {

  private static final String TAG = GetAdminPasscodeFragment.class.getSimpleName();

  private final FragmentCallback fragmentCallback;
  private final BorderAgentInfo borderAgentInfo;
  private final String passcode;
  private final int epskcPort;

  private Button usePreferredButton;

  private TextView getPreferredStatusText;
  private TextView connectStatusText;
  private TextView setDatasetStatusText;
  private TextView migrationNoteText;
  private Button doneButton;

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

  private final ActivityResultLauncher<IntentSenderRequest> preferredCredentialsLauncher =
      registerForActivityResult(
          new StartIntentSenderForResult(),
          result -> {
            int resultCode = result.getResultCode();
            if (resultCode == Activity.RESULT_OK) {
              ThreadNetworkCredentials credentials =
                  ThreadNetworkCredentials.fromIntentSenderResultData(
                      Preconditions.checkNotNull(result.getData()));
              getPreferredStatusText.setText("Preferred credentials loaded \u2713");
              backgroundExecutor.execute(() -> setDataset(credentials));
            } else {
              getPreferredStatusText.setText("Denied to share!");
              doneButton.setVisibility(View.VISIBLE);
            }
          });

  public SetDatasetFragment(
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
    return inflater.inflate(R.layout.fragment_set_dataset, container, false);
  }

  public void onViewCreated(@NonNull View view, Bundle savedInstanceState) {
    super.onViewCreated(view, savedInstanceState);

    usePreferredButton = view.findViewById(R.id.use_preferred_button);
    usePreferredButton.setOnClickListener(this::getPreferredCredentials);

    getPreferredStatusText = view.findViewById(R.id.get_preferred_text);
    getPreferredStatusText.setVisibility(View.INVISIBLE);

    connectStatusText = view.findViewById(R.id.connected_to_br_text);
    connectStatusText.setText("Connecting to " + borderAgentInfo.instanceName + "...");
    connectStatusText.setVisibility(View.INVISIBLE);

    setDatasetStatusText = view.findViewById(R.id.set_dataset_text);
    setDatasetStatusText.setVisibility(View.INVISIBLE);

    migrationNoteText = view.findViewById(R.id.network_migration_text);
    migrationNoteText.setVisibility(View.INVISIBLE);

    doneButton = view.findViewById(R.id.done_button);
    doneButton.setVisibility(View.INVISIBLE);
    doneButton.setOnClickListener(v -> fragmentCallback.onSetDatasetResult(Activity.RESULT_OK));
  }

  private void getPreferredCredentials(View v) {
    getPreferredStatusText.setVisibility(View.VISIBLE);

    ThreadNetwork.getClient(getActivity())
        .getPreferredCredentials()
        .addOnSuccessListener(
            intentSenderResult -> {
              IntentSender intentSender = intentSenderResult.getIntentSender();
              if (intentSender != null) {
                preferredCredentialsLauncher.launch(
                    new IntentSenderRequest.Builder(intentSender).build());
              } else {
                getPreferredStatusText.setText("No preferred credentials found!");
              }
            })
        .addOnFailureListener(e -> getPreferredStatusText.setText(e.getMessage()));
  }

  private void setDataset(ThreadNetworkCredentials credentials) {
    Preconditions.checkNotNull(credentials);

    Log.i(TAG, "Start setting dataset: " + credentials);

    getActivity()
        .runOnUiThread(
            () -> {
              connectStatusText.setVisibility(View.VISIBLE);
            });

    // Create a commissioner candidate

    Commissioner commissioner = Commissioner.create(commissionerHandler);
    Config config = new Config();
    config.setId("Set dataset");

    if (passcode.getBytes(StandardCharsets.UTF_8).length != 9) {
      fail("Invalid passcode: " + passcode);
      return;
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

    getActivity()
        .runOnUiThread(
            () -> {
              connectStatusText.setText("Border Router connected \u2713");
            });

    // Set Pending Dataset

    error = commissioner.setRawPendingDataset(makePendingDataset(credentials));
    if (error.getCode() != ErrorCode.kNone) {
      fail("Failed to set Pending Dataset: " + error.toString());
      commissioner.resign();
      return;
    }

    getActivity()
        .runOnUiThread(
            () -> {
              setDatasetStatusText.setVisibility(View.VISIBLE);
              migrationNoteText.setVisibility(View.VISIBLE);
              doneButton.setVisibility(View.VISIBLE);
            });

    commissioner.resign();
  }

  /**
   * Creates a Pending Dataset for the given Active Dataset with the Pending Timestamp set to the
   * current date time.
   */
  private static ByteArray makePendingDataset(ThreadNetworkCredentials activeDataset) {
    byte[] datasetTlvs = activeDataset.getActiveOperationalDataset();

    byte[] delayTimer = new byte[] {52, 4, 0, 0, 0x75, 0x30}; // 30 seconds

    long now = Instant.now().getEpochSecond();
    byte[] pendingTimestamp = new byte[10];
    ByteBuffer buffer = ByteBuffer.wrap(pendingTimestamp);
    buffer.put(new byte[] {51, 8}).putLong(now << 16);

    return new ByteArray(Bytes.concat(delayTimer, pendingTimestamp, datasetTlvs));
  }

  private void fail(String errorMessage) {
    getActivity().runOnUiThread(() -> showAlertDialog(errorMessage));
  }

  private void showAlertDialog(String message) {
    new MaterialAlertDialogBuilder(getActivity(), R.style.ThreadNetworkAlertTheme)
        .setMessage(message)
        .setPositiveButton(
            "OK", (dialog, which) -> fragmentCallback.onRetrieveDatasetResult(Activity.RESULT_OK))
        .show();
  }
}
