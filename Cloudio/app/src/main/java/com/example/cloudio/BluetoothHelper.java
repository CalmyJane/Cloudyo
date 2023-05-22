package com.example.cloudio;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanResult;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Handler;
import android.util.Log;

import androidx.core.app.ActivityCompat;

import java.util.UUID;

public class BluetoothHelper {

    private static final UUID SERVICE_UUID = UUID.fromString("0000FFE0-0000-1000-8000-00805F9B34FB");
    private static final UUID CHAR_UUID = UUID.fromString("0000FFE1-0000-1000-8000-00805F9B34FB"); // Adjust this to match the characteristic UUID that you will write to.
    private static final String TAG = "com.example.cloudio.BluetoothHelper";

    private BluetoothAdapter mAdapter;
    private BluetoothDevice mDevice;
    private BluetoothGatt mBluetoothGatt;
    private BluetoothGattService mService;
    private BluetoothGattCharacteristic mCharacteristic;

    private Context context;

    public BluetoothHelper(Context context) {
        mAdapter = BluetoothAdapter.getDefaultAdapter();
        if (mAdapter == null) {
            // Device does not support Bluetooth
            Log.e(TAG, "Device does not support Bluetooth");
        }
        this.context = context;
    }

    public boolean connectToDevice(String address) {
        mDevice = mAdapter.getRemoteDevice(address);
        try {
            Thread.sleep(100);
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
        if (ActivityCompat.checkSelfPermission(this.context, android.Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
            // TODO: Consider calling
            //    ActivityCompat#requestPermissions
            // here to request the missing permissions, and then overriding
            //   public void onRequestPermissionsResult(int requestCode, String[] permissions,
            //                                          int[] grantResults)
            // to handle the case where the user grants the permission. See the documentation
            // for ActivityCompat#requestPermissions for more details.
            return true;
        }
        Log.i("Debug", "start connecting to new device");

        mBluetoothGatt = mDevice.connectGatt(context, false, mGattCallback);
        try {
            Thread.sleep(100);
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
        return true;
    }

    public void disconnect() {
        if (mBluetoothGatt != null) {
            if (ActivityCompat.checkSelfPermission(this.context, android.Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
                // TODO: Consider calling
                //    ActivityCompat#requestPermissions
                // here to request the missing permissions, and then overriding
                //   public void onRequestPermissionsResult(int requestCode, String[] permissions,
                //                                          int[] grantResults)
                // to handle the case where the user grants the permission. See the documentation
                // for ActivityCompat#requestPermissions for more details.
                return;
            }
            mBluetoothGatt.disconnect();
        }
    }

    public void sendData(int channel, int value) {
        if (mBluetoothGatt == null || mCharacteristic == null) {
            return;
        }
        mCharacteristic.setValue(buildMessage(channel, value));
        if (ActivityCompat.checkSelfPermission(this.context, android.Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
            // TODO: Consider calling
            //    ActivityCompat#requestPermissions
            // here to request the missing permissions, and then overriding
            //   public void onRequestPermissionsResult(int requestCode, String[] permissions,
            //                                          int[] grantResults)
            // to handle the case where the user grants the permission. See the documentation
            // for ActivityCompat#requestPermissions for more details.
            return;
        }
        mBluetoothGatt.writeCharacteristic(mCharacteristic);
    }
    public byte[] buildMessage(int channel, int value) {
        // Check the range
        if (channel < 0 || channel > 255 || value < 0 || value > 255) {
            throw new IllegalArgumentException("Both 'channel' and 'value' must be between 0 and 255.");
        }

        // Prepare the array
        byte[] message = new byte[5];

        // The first element is 255
        message[0] = (byte)255;

        // The second and third elements represent the 'channel'
        // Split the 8-bit integer 'channel' into two 7-bit integers
        message[1] = (byte)(channel <= 127 ? channel : 127); // Get the first half
        message[2] = (byte)(channel <= 127 ? 0 : channel - 127); // Get the second half

        // The fourth and fifth elements represent the 'value'
        // Split the 8-bit integer 'value' into two 7-bit integers
        message[3] = (byte)(value <= 127 ? value : 127); // Get the first half
        message[4] = (byte)(value <= 127 ? 0 : value - 127); // Get the second half

        return message;
    }

    private final BluetoothGattCallback mGattCallback = new BluetoothGattCallback() {
        @Override
        public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
            if (newState == BluetoothGatt.STATE_CONNECTED) {
                Log.i(TAG, "Connected to GATT server.");
                if (ActivityCompat.checkSelfPermission(context, android.Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
                    // TODO: Consider calling
                    //    ActivityCompat#requestPermissions
                    // here to request the missing permissions, and then overriding
                    //   public void onRequestPermissionsResult(int requestCode, String[] permissions,
                    //                                          int[] grantResults)
                    // to handle the case where the user grants the permission. See the documentation
                    // for ActivityCompat#requestPermissions for more details.
                    return;
                }
                gatt.discoverServices();
            } else if (newState == BluetoothGatt.STATE_DISCONNECTED) {
                Log.i(TAG, "Disconnected from GATT server.");
            }
        }

        @Override
        public void onServicesDiscovered(BluetoothGatt gatt, int status) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                mService = gatt.getService(SERVICE_UUID);
                if (mService != null) {
                    mCharacteristic = mService.getCharacteristic(CHAR_UUID);
                }
            }
            Log.i("service not null", "bla");
        }
    };
}
