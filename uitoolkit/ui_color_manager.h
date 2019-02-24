/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_COLOR_MANAGER_H__
#define __UI_COLOR_MANAGER_H__

#include <vt_color.h>

#include "ui_color.h"
#include "ui_color_cache.h"

typedef struct ui_color_manager {
  /* normal or faded color cache */
  ui_color_cache_t *color_cache;
  ui_color_cache_t *alt_color_cache;

  /* for fg, bg, cursor_fg and cursor_bg */
  struct sys_color {
    ui_color_t xcolor;
    char *name;

  } sys_colors[10];

  u_int8_t alpha;
  int8_t is_reversed;

} ui_color_manager_t;

ui_color_manager_t *ui_color_manager_new(ui_display_t *disp, char *fg_color, char *bg_color,
                                         char *cursor_fg_color, char *cursor_bg_color,
                                         char *bd_color, char *ul_color, char *bl_color,
                                         char *rv_color, char *it_color, char *co_color);

void ui_color_manager_destroy(ui_color_manager_t *color_man);

int ui_color_manager_set_fg_color(ui_color_manager_t *color_man, char *name);

int ui_color_manager_set_bg_color(ui_color_manager_t *color_man, char *name);

int ui_color_manager_set_cursor_fg_color(ui_color_manager_t *color_man, char *name);

int ui_color_manager_set_cursor_bg_color(ui_color_manager_t *color_man, char *name);

int ui_color_manager_set_alt_color(ui_color_manager_t *color_man, vt_color_t color, char *name);

char *ui_color_manager_get_fg_color(ui_color_manager_t *color_man);

char *ui_color_manager_get_bg_color(ui_color_manager_t *color_man);

char *ui_color_manager_get_cursor_fg_color(ui_color_manager_t *color_man);

char *ui_color_manager_get_cursor_bg_color(ui_color_manager_t *color_man);

char *ui_color_manager_get_alt_color(ui_color_manager_t *color_man, vt_color_t color);

ui_color_t *ui_get_xcolor(ui_color_manager_t *color_man, vt_color_t color);

int ui_color_manager_fade(ui_color_manager_t *color_man, u_int fade_ratio);

int ui_color_manager_unfade(ui_color_manager_t *color_man);

int ui_color_manager_reverse_video(ui_color_manager_t *color_man);

int ui_color_manager_restore_video(ui_color_manager_t *color_man);

int ui_color_manager_adjust_cursor_fg_color(ui_color_manager_t *color_man);

int ui_color_manager_adjust_cursor_bg_color(ui_color_manager_t *color_man);

void ui_color_manager_reload(ui_color_manager_t *color_man);

int ui_change_true_transbg_alpha(ui_color_manager_t *color_man, u_int8_t alpha);

#endif
