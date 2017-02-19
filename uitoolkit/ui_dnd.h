/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

/*
 * Drag and Drop stuff.
 */

#ifndef __UI_DND_H__
#define __UI_DND_H__

#include "ui_window.h"

int ui_dnd_filter_event(XEvent *event, ui_window_t *win);

#endif
