package com.example.localnotify.ui.settings;

import androidx.appcompat.app.AppCompatActivity;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.text.InputType;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import com.example.localnotify.MainActivity;
import com.example.localnotify.R;

import java.util.HashSet;
import java.util.Map;
import java.util.Set;

public class RuleActivity extends AppCompatActivity {

    private int index;
    private String action;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_rule);
        Intent intent = getIntent();
        String actionType = intent.getStringExtra("action");
        action = actionType;
        if (action.equals("edit")) {
            String name = intent.getStringExtra("name");
            index = intent.getIntExtra("index", 0);
            EditText editText = (EditText) findViewById(R.id.edit_name);
            editText.setText(name);

            if(SettingsFragment.rulesInfo.containsKey(name + "_x_start")) {
                EditText xStart = (EditText) findViewById(R.id.x_start_pos);
                xStart.setText(SettingsFragment.rulesInfo.get(name + "_x_start"));
            }
            if(SettingsFragment.rulesInfo.containsKey(name + "_y_start")) {
                EditText yStart = (EditText) findViewById(R.id.y_start_pos);
                yStart.setText(SettingsFragment.rulesInfo.get(name + "_y_start"));
            }
            if(SettingsFragment.rulesInfo.containsKey(name + "_x_end")) {
                EditText xEnd = (EditText) findViewById(R.id.x_end_pos);
                xEnd.setText(SettingsFragment.rulesInfo.get(name + "_x_end"));
            }
            if(SettingsFragment.rulesInfo.containsKey(name + "_y_end")) {
                EditText yEnd = (EditText) findViewById(R.id.y_end_pos);
                yEnd.setText(SettingsFragment.rulesInfo.get(name + "_y_end"));
            }

        }


        Button button = (Button) findViewById(R.id.submit_button);
        button.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                EditText editText = (EditText) findViewById(R.id.edit_name);
                String name = "" + editText.getText();
                if (action.equals("edit")) {
                    SettingsFragment.rules.set(index, name);
                } else if(action.equals("create")) {
                    SettingsFragment.rules.add(name);
                }
                EditText xStart = (EditText) findViewById(R.id.x_start_pos);
                SettingsFragment.rulesInfo.put(name + "_x_start", "" + xStart.getText());
                EditText yStart = (EditText) findViewById(R.id.y_start_pos);
                SettingsFragment.rulesInfo.put(name + "_y_start", "" + yStart.getText());
                EditText xEnd = (EditText) findViewById(R.id.x_end_pos);
                SettingsFragment.rulesInfo.put(name + "_x_end", "" + xEnd.getText());
                EditText yEnd = (EditText) findViewById(R.id.y_end_pos);
                SettingsFragment.rulesInfo.put(name + "_y_end", "" + yEnd.getText());

                updateSharedPrefs();
                Intent myIntent = new Intent(RuleActivity.this, MainActivity.class);
                RuleActivity.this.startActivity(myIntent);
            }
        });

        Button delButton = (Button) findViewById(R.id.delete_button);
        delButton.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                if (action.equals("edit")) {
                    SettingsFragment.rules.remove(index);
                    updateSharedPrefs();
                }
                Intent myIntent = new Intent(RuleActivity.this, MainActivity.class);
                RuleActivity.this.startActivity(myIntent);
            }
        });
    }

    private void updateSharedPrefs() {
        Set<String> rulesSet = new HashSet<>(SettingsFragment.rules);
        SharedPreferences sharedPreferences = getSharedPreferences("MySharedPref",MODE_PRIVATE);
        SharedPreferences.Editor myEdit = sharedPreferences.edit();
        myEdit.putStringSet("rulesList", rulesSet);
        for (Map.Entry<String,String> entry : SettingsFragment.rulesInfo.entrySet()) {
            myEdit.putString(entry.getKey(), entry.getValue());
        }
        myEdit.commit();
    }
}
