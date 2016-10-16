/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef ___UI_DISPLAY_H__
#define ___UI_DISPLAY_H__

#include "../ui_display.h"

Cursor ui_display_get_cursor(ui_display_t* disp, u_int shape);

XVisualInfo* ui_display_get_visual_info(ui_display_t* disp);

#endif
