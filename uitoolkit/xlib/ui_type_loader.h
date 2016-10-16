/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_TYPE_LOADER_H__
#define __UI_TYPE_LOADER_H__

#include "../ui_font.h"
#include "../ui_window.h"

typedef enum ui_type_id {
  TYPE_API_COMPAT_CHECK,
  UI_WINDOW_SET_TYPE,
  UI_WINDOW_DRAW_STRING8,
  UI_WINDOW_DRAW_STRING32,
  UI_WINDOW_RESIZE,
  UI_SET_FONT,
  UI_UNSET_FONT,
  UI_CALCULATE_CHAR_WIDTH,
  UI_WINDOW_SET_CLIP,
  UI_WINDOW_UNSET_CLIP,
  UI_SET_OT_FONT,
  UI_CONVERT_TEXT_TO_GLYPHS,
  MAX_TYPE_FUNCS,

} ui_type_id_t;

#define TYPE_API_VERSION 0x01
#define TYPE_API_COMPAT_CHECK_MAGIC                                         \
  (((TYPE_API_VERSION & 0x0f) << 28) | ((sizeof(ui_font_t) & 0xff) << 20) | \
   ((sizeof(ui_window_t) & 0xff) << 12))

void* ui_load_type_xft_func(ui_type_id_t id);

void* ui_load_type_cairo_func(ui_type_id_t id);

#endif
