/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "mc_alpha.h"

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

static char *new_alpha = NULL;
static char *old_alpha = NULL;
static int is_changed;

/* --- static functions --- */

static gint alpha_selected(GtkWidget *widget, gpointer data) {
  g_free(new_alpha);
  new_alpha = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " %s alpha is selected.\n", new_alpha);
#endif

  return 1;
}

/* --- global functions --- */

GtkWidget *mc_alpha_config_widget_new(void) {
  GtkWidget *combo;
  GtkWidget *entry;
  char *alphas[] = {
      "255", "223", "191", "159", "127", "95", "63", "31", "0",
  };

  new_alpha = strdup(old_alpha = mc_get_str_value("alpha"));
  is_changed = 0;

  combo = mc_combo_new_with_width(_("Alpha"), alphas, sizeof(alphas) / sizeof(alphas[0]), new_alpha,
                                  0, 50, &entry);
  g_signal_connect(entry, "changed", G_CALLBACK(alpha_selected), NULL);

  return combo;
}

void mc_update_alpha(void) {
  if (strcmp(new_alpha, old_alpha)) {
    is_changed = 1;
  }

  if (is_changed) {
    mc_set_str_value("alpha", new_alpha);
    free(old_alpha);
    old_alpha = strdup(new_alpha);
  }
}
