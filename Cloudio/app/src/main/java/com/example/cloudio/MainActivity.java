package com.example.cloudio;

import android.bluetooth.BluetoothDevice;
import android.content.pm.PackageManager;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.HorizontalScrollView;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.SeekBar.OnSeekBarChangeListener;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;

import java.util.List;

public class MainActivity extends AppCompatActivity implements DeviceListDialogFragment.DeviceListDialogListener {
    private int numSliders = 10; // Number of sliders
    private BluetoothHelper bt = new BluetoothHelper(this);
    private SeekBar[] seekBars = new SeekBar[numSliders];

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Parent LinearLayout
        LinearLayout parentLayout = new LinearLayout(this);
        parentLayout.setOrientation(LinearLayout.VERTICAL);
        parentLayout.setLayoutParams(new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.MATCH_PARENT
        ));

        // Your logo (a drawable resource)
        ImageView logo = new ImageView(this);
        logo.setImageResource(R.drawable.cj_logo); // Replace 'logo' with your actual logo filename
        logo.setLayoutParams(new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
        ));
        logo.setAdjustViewBounds(true); // Preserve aspect ratio
        parentLayout.addView(logo);

        //Text input field for device adress
        EditText editText = new EditText(this);
        editText.setLayoutParams(new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT));

        editText.setSingleLine(true);
        editText.setText("A0:6C:65:FC:F8:23");
        parentLayout.addView(editText);

        // Button or any other static views
        Button button = new Button(this);
        button.setText("Connect");



        //Scan for devices
        BluetoothLeScannerHelper scanner = new BluetoothLeScannerHelper(this);
        scanner.startScanning();

        // Set the OnClickListener
        button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                bt.connectToDevice(editText.getText().toString());

//                // After scanning for a while, stop scanning and get the device list
//                scanner.stopScanning();
//                List<BluetoothDevice> devices = scanner.getDeviceList();
//                for (BluetoothDevice device : devices) {
//                    if (ActivityCompat.checkSelfPermission(MainActivity.this, android.Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
//                        // TODO: Consider calling
//                        //    ActivityCompat#requestPermissions
//                        // here to request the missing permissions, and then overriding
//                        //   public void onRequestPermissionsResult(int requestCode, String[] permissions,
//                        //                                          int[] grantResults)
//                        // to handle the case where the user grants the permission. See the documentation
//                        // for ActivityCompat#requestPermissions for more details.
//                        return;
//                    }
//                    Log.i("MainActivity", "Found device: " + device.getName());
//                }
//                Log.i("MainActivity", "Printed all Devices -----");
//                // This code will be executed when the button is clicked
//                new DeviceListDialogFragment(MainActivity.this, devices).show(getSupportFragmentManager(), "deviceList");
            }
        });

        parentLayout.addView(button);

        // Create Flash button
        Button flbtn = new Button(this);
        // Calculate 10% of screen height
        DisplayMetrics displayMetrics = this.getResources().getDisplayMetrics();
        int height = (int)(displayMetrics.heightPixels * 0.10);
        flbtn.setLayoutParams((new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, height)));
        flbtn.setText("FLASH");
        flbtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                seekBars[6].setProgress(250); //turn on manual flash, reset short time later

                // Initialize the handler with the main (UI) thread's Looper
                Handler handler = new Handler(Looper.getMainLooper());
                // Delay in milliseconds
                long delayMillis = 100;
                // Perform an operation after the specified delay
                handler.postDelayed(new Runnable() {
                    @Override
                    public void run() {
                        // Code to be executed after the delay
                        // Insert your desired operation here
                        seekBars[6].setProgress(10); //reset manual flash parameter
                    }
                }, delayMillis);
            }
        });
        parentLayout.addView(flbtn);

        // Create HorizontalScrollView
        HorizontalScrollView horizontalScrollView = new HorizontalScrollView(this);
        LinearLayout.LayoutParams layoutParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        horizontalScrollView.setLayoutParams(layoutParams);

        // Create a new LinearLayout for buttons
        LinearLayout buttonLayout = new LinearLayout(this);
        buttonLayout.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, height));
        buttonLayout.setOrientation(LinearLayout.HORIZONTAL);

        // Create buttons with icons
        String[] btns = new String[]{"Off", "Lamp", "Thunderstorm", "Party"};
        int[][] values = new int[][]{
                {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                {50, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                {0, 50, 50, 255, 150, 255, 255, 0, 0, 0},
                {0, 200, 150, 200, 0, 0, 0, 0, 0, 0}
        };

        for (int i = 0; i < 4; i++) {
            final int index = i;  // Save the current index
            Button btn = new Button(this);
            btn.setLayoutParams(new LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.WRAP_CONTENT,
                    height));
            btn.setText(btns[i]); // Set the text for each
            btn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                for (int j = 0; j < numSliders; j++) {
                    seekBars[j].setProgress(values[index][j]);
                }            }
            });
            buttonLayout.addView(btn);
        }

        // Add LinearLayout to HorizontalScrollView
        horizontalScrollView.addView(buttonLayout);

        // Add HorizontalScrollView to your existing LinearLayout
        parentLayout.addView(horizontalScrollView);

        // ScrollView to make our sliders scrollable
        ScrollView scrollView = new ScrollView(this);
        scrollView.setLayoutParams(new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.MATCH_PARENT
        ));

        // LinearLayout to contain our sliders
        LinearLayout linearLayout = new LinearLayout(this);
        linearLayout.setOrientation(LinearLayout.VERTICAL);
        linearLayout.setLayoutParams(new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
        ));
        final String[] NAMES = new String[]{
                "1: Mode",
                "2: Twinkle Density",
                "3: Twinkle Speed",
                "4: Twinkle Color",
                "5: Flash Rate",
                "6: Flash Brightness",
                "7: Manual Flash"
        };
        // Dynamically generate sliders
        for (int i = 1; i <= numSliders; i++) {
            // Title for the SeekBar
            TextView title = new TextView(this);
            if (i <= NAMES.length) {
                title.setText(NAMES[i-1]);
            } else {
                title.setText((i) + ":");
            }
            title.setTextSize(20);
            title.setTextAlignment(TextView.TEXT_ALIGNMENT_CENTER);

            // SeekBar
            SeekBar seekBar = new SeekBar(this);
            seekBar.setLayoutParams(new LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT
            ));
            seekBar.setMax(255);
            final int index = i;
            seekBar.setOnSeekBarChangeListener(new OnSeekBarChangeListener() {
                @Override
                public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                    onSliderChanged(index, progress);
                }

                @Override
                public void onStartTrackingTouch(SeekBar seekBar) {
                    // Do nothing
                }

                @Override
                public void onStopTrackingTouch(SeekBar seekBar) {
                    // Do nothing
                }
            });
            seekBars[i-1] = seekBar;
            // Add title and SeekBar to LinearLayout
            linearLayout.addView(title);
            linearLayout.addView(seekBar);
        }

        // Add LinearLayout to ScrollView
        scrollView.addView(linearLayout);

        // Add ScrollView to parent LinearLayout
        parentLayout.addView(scrollView);

        // Set parent LinearLayout as our content view
        setContentView(parentLayout);
    }

    private void onSliderChanged(int index, int value) {
        // Handle slider change here
        bt.sendData(index, value);
        System.out.println("Slider " + index + " changed to value: " + value);
    }

    @Override
    public void onDeviceSelected(String deviceName) {
        // Handle the selected device name here
        System.out.println("Selected device: " + deviceName);
    }
}
