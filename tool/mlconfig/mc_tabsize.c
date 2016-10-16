/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "mc_tabsize.h"

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

static char* new_tabsize = NULL;
static char* old_tabsize = NULL;
static int is_changed;

/* --- static functions --- */

static gint tabsize_selected(GtkWidget* widget, gpointer data) {
  g_free(new_tabsize);
  new_tabsize = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " %s tabsize is selected.\n", new_tabsize);
#endif

  return 1;
}

/* --- global functions --- */

GtkWidget* mc_tabsize_config_widget_new(void) {
  GtkWidget* combo;
  GtkWidget* entry;
  char* tabsizes[] = {
      "8", "4", "2",
  };

  new_tabsize = strdup(old_tabsize = mc_get_str_value("tabsize"));
  is_changed = 0;

  combo =
      mc_combo_new_with_width(_("Tab width (columns)"), tabsizes,
                              sizeof(tabsizes) / sizeof(tabsizes[0]), new_tabsize, 0, 20, &entry);
  g_signal_connect(entry, "changed", G_CALLBACK(tabsize_selected), NULL);

  return combo;
}

void mc_update_tabsize(void) {
  if (strcmp(new_tabsize, old_tabsize)) is_changed = 1;

  if (is_changed) {
    mc_set_str_value("tabsize", new_tabsize);
    free(old_tabsize);
    old_tabsize = strdup(new_tabsize);
  }
}
