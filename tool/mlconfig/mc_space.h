/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __MC_SPACE_H__
#define __MC_SPACE_H__

#include <gtk/gtk.h>

#define MC_SPACES 4
#define MC_SPACE_LINE 0
#define MC_SPACE_LETTER 1
#define MC_SPACE_BASELINE 2
#define MC_SPACE_UNDERLINE 3

GtkWidget *mc_space_config_widget_new(int id);

void mc_update_space(int id);

#endif
