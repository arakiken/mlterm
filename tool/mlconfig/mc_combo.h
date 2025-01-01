/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __MC_COMBO_H__
#define __MC_COMBO_H__

#include <pobl/bl_types.h>
#include <gtk/gtk.h>
#include <glib.h>

#define MC_COMBO_LABEL_WIDTH 1
#define MC_COMBO_TOTAL_WIDTH 2

GtkWidget *mc_combo_new(const char *label_name, char **item_names, u_int item_num,
                        const char *selected_item_name, int is_readonly, GtkWidget **entry);

GtkWidget *mc_combo_new_with_width(const char *label_name, char **item_names, u_int item_num,
                                   const char *selected_item_name, int is_readonly, int entry_width,
                                   GtkWidget **entry);

#endif
