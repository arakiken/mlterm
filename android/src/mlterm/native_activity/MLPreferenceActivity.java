/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

package mlterm.native_activity;

import android.preference.PreferenceActivity;
import android.preference.CheckBoxPreference;
import android.preference.EditTextPreference;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.PreferenceScreen;
import android.preference.PreferenceCategory;
import android.os.Bundle;
import android.text.InputType;
import android.widget.TextView;

public class MLPreferenceActivity extends PreferenceActivity {
  private native void setConfig(String key, String val);
  private native void setDefaultFontConfig(String font);
  private native boolean getBoolConfig(String key);
  private native String getStrConfig(String key);
  private native String getDefaultFontConfig();

  private void addCheckBox(PreferenceCategory category, final String key, String title) {
    CheckBoxPreference checkbox = new CheckBoxPreference(this);
    checkbox.setTitle(title);
    checkbox.setPersistent(false);
    checkbox.setChecked(getBoolConfig(key));
    checkbox.setOnPreferenceChangeListener(new OnPreferenceChangeListener() {
        @Override
        public boolean onPreferenceChange(Preference preference, Object newValue) {
          boolean checked = (Boolean)newValue;
          if (checked) {
            setConfig(key, "true");
          } else {
            setConfig(key, "false");
          }
          return true;
        }
      });
    category.addPreference(checkbox);
  }

  private void addEditText(PreferenceCategory category, final String key, String title, int type) {
    EditTextPreference edit = new EditTextPreference(this);
    edit.setTitle(title);
    edit.setPersistent(false);
    edit.getEditText().setInputType(type);
    edit.setSummary(getStrConfig(key));
    edit.setOnPreferenceChangeListener(new OnPreferenceChangeListener() {
        @Override
        public boolean onPreferenceChange(Preference preference, Object newValue) {
          setConfig(key, newValue.toString());
          ((EditTextPreference)preference).setSummary(getStrConfig(key));
          return true;
        }
      });
    category.addPreference(edit);
  }

  private void addFontEditText(PreferenceCategory category) {
    EditTextPreference edit = new EditTextPreference(this);
    edit.setTitle("Font path");
    edit.setPersistent(false);
    edit.getEditText().setInputType(InputType.TYPE_CLASS_TEXT);
    edit.setSummary(getDefaultFontConfig());
    edit.setOnPreferenceChangeListener(new OnPreferenceChangeListener() {
        @Override
        public boolean onPreferenceChange(Preference preference, Object newValue) {
          setDefaultFontConfig(newValue.toString());
          ((EditTextPreference)preference).setSummary(getDefaultFontConfig());
          return true;
        }
      });
    category.addPreference(edit);
  }

  private void addList(PreferenceCategory category, final String key, String title,
                       CharSequence[] entries) {
    ListPreference list = new ListPreference(this);
    list.setTitle(title);
    list.setPersistent(false);
    list.setEntries(entries);
    list.setEntryValues(entries);
    list.setSummary(getStrConfig(key));
    list.setOnPreferenceChangeListener(new OnPreferenceChangeListener() {
        @Override
        public boolean onPreferenceChange(Preference preference, Object newValue) {
          setConfig(key, newValue.toString());
          ((ListPreference)preference).setSummary(getStrConfig(key));
          return true;
        }
      });
    category.addPreference(list);
  }

  @Override
  public void onCreate(Bundle state) {
    super.onCreate(state);

    PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(this);
    setPreferenceScreen(screen);

    PreferenceCategory category = new PreferenceCategory(this);
    screen.addPreference(category);

    addCheckBox(category, "start_with_local_pty", "Don't show SSH dialog on startup");
    addEditText(category, "fontsize", "Font size", InputType.TYPE_CLASS_NUMBER);
    addFontEditText(category);
    addEditText(category, "encoding", "Encoding",
                InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
    addCheckBox(category, "col_size_of_width_a", "Ambiguouswidth = fullwidth");
    addEditText(category, "line_space", "Line space", InputType.TYPE_CLASS_NUMBER);
    addEditText(category, "letter_space", "Character space", InputType.TYPE_CLASS_NUMBER);
    addEditText(category, "fg_color", "Foreground Color",
                InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
    addEditText(category, "bg_color", "Background Color",
                InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
    addEditText(category, "wall_picture", "Wall picture",
                InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
    addEditText(category, "alpha", "Alpha", InputType.TYPE_CLASS_NUMBER);
    CharSequence[] entries = { "right", "left", "none" };
    addList(category, "scrollbar_mode", "Scrollbar", entries);
    addCheckBox(category, "use_extended_scroll_shortcut", "Scroll by Shift+Up or Shift+Down");
    addEditText(category, "logsize", "Backlog size", InputType.TYPE_CLASS_NUMBER);
  }
}
