package com.elfin.Boat;

import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.os.Bundle;
import android.preference.CheckBoxPreference;
import android.preference.PreferenceActivity;
import android.preference.PreferenceScreen;

import java.util.ArrayList;
import java.util.Collection;

public final class PreferencesActivity extends PreferenceActivity {

	@Override
	protected void onCreate(Bundle icicle) {
		super.onCreate(icicle);
		addPreferencesFromResource(R.xml.preferences);
		//PreferenceScreen preferences = getPreferenceScreen();
		//preferences.getSharedPreferences().registerOnSharedPreferenceChangeListener(this);
	}
}
