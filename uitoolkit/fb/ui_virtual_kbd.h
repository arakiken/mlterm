/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_VIRTUAL_KBD_H__
#define __UI_VIRTUAL_KBD_H__

#include "ui_display.h"
#include "../ui_window.h"

int ui_virtual_kbd_hide(void);

int ui_is_virtual_kbd_event(ui_display_t* disp, XButtonEvent* bev);

int ui_virtual_kbd_read(XKeyEvent* kev, XButtonEvent* bev);

ui_window_t* ui_is_virtual_kbd_area(int y);

#endif
