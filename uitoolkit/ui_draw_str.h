/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_DRAW_STR_H__
#define __UI_DRAW_STR_H__

#include <vt_char.h>
#include "ui_window.h"
#include "ui_font_manager.h"
#include "ui_color_manager.h"

int ui_draw_str(ui_window_t *window, ui_font_manager_t *font_man, ui_color_manager_t *color_man,
                vt_char_t *chars, u_int num_of_chars, int x, int y, u_int height, u_int ascent,
                u_int top_margin, u_int bottom_margin, int hide_underline);

int ui_draw_str_to_eol(ui_window_t *window, ui_font_manager_t *font_man,
                       ui_color_manager_t *color_man, vt_char_t *chars, u_int num_of_chars, int x,
                       int y, u_int height, u_int ascent, u_int top_margin, u_int bottom_margin,
                       int hide_underline);

u_int ui_calculate_mlchar_width(ui_font_t *font, vt_char_t *ch, int *draw_alone);

#endif
