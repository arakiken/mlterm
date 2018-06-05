/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_CONNECT_DIALOG_H__
#define __UI_CONNECT_DIALOG_H__

#include "ui.h"

int ui_connect_dialog(char **uri, char **pass, char **exec_cmd, char **privkey, int *x11_fwd,
                      char *display_name, Window parent_window, char *default_server);

#endif
