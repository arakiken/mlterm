/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "vt_cursor.h"

#include <string.h> /* memset */
#include <pobl/bl_debug.h>

/* --- static functions --- */

static int cursor_goto(vt_cursor_t *cursor, int col_or_idx, int row, int is_by_col) {
  int char_index;
  u_int cols_rest;
  vt_line_t *line;

  if (row > vt_model_end_row(cursor->model)) {
    /* round row to end of row */
    row = vt_model_end_row(cursor->model);
  }

  if ((line = vt_model_get_line(cursor->model, row)) == NULL) {
    return 0;
  }

  if (is_by_col) {
    char_index = vt_convert_col_to_char_index(line, &cols_rest, col_or_idx, BREAK_BOUNDARY);
  } else {
    char_index = col_or_idx;
    cols_rest = 0;
  }

  if (!vt_line_assure_boundary(line, char_index)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " cursor cannot goto char index %d(line length is %d)\n",
                   char_index, line->num_of_filled_chars);
#endif

    char_index = vt_line_end_char_index(line);

    /* In this case, cols_rest is always 0. */
  }

  cursor->char_index = char_index;
  cursor->row = row;
  cursor->col_in_char = cols_rest;
  cursor->col = vt_convert_char_index_to_col(vt_model_get_line(cursor->model, cursor->row),
                                             cursor->char_index, 0) +
                cursor->col_in_char;

  return 1;
}

/* --- global functions --- */

int vt_cursor_init(vt_cursor_t *cursor, vt_model_t *model) {
  memset(cursor, 0, sizeof(vt_cursor_t));
  cursor->model = model;

  return 1;
}

int vt_cursor_final(vt_cursor_t *cursor) {
  /* Do nothing */

  return 1;
}

int vt_cursor_goto_by_char(vt_cursor_t *cursor, int char_index, int row) {
  return cursor_goto(cursor, char_index, row, 0);
}

/* Move horizontally */
int vt_cursor_moveh_by_char(vt_cursor_t *cursor, int char_index) {
  return cursor_goto(cursor, char_index, cursor->row, 0);
}

int vt_cursor_goto_by_col(vt_cursor_t *cursor, int col, int row) {
  return cursor_goto(cursor, col, row, 1);
}

/* Move horizontally */
int vt_cursor_moveh_by_col(vt_cursor_t *cursor, int col) {
  return cursor_goto(cursor, col, cursor->row, 1);
}

int vt_cursor_goto_home(vt_cursor_t *cursor) {
  cursor->row = 0;
  cursor->char_index = 0;
  cursor->col = 0;
  cursor->col_in_char = 0;

  return 1;
}

int vt_cursor_goto_beg_of_line(vt_cursor_t *cursor) {
  cursor->char_index = 0;
  cursor->col = 0;
  cursor->col_in_char = 0;

  return 1;
}

int vt_cursor_go_forward(vt_cursor_t *cursor) {
  /* full width char check. */
  if (cursor->col_in_char + 1 < vt_char_cols(vt_get_cursor_char(cursor))) {
#ifdef __DEBUG
    bl_debug_printf(BL_DEBUG_TAG " cursor is at 2nd byte of multi byte char.\n");
#endif

    cursor->col++;
    cursor->col_in_char++;

    return 1;
  } else if (cursor->char_index < vt_line_end_char_index(vt_get_cursor_line(cursor))) {
    cursor->col = vt_convert_char_index_to_col(vt_get_cursor_line(cursor), ++cursor->char_index, 0);
    cursor->col_in_char = 0;

    return 1;
  } else {
    /* Can't go forward in this line anymore. */

    return 0;
  }
}

int vt_cursor_cr_lf(vt_cursor_t *cursor) {
  if (cursor->model->num_of_rows <= cursor->row + 1) {
    return 0;
  }

  cursor->row++;
  cursor->char_index = 0;
  cursor->col = 0;

  if (!vt_line_assure_boundary(vt_get_cursor_line(cursor), 0)) {
    bl_error_printf("Could cause unexpected behavior.\n");
    return 0;
  }

  return 1;
}

vt_line_t *vt_get_cursor_line(vt_cursor_t *cursor) {
  return vt_model_get_line(cursor->model, cursor->row);
}

vt_char_t *vt_get_cursor_char(vt_cursor_t *cursor) {
  return vt_model_get_line(cursor->model, cursor->row)->chars + cursor->char_index;
}

int vt_cursor_char_is_cleared(vt_cursor_t *cursor) {
  cursor->char_index += cursor->col_in_char;
  cursor->col_in_char = 0;

  return 1;
}

int vt_cursor_left_chars_in_line_are_cleared(vt_cursor_t *cursor) {
  cursor->char_index = cursor->col;
  cursor->col_in_char = 0;

  return 1;
}

int vt_cursor_save(vt_cursor_t *cursor) {
  cursor->saved_col = cursor->col;
  cursor->saved_char_index = cursor->char_index;
  cursor->saved_row = cursor->row;
  cursor->is_saved = 1;

  return 1;
}

int vt_cursor_restore(vt_cursor_t *cursor) {
  if (!cursor->is_saved) {
    return 0;
  }

  if (!vt_cursor_goto_by_col(cursor, cursor->saved_col, cursor->saved_row)) {
    return 0;
  }

  return 1;
}

#ifdef DEBUG

void vt_cursor_dump(vt_cursor_t *cursor) {
  bl_msg_printf("Cursor position => CH_IDX %d COL %d(+%d) ROW %d.\n", cursor->char_index,
                cursor->col, cursor->col_in_char, cursor->row);
}

#endif
