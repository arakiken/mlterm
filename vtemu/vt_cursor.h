/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_CURSOR_H__
#define __VT_CURSOR_H__

#include "vt_model.h"

typedef struct vt_cursor {
  /*
   * XXX
   * Following members are modified directly in vt_logical_visual.c
   * without vt_cursor_xxx() functions.
   */

  /*
   * public (readonly)
   */
  int row;
  int char_index;
  int col;
  int col_in_char;

  /*
   * private
   */
  int saved_row;
  int saved_char_index;
  int saved_col;
  int8_t is_saved;

  vt_model_t *model;

} vt_cursor_t;

void vt_cursor_init(vt_cursor_t *cursor, vt_model_t *model);

void vt_cursor_final(vt_cursor_t *cursor);

int vt_cursor_goto_by_char(vt_cursor_t *cursor, int char_index, int row);

int vt_cursor_moveh_by_char(vt_cursor_t *cursor, int char_index);

int vt_cursor_goto_by_col(vt_cursor_t *cursor, int col, int row);

int vt_cursor_moveh_by_col(vt_cursor_t *cursor, int col);

void vt_cursor_goto_home(vt_cursor_t *cursor);

void vt_cursor_goto_beg_of_line(vt_cursor_t *cursor);

int vt_cursor_go_forward(vt_cursor_t *cursor);

int vt_cursor_cr_lf(vt_cursor_t *cursor);

vt_line_t *vt_get_cursor_line(vt_cursor_t *cursor);

vt_char_t *vt_get_cursor_char(vt_cursor_t *cursor);

void vt_cursor_char_is_cleared(vt_cursor_t *cursor);

void vt_cursor_left_chars_in_line_are_cleared(vt_cursor_t *cursor);

void vt_cursor_save(vt_cursor_t *cursor);

#define vt_saved_cursor_to_home(cursor) \
  ((cursor)->saved_col = (cursor)->saved_char_index = (cursor)->saved_row = 0)

int vt_cursor_restore(vt_cursor_t *cursor);

#ifdef DEBUG

void vt_cursor_dump(vt_cursor_t *cursor);

#endif

#endif
