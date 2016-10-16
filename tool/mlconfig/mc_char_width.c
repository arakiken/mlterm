/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "mc_char_width.h"

#include <pobl/bl_str.h>
#include <pobl/bl_mem.h> /* free */
#include <pobl/bl_debug.h>
#include <glib.h>
#include <c_intl.h>

#include "mc_unicode_areas.h"
#include "mc_flags.h"
#include "mc_io.h"

#if 0
#define __DEBUG
#endif

/* --- static variables --- */

static char* full_width_areas;

/* --- static functions --- */

static void edit_full_width_areas(GtkWidget* widget, gpointer data) {
  char* cur_areas;
  char* new_areas;

  if (full_width_areas) {
    cur_areas = strdup(full_width_areas);
  } else {
    cur_areas = mc_get_str_value("unicode_full_width_areas");
  }

  if ((new_areas = mc_get_unicode_areas(cur_areas)) &&
      bl_compare_str(full_width_areas, new_areas) != 0) {
    free(full_width_areas);
    full_width_areas = new_areas;
  }

  free(cur_areas);
}

/* --- global functions --- */

GtkWidget* mc_char_width_config_widget_new(void) {
  GtkWidget* hbox;
  GtkWidget* vbox;
  GtkWidget* widget;

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_widget_show(vbox);

  widget = mc_flag_config_widget_new(MC_FLAG_AWIDTH);
  gtk_widget_show(widget);
  gtk_box_pack_start(GTK_BOX(vbox), widget, FALSE, FALSE, 0);

  widget = mc_flag_config_widget_new(MC_FLAG_MCOL);
  gtk_widget_show(widget);
  gtk_box_pack_start(GTK_BOX(vbox), widget, FALSE, FALSE, 0);

  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

  widget = gtk_button_new_with_label(_("Set full width\nareas manually"));
  gtk_widget_show(widget);
  gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, FALSE, 4);
  g_signal_connect(widget, "clicked", G_CALLBACK(edit_full_width_areas), NULL);

  return hbox;
}

void mc_update_char_width(void) {
  mc_update_flag_mode(MC_FLAG_MCOL);
  mc_update_flag_mode(MC_FLAG_AWIDTH);

  if (full_width_areas) {
    mc_set_str_value("unicode_full_width_areas", full_width_areas);
  }
}
