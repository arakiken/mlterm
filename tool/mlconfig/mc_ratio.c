/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "mc_ratio.h"

#include <pobl/bl_str.h>
#include <pobl/bl_mem.h> /* free */
#include <pobl/bl_debug.h>
#include <glib.h>
#include <c_intl.h>

#include "mc_combo.h"
#include "mc_io.h"

#if 0
#define __DEBUG
#endif

#define MAX_VALUE_LEN 3

/* --- static variables --- */

static char new_values[MC_RATIOS][MAX_VALUE_LEN + 1]; /* 0 - 100 */
static char old_values[MC_RATIOS][MAX_VALUE_LEN + 1]; /* 0 - 100 */
static int is_changed[MC_RATIOS];

static char *config_keys[MC_RATIOS] = {
    "contrast", "gamma", "brightness", "fade_ratio", "screen_width_ratio", "screen_height_ratio",
};

static char *labels[MC_RATIOS] = {
    N_("Contrast  "), N_("Gamma"), N_("Brightness"), N_("Fade ratio on unfocus"), N_("Width"),
    N_("Height"),
};

/* --- static functions --- */

static gint ratio_selected(GtkWidget *widget, gpointer data) {
  gchar *text;

  text = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);
  if (strlen(text) <= MAX_VALUE_LEN) {
    strcpy(data, text);
  }
  g_free(text);

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " %s ratio is selected.\n", text);
#endif

  return 1;
}

/* --- global functions --- */

GtkWidget *mc_ratio_config_widget_new(int id) {
  char *value;
  GtkWidget *combo;
  GtkWidget *entry;
  char *ratios[] = {
      "100", "90", "80", "70", "60", "50", "40", "30", "20", "10",
  };

  value = mc_get_str_value(config_keys[id]);
  if (strlen(value) <= MAX_VALUE_LEN) {
    strcpy(new_values[id], value);
    strcpy(old_values[id], value);
  }
  free(value);

  combo = mc_combo_new_with_width(_(labels[id]), ratios, sizeof(ratios) / sizeof(ratios[0]),
                                  new_values[id], 0, 50, &entry);
  g_signal_connect(entry, "changed", G_CALLBACK(ratio_selected), &new_values[id]);

  return combo;
}

void mc_update_ratio(int id) {
  if (strcmp(new_values[id], old_values[id])) {
    is_changed[id] = 1;
  }

  if (is_changed[id]) {
    mc_set_str_value(config_keys[id], new_values[id]);
    strcpy(old_values[id], new_values[id]);
  }
}
