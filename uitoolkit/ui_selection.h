/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_SELECTION_H__
#define __UI_SELECTION_H__

#include <pobl/bl_types.h> /* u_int */
#include <vt_str.h>

#include "ui.h"

typedef enum {
  SEL_CHAR = 0x1,
  SEL_WORD = 0x2,
  SEL_LINE = 0x3,

} ui_sel_mode_t;

typedef struct ui_sel_event_listener {
  void *self;
  int (*select_in_window)(void *);
  void (*reverse_color)(void *, int, int, int, int, int);
  void (*restore_color)(void *, int, int, int, int, int);

} ui_sel_event_listener_t;

typedef struct ui_selection {
  ui_sel_event_listener_t *sel_listener;

  vt_char_t *sel_str;
  u_int sel_len;

  /*
   * Be careful that value of col must be munis in rtl line.
   * +-----------------------------+
   * |          a  a  a  a  a  a  a|<= RTL line
   *           -1 -2 -3 -4 -5 -6 -7 <= index
   */

  int base_col_l;
  int base_row_l;
  int base_col_r;
  int base_row_r;
  int beg_col;
  int beg_row;
  int end_col;
  int end_row;
  int lock_col;
  int lock_row;

  int prev_col;
  int prev_row;

  int8_t is_selecting; /* ui_sel_mode_t is stored */
  int8_t is_reversed;
  int8_t is_locked;
  int8_t is_rect;
#ifdef SELECTION_STYLE_CHANGEABLE
  int8_t str_not_updated;
#endif

} ui_selection_t;


#ifdef SELECTION_STYLE_CHANGEABLE
void ui_set_change_selection_immediately(int flag);
#endif

void ui_sel_init(ui_selection_t *sel, ui_sel_event_listener_t *listener);

void ui_sel_final(ui_selection_t *sel);

void ui_start_selection(ui_selection_t *sel, int col_l, int row_l, int col_r, int row_r,
                        ui_sel_mode_t mode, int is_rect);

int ui_selecting(ui_selection_t *sel, int col, int row);

int ui_stop_selecting(ui_selection_t *sel);

int ui_restore_selected_region_color_except_logs(ui_selection_t *sel);

int ui_reverse_selected_region_color_except_logs(ui_selection_t *sel);

int ui_restore_selected_region_color(ui_selection_t *sel);

int ui_reverse_selected_region_color(ui_selection_t *sel);

void ui_selection_set_str(ui_selection_t *sel, vt_char_t *str, u_int len);

#define ui_selection_has_str(sel) ((sel)->sel_str != NULL && (sel)->sel_len > 0)

#ifdef SELECTION_STYLE_CHANGEABLE
#define ui_selection_str_is_not_updated(sel) ((sel)->str_not_updated)
#else
#define ui_selection_str_is_not_updated(sel) (0)
#endif

int ui_sel_clear(ui_selection_t *sel);

int ui_selected_region_is_changed(ui_selection_t *sel, int col, int row, u_int base);

void ui_sel_line_scrolled_out(ui_selection_t *sel, int min_row);

#define ui_is_selecting(sel) ((sel)->is_selecting)

#define ui_sel_is_reversed(sel) ((sel)->is_reversed)

int ui_is_after_sel_right_base_pos(ui_selection_t *sel, int col, int row);

int ui_is_before_sel_left_base_pos(ui_selection_t *sel, int col, int row);

void ui_sel_lock(ui_selection_t *sel);

#endif
