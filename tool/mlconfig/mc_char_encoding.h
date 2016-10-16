/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __MC_CHAR_ENCODING_H__
#define __MC_CHAR_ENCODING_H__

#include <gtk/gtk.h>

GtkWidget* mc_char_encoding_config_widget_new(void);

void mc_update_char_encoding(void);

char* mc_get_char_encoding(void);

#endif
