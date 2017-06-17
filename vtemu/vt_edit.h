/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_EDIT_H__
#define __VT_EDIT_H__

#include <pobl/bl_types.h>

#include "vt_str.h"
#include "vt_line.h"
#include "vt_model.h"
#include "vt_cursor.h"

typedef struct vt_edit_scroll_event_listener {
  void *self;

  void (*receive_scrolled_out_line)(void *, vt_line_t *);
  void (*scrolled_out_lines_finished)(void *);

  int (*window_scroll_upward_region)(void *, int, int, u_int);
  int (*window_scroll_downward_region)(void *, int, int, u_int);

} vt_edit_scroll_event_listener_t;

typedef struct vt_edit {
  vt_model_t model;
  vt_cursor_t cursor;

  u_int tab_size;
  u_int8_t *tab_stops;

  vt_char_t bce_ch;

  /* used for line overlapping */
  vt_line_t *wraparound_ready_line;

  int16_t vmargin_beg;
  int16_t vmargin_end;

  vt_edit_scroll_event_listener_t *scroll_listener;

  int16_t hmargin_beg;
  int16_t hmargin_end;
  int8_t use_margin;

  int8_t is_logging;
  int8_t is_relative_origin;
  int8_t is_auto_wrap;
  int8_t use_bce;
  int8_t use_rect_attr_select;

} vt_edit_t;

int vt_edit_init(vt_edit_t *edit, vt_edit_scroll_event_listener_t *scroll_listener,
                 u_int num_of_cols, u_int num_of_rows, u_int tab_size, int is_logging, int use_bce);

int vt_edit_final(vt_edit_t *edit);

int vt_edit_clone(vt_edit_t *dst_edit, vt_edit_t *src_edit);

int vt_edit_resize(vt_edit_t *edit, u_int num_of_cols, u_int num_of_rows);

int vt_edit_insert_chars(vt_edit_t *edit, vt_char_t *chars, u_int num_of_chars);

int vt_edit_insert_blank_chars(vt_edit_t *edit, u_int num_of_blank_chars);

int vt_edit_overwrite_chars(vt_edit_t *edit, vt_char_t *chars, u_int num_of_chars);

int vt_edit_delete_cols(vt_edit_t *edit, u_int delete_cols);

int vt_edit_clear_cols(vt_edit_t *edit, u_int cols);

int vt_edit_insert_new_line(vt_edit_t *edit);

int vt_edit_delete_line(vt_edit_t *edit);

int vt_edit_clear_line_to_right(vt_edit_t *edit);

int vt_edit_clear_line_to_left(vt_edit_t *edit);

int vt_edit_clear_below(vt_edit_t *edit);

int vt_edit_clear_above(vt_edit_t *edit);

int vt_edit_set_vmargin(vt_edit_t *edit, int beg, int end);

int vt_edit_scroll_upward(vt_edit_t *edit, u_int size);

int vt_edit_scroll_downward(vt_edit_t *edit, u_int size);

int vt_edit_scroll_leftward(vt_edit_t *edit, u_int size);

int vt_edit_scroll_rightward(vt_edit_t *edit, u_int size);

int vt_edit_scroll_leftward_from_cursor(vt_edit_t *edit, u_int size);

int vt_edit_scroll_rightward_from_cursor(vt_edit_t *edit, u_int size);

int vt_edit_set_use_hmargin(vt_edit_t *edit, int use);

int vt_edit_set_hmargin(vt_edit_t *edit, int beg, int end);

int vt_edit_forward_tabs(vt_edit_t *edit, u_int num);

int vt_edit_backward_tabs(vt_edit_t *edit, u_int num);

#define vt_edit_get_tab_size(edit) ((edit)->tab_size)

int vt_edit_set_tab_size(vt_edit_t *edit, u_int tab_size);

int vt_edit_set_tab_stop(vt_edit_t *edit);

int vt_edit_clear_tab_stop(vt_edit_t *edit);

int vt_edit_clear_all_tab_stops(vt_edit_t *edit);

#define vt_edit_get_line(edit, row) (vt_model_get_line(&(edit)->model, row))

int vt_edit_set_modified_all(vt_edit_t *edit);

#define vt_edit_get_cols(edit) ((edit)->model.num_of_cols)

#define vt_edit_get_rows(edit) ((edit)->model.num_of_rows)

int vt_edit_go_forward(vt_edit_t *edit, int flag);

int vt_edit_go_back(vt_edit_t *edit, int flag);

int vt_edit_go_upward(vt_edit_t *edit, int flag);

int vt_edit_go_downward(vt_edit_t *edit, int flag);

int vt_edit_goto_beg_of_line(vt_edit_t *edit);

int vt_edit_goto_home(vt_edit_t *edit);

int vt_edit_goto(vt_edit_t *edit, int col, int row);

int vt_edit_set_relative_origin(vt_edit_t *edit);

int vt_edit_set_absolute_origin(vt_edit_t *edit);

int vt_edit_set_auto_wrap(vt_edit_t *edit, int flag);

#define vt_edit_is_auto_wrap(edit) ((edit)->is_auto_wrap)

#define vt_edit_set_use_bce(edit, use) ((edit)->use_bce = (use))

#define vt_edit_is_using_bce(edit) ((edit)->use_bce)

int vt_edit_set_bce_fg_color(vt_edit_t *edit, vt_color_t fg_color);

int vt_edit_set_bce_bg_color(vt_edit_t *edit, vt_color_t bg_color);

int vt_edit_save_cursor(vt_edit_t *edit);

int vt_edit_restore_cursor(vt_edit_t *edit);

int vt_edit_fill_area(vt_edit_t *edit, vt_char_t *ch, int col, int row, u_int num_of_cols,
                      u_int num_of_rows);

int vt_edit_copy_area(vt_edit_t *src_edit, int src_col, int src_row, u_int num_of_copy_cols,
                      u_int num_of_copy_rows, vt_edit_t *dst_edit, int dst_col, int dst_row);

int vt_edit_erase_area(vt_edit_t *edit, int col, int row, u_int num_of_cols, u_int num_of_rows);

int vt_edit_change_attr_area(vt_edit_t *edit, int col, int row, u_int num_of_cols,
                             u_int num_of_rows, void (*func)(vt_char_t *, int, int, int, int),
                             int attr);

void vt_edit_clear_size_attr(vt_edit_t *edit);

#define vt_edit_set_use_rect_attr_select(edit, use) ((edit)->use_rect_attr_select = (use))

#define vt_edit_is_using_rect_attr_select(edit) ((edit)->use_rect_attr_select)

#define vt_cursor_char_index(edit) ((edit)->cursor.char_index)

#define vt_cursor_col(edit) ((edit)->cursor.col)

#define vt_cursor_row(edit) ((edit)->cursor.row)

#define vt_cursor_relative_col(edit) ((edit)->cursor.col - (edit)->hmargin_beg)

#define vt_cursor_relative_row(edit) ((edit)->cursor.row - (edit)->vmargin_beg)

#ifdef DEBUG

void vt_edit_dump(vt_edit_t *edit);

void vt_edit_dump_updated(vt_edit_t *edit);

#endif

#endif
