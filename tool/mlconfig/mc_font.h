/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __MC_FONT_H__
#define __MC_FONT_H__

#include <gtk/gtk.h>

#include "mc_io.h"

GtkWidget *mc_font_config_widget_new(void);

void mc_update_font_misc(void);

void mc_update_font_name(mc_io_t io);

#endif
