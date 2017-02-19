/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __MC_RADIO_H__
#define __MC_RADIO_H__

#include <gtk/gtk.h>

#define MC_RADIOS 7

#define MC_RADIO_MOD_META_MODE 0
#define MC_RADIO_BELL_MODE 1
#define MC_RADIO_SB_MODE 2
#define MC_RADIO_VERTICAL_MODE 3
#define MC_RADIO_BOX_DRAWING 4
#define MC_RADIO_FONT_POLICY 5
#define MC_RADIO_LOG_VTSEQ 6

GtkWidget *mc_radio_config_widget_new(int id);

void mc_update_radio(int id);

void mc_radio_set_callback(int id, void (*func)(void));

int mc_radio_get_value(int id);

#endif
