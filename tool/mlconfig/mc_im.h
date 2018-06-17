/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __MC_IM_H__
#define __MC_IM_H__

#include <gtk/gtk.h>

void mc_im_init(void);

GtkWidget *mc_im_config_widget_new(void);

void mc_update_im(void);

#endif
