/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_FONT_CACHE_H__
#define __UI_FONT_CACHE_H__

#include "ui.h"

#include <pobl/bl_map.h>
#include <pobl/bl_types.h>
#include <mef/ef_charset.h>

#include "ui_font_config.h"

BL_MAP_TYPEDEF(ui_font, vt_font_t, ui_font_t *);

typedef struct ui_font_cache {
  /*
   * Public(readonly)
   */
  Display *display;
  u_int font_size;
  ef_charset_t usascii_font_cs;
  ui_font_config_t *font_config;
  u_int8_t letter_space;

  ui_font_t *usascii_font;

  /*
   * Private
   */
  BL_MAP(ui_font) uifont_table;
  struct {
    vt_font_t font;
    ui_font_t *uifont;

  } prev_cache;

  u_int ref_count;

} ui_font_cache_t;

void ui_set_use_leftward_double_drawing(int use);

ui_font_cache_t *ui_acquire_font_cache(Display *display, u_int font_size,
                                       ef_charset_t usascii_font_cs, ui_font_config_t *font_config,
                                       u_int letter_space);

void ui_release_font_cache(ui_font_cache_t *font_cache);

void ui_font_cache_unload(ui_font_cache_t *font_cache);

void ui_font_cache_unload_all(void);

ui_font_t *ui_font_cache_get_font(ui_font_cache_t *font_cache, vt_font_t font);

XFontSet ui_font_cache_get_fontset(ui_font_cache_t *font_cache);

#endif
