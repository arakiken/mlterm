/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_COLOR_CACHE_H__
#define __UI_COLOR_CACHE_H__

#include <vt_color.h>
#include "ui_color.h"

typedef struct ui_color_cache_256ext {
  ui_color_t xcolors[MAX_256EXT_COLORS];
  u_int8_t is_loaded[MAX_256EXT_COLORS];

  u_int ref_count;

} ui_color_cache_256ext_t;

typedef struct ui_color_cache {
  ui_display_t *disp;

  ui_color_t xcolors[MAX_VTSYS_COLORS];
  u_int8_t is_loaded[MAX_VTSYS_COLORS];

  ui_color_cache_256ext_t *cache_256ext;

  ui_color_t black;

  u_int8_t fade_ratio;

  u_int16_t ref_count; /* 0 - 65535 */

} ui_color_cache_t;

ui_color_cache_t *ui_acquire_color_cache(ui_display_t *disp, u_int8_t fade_ratio);

void ui_release_color_cache(ui_color_cache_t *color_cache);

void ui_color_cache_unload(ui_color_cache_t *color_cache);

void ui_color_cache_unload_all(void);

int ui_load_xcolor(ui_color_cache_t *color_cache, ui_color_t *xcolor, char *name);

ui_color_t *ui_get_cached_xcolor(ui_color_cache_t *color_cache, vt_color_t color);

#endif
