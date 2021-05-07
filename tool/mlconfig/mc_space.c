/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "mc_space.h"

#include <pobl/bl_str.h>
#include <pobl/bl_mem.h> /* free */
#include <pobl/bl_debug.h>
#include <pobl/bl_util.h> /* BL_ARRAY_SIZE */
#include <glib.h>
#include <c_intl.h>

#include "mc_combo.h"
#include "mc_io.h"

#if 0
#define __DEBUG
#endif

#define MAX_VALUE_LEN 2

/* --- static variables --- */

static char new_values[MC_SPACES][MAX_VALUE_LEN + 1]; /* -9...99 */
static char old_values[MC_SPACES][MAX_VALUE_LEN + 1]; /* -9...99 */
static int is_changed[MC_SPACES];

static char *config_keys[MC_SPACES] = {
  "line_space", "letter_space", "baseline_offset", "underline_offset",
};

static char *labels[MC_SPACES] = {
  N_("Line space (pixels)"), N_("Letter space (pixels)"), N_("Baseline position (pixels)"),
  N_("Underline position (pixels)"),
};

/* --- static functions --- */

static void space_selected(GtkWidget *widget, gpointer data) {
  gchar *text;

  text = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);
  if (strlen(text) <= MAX_VALUE_LEN) {
    strcpy(data, text);
  }
  g_free(text);

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " %s space is selected.\n", text);
#endif
}

/* --- global functions --- */

GtkWidget *mc_space_config_widget_new(int id) {
  char *value;
  GtkWidget *combo;
  GtkWidget *entry;
  char *spaces[] = {
    "3", "2", "1", "0", "-1", "-2",
  };
  char *spaces2[] = {
    "5", "4", "3", "2", "1", "0",
  };

  value = mc_get_str_value(config_keys[id]);
  if (strlen(value) <= MAX_VALUE_LEN) {
    strcpy(new_values[id], value);
    strcpy(old_values[id], value);
  }
  free(value);

  if (id == MC_SPACE_LETTER) {
    combo = mc_combo_new_with_width(_(labels[id]), spaces2, BL_ARRAY_SIZE(spaces2),
                                    new_values[id], 0, 20, &entry);
  } else {
    combo = mc_combo_new_with_width(_(labels[id]), spaces, BL_ARRAY_SIZE(spaces),
                                    new_values[id], 0, 20, &entry);
  }

  g_signal_connect(entry, "changed", G_CALLBACK(space_selected), &new_values[id]);

  return combo;
}

void mc_update_space(int id) {
  if (strcmp(new_values[id], old_values[id])) {
    is_changed[id] = 1;
  }

  if (is_changed[id]) {
    mc_set_str_value(config_keys[id], new_values[id]);
    strcpy(old_values[id], new_values[id]);
  }
}
