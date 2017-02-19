/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_COLOR_H__
#define __UI_COLOR_H__

#include <pobl/bl_types.h>
#include <pobl/bl_mem.h> /* alloca */

#include "ui_display.h"

typedef struct ui_color {
  /* Public */
  u_int32_t pixel;

#ifdef UI_COLOR_HAS_RGB
  /* Private except ui_color_cache.c */
  u_int8_t red;
  u_int8_t green;
  u_int8_t blue;
  u_int8_t alpha;
#endif

} ui_color_t;

int ui_load_named_xcolor(ui_display_t *disp, ui_color_t *xcolor, char *name);

int ui_load_rgb_xcolor(ui_display_t *disp, ui_color_t *xcolor, u_int8_t red, u_int8_t green,
                       u_int8_t blue, u_int8_t alpha);

int ui_unload_xcolor(ui_display_t *disp, ui_color_t *xcolor);

int ui_get_xcolor_rgba(u_int8_t *red, u_int8_t *green, u_int8_t *blue, u_int8_t *alpha,
                       ui_color_t *xcolor);

int ui_xcolor_fade(ui_display_t*, ui_color_t *xcolor, u_int fade_ratio);

#endif
