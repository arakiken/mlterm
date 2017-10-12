/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "mc_click.h"

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

/* --- static variables --- */

static char *new_click_interval = NULL;
static char *old_click_interval = NULL;
static int is_changed;

/* --- static functions --- */

static gint click_interval_selected(GtkWidget *widget, gpointer data) {
  g_free(new_click_interval);
  new_click_interval = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " %s click interval is selected.\n", new_click_interval);
#endif

  return 1;
}

/* --- global functions --- */

GtkWidget *mc_click_interval_config_widget_new(void) {
  GtkWidget *combo;
  GtkWidget *entry;
  char *click_intervals[] = {
    "250", "300", "350", "400", "450", "500",
  };

  new_click_interval = strdup(old_click_interval = mc_get_str_value("click_interval"));
  is_changed = 0;

  combo = mc_combo_new_with_width(_("Double click interval (msec)"), click_intervals,
                                  sizeof(click_intervals) / sizeof(click_intervals[0]),
                                  new_click_interval, 0, 50, &entry);
  g_signal_connect(entry, "changed", G_CALLBACK(click_interval_selected), NULL);

  return combo;
}

void mc_update_click_interval(void) {
  if (strcmp(new_click_interval, old_click_interval)) is_changed = 1;

  if (is_changed) {
    mc_set_str_value("click_interval", new_click_interval);
    free(old_click_interval);
    old_click_interval = strdup(new_click_interval);
  }
}
