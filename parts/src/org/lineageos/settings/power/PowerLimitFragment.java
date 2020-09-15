/*
 * Copyright (C) 2015-2016 The CyanogenMod Project
 *               2017,2021-2025 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.lineageos.settings.power;

import android.os.Bundle;
import android.util.Log;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragment;
import androidx.preference.SwitchPreference;
import org.lineageos.settings.R;

public class PowerLimitFragment extends PreferenceFragment implements
        Preference.OnPreferenceChangeListener {

    private static final String TAG = "PowerLimitFragment";
    private static final String PREF_POWER_LIMIT = "power_limit_pref";
    private static final String NODE_PATH = "/sys/kernel/cpu_power_toggle/limit_mode";

    private SwitchPreference mPowerLimitPref;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        addPreferencesFromResource(R.xml.power_limit_settings);

        mPowerLimitPref = (SwitchPreference) findPreference(PREF_POWER_LIMIT);
        mPowerLimitPref.setOnPreferenceChangeListener(this);
        mPowerLimitPref.setChecked(isCurrentlyLimited());
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (preference == mPowerLimitPref) {
            boolean enabled = (Boolean) newValue;
            return writeNode(enabled ? "1" : "0");
        }
        return false;
    }

    private boolean writeNode(String value) {
        try (FileWriter fw = new FileWriter(NODE_PATH)) {
            fw.write(value);
            return true;
        } catch (IOException e) {
            Log.e(TAG, "Failed to write node", e);
            return false;
        }
    }

    private boolean isCurrentlyLimited() {
        try {
            return "1".equals(new java.util.Scanner(new File(NODE_PATH)).useDelimiter("\\A").next().trim());
        } catch (Exception e) {
            return false;
        }
    }
}
