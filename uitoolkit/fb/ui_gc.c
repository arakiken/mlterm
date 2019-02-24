/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../ui_gc.h"

#include <stdio.h>

/* --- global functions --- */

ui_gc_t *ui_gc_new(Display *display, Drawable drawable) { return NULL; }

void ui_gc_destroy(ui_gc_t *gc) {}

void ui_gc_set_fg_color(ui_gc_t *gc, u_long fg_color) {}

void ui_gc_set_bg_color(ui_gc_t *gc, u_long bg_color) {}

void ui_gc_set_fid(ui_gc_t *gc, Font fid) {}
