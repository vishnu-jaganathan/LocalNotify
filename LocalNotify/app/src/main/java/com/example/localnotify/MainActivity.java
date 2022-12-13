package com.example.localnotify;

import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbManager;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import com.example.localnotify.ui.home.HomeFragment;
import com.example.localnotify.ui.settings.RuleActivity;
import com.example.localnotify.ui.settings.SettingsFragment;
import com.example.localnotify.ui.settings.SettingsViewModel;
import com.felhr.usbserial.UsbSerialDevice;
import com.felhr.usbserial.UsbSerialInterface;
import com.google.android.material.bottomnavigation.BottomNavigationView;

import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.Fragment;
import androidx.navigation.NavController;
import androidx.navigation.Navigation;
import androidx.navigation.ui.AppBarConfiguration;
import androidx.navigation.ui.NavigationUI;

import com.example.localnotify.databinding.ActivityMainBinding;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;

import com.felhr.usbserial.UsbSerialDevice;
import com.felhr.usbserial.UsbSerialInterface;

public class MainActivity extends AppCompatActivity {

    public static double anchorDist = 38; // measures in 10s of centimeters
    public static double leftDist = 0;
    public static double rightDist = 0;
    private int key = 0;
    private boolean receiveKey = true;
    private static final String ACTION_USB_PERMISSION =
            "com.android.example.USB_PERMISSION";


    private UsbSerialInterface.UsbReadCallback mCallback = new UsbSerialInterface.UsbReadCallback() {
        @Override
        public void onReceivedData(byte[] arg0) {
            if (arg0.length < 1) {
                return;
            }
            if (receiveKey) {
                key = (int) arg0[0];
            } else {
                if (key == 3) {
                    leftDist = (int) arg0[0];
                } else if (key == 4) {
                    rightDist = (int) arg0[0];
                }
            }
            receiveKey = !receiveKey;
        }
    };

    private final BroadcastReceiver usbReceiver = new BroadcastReceiver() {

        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (ACTION_USB_PERMISSION.equals(action)) {
                synchronized (this) {
                    UsbDevice device = (UsbDevice)intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);

                    if (intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)) {
                        if(device != null){
                            UsbDeviceConnection connection = usbManager.openDevice(device);
                            serial = UsbSerialDevice.createUsbSerialDevice(my_device, connection);
                            serial.open();
                            serial.setBaudRate(115200); // 115200
                            serial.setDataBits(UsbSerialInterface.DATA_BITS_8);
                            serial.setParity(UsbSerialInterface.PARITY_ODD);
                            serial.setFlowControl(UsbSerialInterface.FLOW_CONTROL_OFF);
                            serial.read(mCallback);
                        }
                    }
                    else {
                        Toast.makeText(MainActivity.this, "PERMISSION DENIED", Toast.LENGTH_SHORT).show();
                    }
                }
            }
        }
    };

    public static PendingIntent permissionIntent;
    public static UsbDevice my_device;
    public static UsbManager usbManager;
    private UsbSerialDevice serial;

    public static NotificationManager notificationManager;


    private ActivityMainBinding binding;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());
        BottomNavigationView navView = findViewById(R.id.nav_view);
        // Passing each menu ID as a set of Ids because each
        // menu should be considered as top level destinations.
        AppBarConfiguration appBarConfiguration = new AppBarConfiguration.Builder(
                R.id.navigation_settings, R.id.navigation_instructions, R.id.navigation_home)
                .build();
        NavController navController = Navigation.findNavController(this, R.id.nav_host_fragment_activity_main);
        NavigationUI.setupActionBarWithNavController(this, navController, appBarConfiguration);
        NavigationUI.setupWithNavController(binding.navView, navController);

        notificationManager = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);

        usbManager = (UsbManager) getSystemService(Context.USB_SERVICE);
        String ACTION_USB_PERMISSION = "com.android.example.USB_PERMISSION";
        permissionIntent = PendingIntent.getBroadcast(this, 0, new Intent(ACTION_USB_PERMISSION), 0);
        IntentFilter filter = new IntentFilter(ACTION_USB_PERMISSION);
        registerReceiver(usbReceiver, filter);


        SharedPreferences sh = getSharedPreferences("MySharedPref", MODE_PRIVATE);
        Set<String> savedRules = sh.getStringSet("rulesList", new HashSet<>());
        SettingsFragment.rules.clear();
        SettingsFragment.rules.addAll(savedRules);
        Map<String, ?> stored = sh.getAll();
        for (Map.Entry<String,?> entry : stored.entrySet()) {
            if (entry.getValue() instanceof String) {
                SettingsFragment.rulesInfo.put(entry.getKey(), (String) entry.getValue());
            }
        }
        anchorDist = sh.getInt("anchorDist", 38);
    }


}
