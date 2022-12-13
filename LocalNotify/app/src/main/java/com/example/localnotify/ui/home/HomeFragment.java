package com.example.localnotify.ui.home;

import static java.lang.Integer.parseInt;

import android.app.NotificationManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbManager;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.ViewModelProvider;

import com.example.localnotify.MainActivity;
import com.example.localnotify.R;
import com.example.localnotify.databinding.FragmentHomeBinding;
import com.example.localnotify.ui.settings.RuleActivity;
import com.example.localnotify.ui.settings.SettingsFragment;

import java.lang.reflect.Array;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;

public class HomeFragment extends Fragment {


    private Thread coordsThread;
    private FragmentHomeBinding binding;

    private Set<String> activeRules = new HashSet<>();

    public View onCreateView(@NonNull LayoutInflater inflater,
                             ViewGroup container, Bundle savedInstanceState) {
        HomeViewModel homeViewModel =
                new ViewModelProvider(this).get(HomeViewModel.class);

        binding = FragmentHomeBinding.inflate(inflater, container, false);
        View root = binding.getRoot();

        Button button = (Button) root.findViewById(R.id.register_tag_button);
        button.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                HashMap<String, UsbDevice> deviceList = MainActivity.usbManager.getDeviceList();
                Iterator<UsbDevice> deviceIterator = deviceList.values().iterator();
                while(deviceIterator.hasNext()) {
                    MainActivity.my_device = deviceIterator.next();
                    MainActivity.usbManager.requestPermission(MainActivity.my_device, MainActivity.permissionIntent);
                }
            }
        });

        if (coordsThread == null) {
            coordsThread = new Thread() {

                @Override
                public void run() {
                    try {
                        while (!this.isInterrupted()) {
                            Thread.sleep(500);
                            getActivity().runOnUiThread(new Runnable() {
                                @Override
                                public void run() {
                                    if (MainActivity.leftDist != 0 && MainActivity.rightDist != 0) {
                                        int x = (int) ((Math.pow(MainActivity.anchorDist, 2)
                                                + Math.pow(MainActivity.leftDist, 2)
                                                - Math.pow(MainActivity.rightDist, 2)) / (2 * MainActivity.anchorDist));
                                        int y = (int) (Math.sqrt(Math.pow(MainActivity.leftDist, 2) - Math.pow(x, 2)));
                                        x *= 10;
                                        y *= 10;
                                        TextView textView = (TextView) root.findViewById(R.id.coords_display);
                                        TextView active = (TextView) root.findViewById(R.id.active_display);
                                        textView.setText("X: " + x + " cm, Y: " + y + " cm");
                                        String toDisplay = "";
                                        boolean dndMode = false;
                                        for (int i = 0; i < SettingsFragment.rules.size(); i++) {
                                            String name = SettingsFragment.rules.get(i);
                                            int x_start = parseInt(SettingsFragment.rulesInfo.get(name + "_x_start"));
                                            int y_start = parseInt(SettingsFragment.rulesInfo.get(name + "_y_start"));
                                            int x_end = parseInt(SettingsFragment.rulesInfo.get(name + "_x_end"));
                                            int y_end = parseInt(SettingsFragment.rulesInfo.get(name + "_y_end"));
                                            if (x_start < x && x < x_end && y_start < y && y < y_end) {
                                                toDisplay += name + "\n";
                                                dndMode = true;
                                                if (!activeRules.contains(name)) {
                                                    Toast.makeText(root.getContext(), "Entered " + name, Toast.LENGTH_SHORT).show();
                                                    activeRules.add(name);
                                                }
                                            } else if (activeRules.contains(name)) {
                                                Toast.makeText(root.getContext(), "Left " + name, Toast.LENGTH_SHORT).show();
                                                activeRules.remove(name);
                                            }
                                        }
                                        if (dndMode) {
                                            MainActivity.notificationManager.setInterruptionFilter(NotificationManager.INTERRUPTION_FILTER_NONE);
                                        } else {
                                            MainActivity.notificationManager.setInterruptionFilter(NotificationManager.INTERRUPTION_FILTER_ALL);
                                        }
                                        active.setText(toDisplay);
                                    }
                                }
                            });
                        }
                    } catch (InterruptedException e) {}
                }
            };
            coordsThread.start();
        }


        return root;
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        coordsThread.interrupt();
        coordsThread = null;
        binding = null;
    }
}