package com.example.cloudio;

import android.app.Dialog;
import android.bluetooth.BluetoothDevice;
import android.content.DialogInterface;
import android.content.pm.PackageManager;
import android.os.Bundle;

import androidx.appcompat.app.AlertDialog;
import androidx.core.app.ActivityCompat;
import androidx.fragment.app.DialogFragment;

import java.util.List;

public class DeviceListDialogFragment extends DialogFragment {

    public interface DeviceListDialogListener {
        void onDeviceSelected(String deviceName);
    }

    private DeviceListDialogListener listener;
    private List<BluetoothDevice> devices;

    public DeviceListDialogFragment(DeviceListDialogListener listener, List<BluetoothDevice> devices) {
        this.listener = listener;
        this.devices = devices;
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());

        final String[] deviceNames = new String[devices.size()]; // your device names here
        int i = 0;
        for (BluetoothDevice bd :
                devices) {
            if (ActivityCompat.checkSelfPermission(this.getContext(), android.Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
                // TODO: Consider calling
                //    ActivityCompat#requestPermissions
                // here to request the missing permissions, and then overriding
                //   public void onRequestPermissionsResult(int requestCode, String[] permissions,
                //                                          int[] grantResults)
                // to handle the case where the user grants the permission. See the documentation
                // for ActivityCompat#requestPermissions for more details.
                return null;
            }
            deviceNames[i] = bd.getName();
            i++;
        }

        builder.setTitle("Pick a Device")
                .setItems(deviceNames, new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int which) {
                        // Return the selected device name to the MainActivity
                        listener.onDeviceSelected(deviceNames[which]);
                    }
                });
        return builder.create();
    }
}
