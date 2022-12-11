package com.example.localnotify.ui.settings;

import static android.content.Context.MODE_PRIVATE;
import static java.lang.Integer.parseInt;

import android.app.AlertDialog;
import android.app.NotificationManager;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.text.InputType;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.ViewModelProvider;

import com.example.localnotify.MainActivity;
import com.example.localnotify.R;
import com.example.localnotify.databinding.FragmentSettingsBinding;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

public class SettingsFragment extends Fragment {

    private FragmentSettingsBinding binding;

    public static ArrayList<String> rules = new ArrayList<>();
    public static Map<String, String> rulesInfo = new HashMap<>();

    public View onCreateView(@NonNull LayoutInflater inflater,
                             ViewGroup container, Bundle savedInstanceState) {

        SettingsViewModel settingsViewModel =
                new ViewModelProvider(this).get(SettingsViewModel.class);

        binding = FragmentSettingsBinding.inflate(inflater, container, false);
        View root = binding.getRoot();


        TextView anchorDistDisplay = (TextView) root.findViewById(R.id.anchor_dist_text);
        anchorDistDisplay.setText("" + (MainActivity.anchorDist * 10));


        Button onButton = (Button) root.findViewById(R.id.dnd_on);
        onButton.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                MainActivity.notificationManager.setInterruptionFilter(NotificationManager.INTERRUPTION_FILTER_NONE);
            }
        });

        Button offButton = (Button) root.findViewById(R.id.dnd_off);
        offButton.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                MainActivity.notificationManager.setInterruptionFilter(NotificationManager.INTERRUPTION_FILTER_ALL);
            }
        });

        ArrayAdapter adapter = new ArrayAdapter<String>(root.getContext(),
                R.layout.activity_listview, rules);
        ListView listView = (ListView) root.findViewById(R.id.mobile_list);
        listView.setAdapter(adapter);
        listView.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                Intent myIntent = new Intent(root.getContext(), RuleActivity.class);
                myIntent.putExtra("action", "edit");
                myIntent.putExtra("name", ((TextView) view).getText());
                myIntent.putExtra("index", position);
                SettingsFragment.this.startActivity(myIntent);
            }
        });

        Button button = (Button) root.findViewById(R.id.anchor_dist_button);
        button.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                AlertDialog.Builder builder = new AlertDialog.Builder(v.getContext());
                builder.setTitle("Set Distance between Anchors in cm");
                final EditText input = new EditText(v.getContext());
                input.setInputType(InputType.TYPE_CLASS_NUMBER);
                builder.setView(input);

                builder.setPositiveButton("OK", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        TextView view = (TextView) root.findViewById(R.id.anchor_dist_text);
                        int dist = parseInt(input.getText().toString());
                        view.setText("Anchor Distance: " + dist);
                        MainActivity.anchorDist = dist / 10;
                        SharedPreferences sharedPreferences = getActivity().getSharedPreferences("MySharedPref", MODE_PRIVATE);
                        SharedPreferences.Editor myEdit = sharedPreferences.edit();
                        myEdit.putInt("anchorDist", dist / 10);
                        myEdit.commit();
                    }
                });
                builder.setNegativeButton("Cancel", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        dialog.cancel();
                    }
                });

                builder.show();
            }
        });

        Button addButton = (Button) root.findViewById(R.id.add_rule_button);
        addButton.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                Intent myIntent = new Intent(root.getContext(), RuleActivity.class);
                myIntent.putExtra("action", "create");
                SettingsFragment.this.startActivity(myIntent);
            }
        });
        return root;
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        binding = null;
    }

}
