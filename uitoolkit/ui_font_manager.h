/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_FONT_MANAGER_H__
#define __UI_FONT_MANAGER_H__

#include "ui.h"

#include <pobl/bl_types.h>
#include <vt_char_encoding.h>

#include "ui_font_cache.h"

typedef struct ui_font_manager {
  ui_font_cache_t *font_cache;

  ui_font_config_t *font_config;

  u_int8_t step_in_changing_font_size;
  int8_t use_bold_font;
  int8_t use_italic_font;
  int8_t size_attr;
#ifdef USE_OT_LAYOUT
  int8_t use_ot_layout;
#endif

} ui_font_manager_t;

int ui_set_font_size_range(u_int min_font_size, u_int max_font_size);

ui_font_manager_t *ui_font_manager_new(Display *display, ui_type_engine_t type_engine,
                                       ui_font_present_t font_present, u_int font_size,
                                       ef_charset_t usascii_font_cs,
                                       u_int step_in_changing_font_size, u_int letter_space,
                                       int use_bold_font, int use_italic_font);

void ui_font_manager_destroy(ui_font_manager_t *font_man);

void ui_font_manager_set_attr(ui_font_manager_t *font_man, int size_attr, int use_ot_layout);

ui_font_t *ui_get_font(ui_font_manager_t *font_man, vt_font_t font);

#define ui_get_usascii_font(font_man) ((font_man)->font_cache->usascii_font)

int ui_font_manager_usascii_font_cs_changed(ui_font_manager_t *font_man,
                                            ef_charset_t usascii_font_cs);

int ui_change_font_present(ui_font_manager_t *font_man, ui_type_engine_t type_engine,
                           ui_font_present_t font_present);

ui_type_engine_t ui_get_type_engine(ui_font_manager_t *font_man);

ui_font_present_t ui_get_font_present(ui_font_manager_t *font_man);

int ui_change_font_size(ui_font_manager_t *font_man, u_int font_size);

int ui_larger_font(ui_font_manager_t *font_man);

int ui_smaller_font(ui_font_manager_t *font_man);

u_int ui_get_font_size(ui_font_manager_t *font_man);

int ui_set_letter_space(ui_font_manager_t *font_man, u_int letter_space);

#define ui_get_letter_space(font_man) ((font_man)->font_cache->letter_space)

int ui_set_use_bold_font(ui_font_manager_t *font_man, int use_bold_font);

#define ui_is_using_bold_font(font_man) ((font_man)->use_bold_font)

int ui_set_use_italic_font(ui_font_manager_t *font_man, int use_italic_font);

#define ui_is_using_italic_font(font_man) ((font_man)->use_italic_font)

#define ui_get_fontset(font_man) ui_font_cache_get_fontset((font_man)->font_cache)

#define ui_get_current_usascii_font_cs(font_man) ((font_man)->font_cache->usascii_font_cs)

ef_charset_t ui_get_usascii_font_cs(vt_char_encoding_t encoding);

#define ui_font_manager_dump_font_config(font_man) ui_font_config_dump((font_man)->font_config)

#endif
