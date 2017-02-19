/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "mc_logsize.h"

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

static char *new_logsize = NULL;
static char *old_logsize = NULL;
static int is_changed;

/* --- static functions --- */

static gint logsize_selected(GtkWidget *widget, gpointer data) {
  g_free(new_logsize);
  new_logsize = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " %s logsize is selected.\n", new_logsize);
#endif

  return 1;
}

/* --- global functions --- */

GtkWidget *mc_logsize_config_widget_new(void) {
  GtkWidget *combo;
  GtkWidget *entry;
  char *logsizes[] = {
      "128", "256", "512", "1024", "unlimited",
  };

  new_logsize = strdup(old_logsize = mc_get_str_value("logsize"));
  is_changed = 0;

  combo =
      mc_combo_new_with_width(_("Backlog size (lines)"), logsizes,
                              sizeof(logsizes) / sizeof(logsizes[0]), new_logsize, 0, 50, &entry);
  g_signal_connect(entry, "changed", G_CALLBACK(logsize_selected), NULL);

  return combo;
}

void mc_update_logsize(void) {
  if (strcmp(new_logsize, old_logsize)) is_changed = 1;

  if (is_changed) {
    mc_set_str_value("logsize", new_logsize);
    free(old_logsize);
    old_logsize = strdup(new_logsize);
  }
}
