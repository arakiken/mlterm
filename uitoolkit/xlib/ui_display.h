/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef ___UI_DISPLAY_H__
#define ___UI_DISPLAY_H__

#include "../ui_display.h"

Cursor ui_display_get_cursor(ui_display_t *disp, u_int shape);

XVisualInfo *ui_display_get_visual_info(ui_display_t *disp);

int ui_display_own_clipboard(ui_display_t *disp, ui_window_ptr_t win);

int ui_display_clear_clipboard(ui_display_t *disp, ui_window_ptr_t win);

#endif
