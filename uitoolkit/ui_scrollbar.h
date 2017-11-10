/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_SCROLLBAR_H__
#define __UI_SCROLLBAR_H__

#include <pobl/bl_types.h> /* u_int */
#include <vt_term.h>

#include "ui_window.h"
#include "ui_sb_view.h"
#include "ui_sb_mode.h"
#include "ui_color_manager.h"
#include "ui_picture.h"

typedef struct ui_scrollbar_event_listener {
  void *self;
  int (*screen_scroll_to)(void *, int);
  int (*screen_scroll_upward)(void *, u_int);
  int (*screen_scroll_downward)(void *, u_int);
  int (*screen_is_static)(void *);

} ui_scrollbar_event_listener_t;

typedef struct ui_scrollbar {
  ui_window_t window;

  char *view_name;
  ui_sb_view_t *view;

  char *fg_color;
  char *bg_color;
  ui_color_t fg_xcolor;
  ui_color_t bg_xcolor;

  ui_scrollbar_event_listener_t *sb_listener;

  u_int bar_height;    /* Scrollbar height */
  u_int top_margin;    /* Button area */
  u_int bottom_margin; /* Button area */
  u_int line_height;
  u_int num_scr_lines;
  u_int num_log_lines;
  u_int num_filled_log_lines;
  int bar_top_y; /* Scrollbar position without button area */
  int y_on_bar;  /* Used in button_motion event handler */
  int current_row;
  int redraw_y;
  u_int redraw_height;

  int up_button_y;
  u_int up_button_height;
  int down_button_y;
  u_int down_button_height;
  int8_t is_pressing_up_button;
  int8_t is_pressing_down_button;

  int8_t is_motion;

} ui_scrollbar_t;

int ui_scrollbar_init(ui_scrollbar_t *sb, ui_scrollbar_event_listener_t *sb_listener,
                      char *view_name, char *fg_color, char *bg_color, u_int height,
                      u_int line_height, u_int num_log_lines, u_int num_filled_log_lines,
                      int use_transbg, ui_picture_modifier_t *pic_mod);

void ui_scrollbar_final(ui_scrollbar_t *sb);

void ui_scrollbar_set_num_log_lines(ui_scrollbar_t *sb, u_int num_log_lines);

void ui_scrollbar_set_num_filled_log_lines(ui_scrollbar_t *sb, u_int num_filled_log_lines);

int ui_scrollbar_line_is_added(ui_scrollbar_t *sb);

void ui_scrollbar_reset(ui_scrollbar_t *sb);

int ui_scrollbar_move_upward(ui_scrollbar_t *sb, u_int size);

int ui_scrollbar_move_downward(ui_scrollbar_t *sb, u_int size);

int ui_scrollbar_move(ui_scrollbar_t *sb, int row);

int ui_scrollbar_set_line_height(ui_scrollbar_t *sb, u_int line_height);

int ui_scrollbar_set_fg_color(ui_scrollbar_t *sb, char *fg_color);

int ui_scrollbar_set_bg_color(ui_scrollbar_t *sb, char *bg_color);

int ui_scrollbar_change_view(ui_scrollbar_t *sb, char *name);

int ui_scrollbar_set_transparent(ui_scrollbar_t *sb, ui_picture_modifier_t *pic_mod, int force);

int ui_scrollbar_unset_transparent(ui_scrollbar_t *sb);

#endif
