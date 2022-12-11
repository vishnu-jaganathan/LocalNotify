package com.example.localnotify.ui.instructions;

import android.app.NotificationManager;
import android.hardware.usb.UsbDevice;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.ViewModelProvider;

import com.example.localnotify.MainActivity;
import com.example.localnotify.R;
import com.example.localnotify.databinding.FragmentInstructionsBinding;

import java.util.HashMap;
import java.util.Iterator;

public class InstructionsFragment extends Fragment {

    private FragmentInstructionsBinding binding;

    public View onCreateView(@NonNull LayoutInflater inflater,
                             ViewGroup container, Bundle savedInstanceState) {
        InstructionsViewModel instructionsViewModel =
                new ViewModelProvider(this).get(InstructionsViewModel.class);

        binding = FragmentInstructionsBinding.inflate(inflater, container, false);
        View root = binding.getRoot();


        return root;
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        binding = null;
    }
}