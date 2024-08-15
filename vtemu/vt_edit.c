/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "vt_edit.h"

#include <string.h>      /* memmove/memset */
#include <pobl/bl_mem.h> /* alloca */
#include <pobl/bl_debug.h>
#include <pobl/bl_util.h>
#include <mef/ef_charset.h> /* ef_charset_t */

#include "vt_edit_util.h"
#include "vt_edit_scroll.h"

#if 0
#define __DEBUG
#endif

#if 0
#define CURSOR_DEBUG
#endif

#if 0
#define COMPAT_XTERM
#endif

/*
 * vt_edit_t::tab_stops
 * e.g.)
 * 1 line = 40 columns
 * => tab_stops = u_int8_t * 5 (40bits)
 * => Check tab_stops bits if you want to know whether a column is set tab stop
 * or not.
 */
#define TAB_STOPS_SIZE(edit) (((edit)->model.num_cols - 1) / 8 + 1)

#define reset_wraparound_checker(edit) ((edit)->wraparound_ready_line = NULL)

#define MARGIN_IS_ENABLED(edit) \
  ((edit)->use_margin &&        \
   (0 < (edit)->hmargin_beg || (edit)->hmargin_end + 1 < (edit)->model.num_cols))

#define CURSOR_IS_INSIDE_HMARGIN(edit) \
  ((edit)->hmargin_beg <= (edit)->cursor.col && (edit)->cursor.col <= (edit)->hmargin_end)

#define CURSOR_IS_INSIDE_VMARGIN(edit) \
  ((edit)->vmargin_beg <= (edit)->cursor.row && (edit)->cursor.row <= (edit)->vmargin_end)

/* --- static variables --- */

static char *resize_mode_name_table[] = {
  "none", "scroll", "wrap",
};
static vt_resize_mode_t resize_mode = RZ_WRAP;

/* --- static functions --- */

/*
 * Insert chars within a line.
 * The cursor must be inside the left and right margins. (The caller of this
 * function must check it in advance.)
 */
static int insert_chars(vt_edit_t *edit, vt_char_t *ins_chars, u_int num_ins_chars,
                        int do_move_cursor) {
  vt_char_t *buffer;
  u_int buf_len;
  u_int num_cols;
  u_int filled_len;
  u_int filled_cols;
  u_int last_index;
  u_int cols_after;
  u_int cols;
  int cursor_col;
  int count;
  vt_line_t *cursor_line;

#ifdef CURSOR_DEBUG
  vt_cursor_dump(&edit->cursor);
#endif

  cursor_line = CURSOR_LINE(edit);

  buf_len = edit->model.num_cols;

#ifndef COMPAT_XTERM
  if (edit->cursor.col > edit->hmargin_end) {
    num_cols = edit->model.num_cols;
  } else
#endif
  {
    num_cols = edit->hmargin_end + 1;
  }

  if ((buffer = vt_str_alloca(buf_len)) == NULL) {
    return 0;
  }
  vt_str_init(buffer, buf_len);

  filled_len = 0;
  filled_cols = 0;
  cursor_col = edit->cursor.col;

  /*
   * before cursor(excluding cursor)
   */

  if (edit->cursor.col_in_char) {
#ifdef DEBUG
    if (vt_char_cols(CURSOR_CHAR(edit)) <= edit->cursor.col_in_char) {
      bl_warn_printf(BL_DEBUG_TAG " illegal col_in_char.\n");
    }
#endif

    /*
     * padding spaces for former half of cursor.
     */
    for (count = 0; count < edit->cursor.col_in_char; count++) {
      vt_char_copy(&buffer[filled_len++], vt_sp_ch());
    }

    filled_cols += count;
    cols_after = vt_char_cols(CURSOR_CHAR(edit)) - count;
    cursor_col -= count;
  } else {
    cols_after = 0;
  }

  /*
   * chars are appended one by one below since the line may be full.
   */

  /*
   * inserted chars
   */

  for (count = 0; count < num_ins_chars; count++) {
    cols = vt_char_cols(&ins_chars[count]);
    if (cursor_col + filled_cols + cols > num_cols) {
      /*
       * ---+      ---+
       *    |         |
       * abcde  => abe|
       */
      if (filled_len > 0) {
        vt_char_copy(&buffer[filled_len - 1], &ins_chars[num_ins_chars - 1]);
      }

      break;
    }

    vt_char_copy(&buffer[filled_len++], &ins_chars[count]);
    filled_cols += cols;
  }

  if (edit->cursor.char_index + filled_len == num_cols) {
    /* cursor position doesn't proceed. */
    last_index = filled_len - 1;
  } else {
    last_index = filled_len;
  }

  /*
   * cursor char
   */

  if (cols_after) {
    /*
     * padding spaces for latter half of cursor.
     */
    for (count = 0; count < cols_after; count++) {
      /* + 1 is for vt_sp_ch() */
      if (cursor_col + filled_cols + 1 > num_cols) {
        goto line_full;
      }

      vt_char_copy(&buffer[filled_len++], vt_sp_ch());
    }
    filled_cols += count;
  } else {
    cols = vt_char_cols(CURSOR_CHAR(edit));
    if (cursor_col + filled_cols + cols > num_cols) {
      goto line_full;
    }

    vt_char_copy(&buffer[filled_len++], CURSOR_CHAR(edit));
    filled_cols += cols;
  }

  /*
   * after cursor(excluding cursor)
   */

  for (count = edit->cursor.char_index + 1; count < cursor_line->num_filled_chars; count++) {
    cols = vt_char_cols(vt_char_at(cursor_line, count));
    if (cursor_col + filled_cols + cols > num_cols) {
      break;
    }

    vt_char_copy(&buffer[filled_len++], vt_char_at(cursor_line, count));
    filled_cols += cols;
  }

line_full:
  /*
   * Updating current line and cursor.
   */

  vt_line_overwrite(cursor_line, edit->cursor.char_index, buffer, filled_len, filled_cols);

  vt_str_final(buffer, buf_len);

  if (do_move_cursor) {
    vt_cursor_moveh_by_char(&edit->cursor, edit->cursor.char_index + last_index);
  } else if (edit->cursor.col_in_char) {
    vt_cursor_moveh_by_char(&edit->cursor, edit->cursor.char_index + edit->cursor.col_in_char);
  }

#ifdef CURSOR_DEBUG
  vt_cursor_dump(&edit->cursor);
#endif

  return 1;
}

static int horizontal_tabs(vt_edit_t *edit, u_int num, int is_forward) {
  int col;
  u_int count;

  if (edit->wraparound_ready_line) {
    vt_edit_go_downward(edit, SCROLL);
    vt_edit_goto_beg_of_line(edit);
  }

  /*
   * Compatible with rlogin 2.23.1.
   *
   * To be compatible with xterm-332, enclose by #if 0 ... #endif.
   * (esctest: DECSETTests.test_DECSET_DECAWM_NoLineWrapOnTabWithLeftRightMargin)
   */
#if 1
  if (edit->cursor.col < edit->hmargin_beg) {
    vt_cursor_goto_by_col(&edit->cursor, edit->hmargin_beg, edit->cursor.row);
  } else if (edit->cursor.col > edit->hmargin_end) {
    vt_cursor_goto_by_col(&edit->cursor, edit->hmargin_end, edit->cursor.row);
  }
#endif

  col = edit->cursor.col;

  for (count = 0; count < num; count++) {
    while (1) {
      if (is_forward) {
        if (col >= edit->hmargin_end) {
          return 1;
        }

        col++;
        vt_edit_go_forward(edit, WRAPAROUND);
      } else {
        if (col <= edit->hmargin_beg) {
          return 1;
        }

        col--;
        vt_edit_go_back(edit, WRAPAROUND);
      }

      if (vt_edit_is_tab_stop(edit, col)) {
        break;
      }
    }
  }

  return 1;
}

static void copy_area(vt_edit_t *src_edit, int src_col, int src_row, u_int num_copy_cols,
                      u_int num_copy_rows, vt_edit_t *dst_edit, int dst_col, int dst_row) {
  u_int count;
  vt_line_t *src_line;
  vt_line_t *dst_line;
  int src_char_index;
  int dst_char_index;
  u_int src_cols_rest;
  u_int src_cols_after;
  u_int dst_cols_rest;
  u_int num_src_chars;
  u_int num_src_cols;

  for (count = 0; count < num_copy_rows; count++) {
    int srow;
    int drow;

    if (src_row < dst_row) {
      srow = src_row + num_copy_rows - count - 1;
      drow = dst_row + num_copy_rows - count - 1;
    } else {
      srow = src_row + count;
      drow = dst_row + count;
    }

    if (!(src_line = vt_edit_get_line(src_edit, srow)) ||
        !(dst_line = vt_edit_get_line(dst_edit, drow))) {
      continue;
    }

    /* Beginning of src line */

    src_char_index =
        vt_convert_col_to_char_index(src_line, &src_cols_rest, src_col, BREAK_BOUNDARY);
    if (src_char_index >= src_line->num_filled_chars) {
      src_cols_after = num_copy_cols;
    } else if (src_cols_rest > 0) {
      src_cols_after = vt_char_cols(src_line->chars + src_char_index) - src_cols_rest;
      src_char_index++;
    } else {
      src_cols_after = 0;
    }

    /* Beginning of dst line */

    dst_char_index = vt_convert_col_to_char_index(dst_line, &dst_cols_rest, dst_col, 0);

    /* Fill rest at the beginning */

    if (dst_cols_rest + src_cols_after > 0) {
      vt_line_fill(dst_line, vt_sp_ch(), dst_char_index, dst_cols_rest + src_cols_after);
      if (src_char_index >= src_line->num_filled_chars) {
        continue;
      }

      dst_char_index += (dst_cols_rest + src_cols_after);
    }

    /* End of src line */

    num_src_chars =
        vt_convert_col_to_char_index(src_line, &src_cols_rest, /* original value is replaced. */
                                     src_col + num_copy_cols - 1, 0) +
        1 - src_char_index;
    if (src_cols_rest == 0) {
      if ((src_cols_rest =
               vt_char_cols(src_line->chars + src_char_index + num_src_chars - 1) - 1) > 0) {
        num_src_chars--;
      }
    } else {
      src_cols_rest = 0;
    }
    num_src_cols = num_copy_cols - src_cols_after - src_cols_rest;

    /* Copy src to dst */

    if (num_src_cols > 0) {
      vt_line_overwrite(dst_line, dst_char_index, src_line->chars + src_char_index,
                        num_src_chars, num_src_cols);
    }

    /* Fill rest at the end */

    if (src_cols_rest > 0) {
      vt_line_fill(dst_line, vt_sp_ch(), dst_char_index + num_src_chars, src_cols_rest);
    }
  }
}

static void erase_area(vt_edit_t *edit, int col, int row, u_int num_cols, u_int num_rows) {
  u_int count;
  vt_line_t *line;
  int char_index;
  u_int cols_rest;

  for (count = 0; count < num_rows; count++) {
    if (!(line = vt_edit_get_line(edit, row + count))) {
      continue;
    }

    char_index = vt_convert_col_to_char_index(line, &cols_rest, col, BREAK_BOUNDARY);

    if (char_index >= line->num_filled_chars && !edit->use_bce) {
      continue;
    }

    if (cols_rest > 0) {
      vt_line_fill(line, edit->use_bce ? &edit->bce_ch : vt_sp_ch(), char_index, cols_rest);
      char_index += cols_rest;
    }

    vt_line_fill(line, edit->use_bce ? &edit->bce_ch : vt_sp_ch(), char_index, num_cols);
  }
}

static int scroll_downward_region(vt_edit_t *edit, u_int size, int is_cursor_beg,
                                  int ignore_cursor_pos) {
  int vmargin_beg;

  if (is_cursor_beg) {
    if (edit->cursor.row < edit->vmargin_beg) {
      return 0;
    }

    vmargin_beg = edit->cursor.row;
  } else {
    vmargin_beg = edit->vmargin_beg;
  }

  /*
   * XXX
   * CURSOR_IS_INSIDE_HMARGIN(edit) disables vim to scroll the right side of
   * vertically split window.
   */
  if (ignore_cursor_pos ||
      (/* CURSOR_IS_INSIDE_HMARGIN(edit) && */
       edit->cursor.row >= vmargin_beg && edit->cursor.row <= edit->vmargin_end)) {
    if (size > edit->vmargin_end - vmargin_beg + 1) {
      size = edit->vmargin_end - vmargin_beg + 1;
    } else {
      copy_area(edit, edit->hmargin_beg, vmargin_beg, edit->hmargin_end - edit->hmargin_beg + 1,
                edit->vmargin_end - vmargin_beg + 1 - size,
                edit, edit->hmargin_beg, vmargin_beg + size);
    }

    erase_area(edit, edit->hmargin_beg, vmargin_beg, edit->hmargin_end - edit->hmargin_beg + 1,
               size);

    return 1;
  } else {
    return 0;
  }
}

static int scroll_upward_region(vt_edit_t *edit, u_int size, int is_cursor_beg,
                                int ignore_cursor_pos) {
  int vmargin_beg;

  if (is_cursor_beg) {
    if (edit->cursor.row < edit->vmargin_beg) {
      return 0;
    }

    vmargin_beg = edit->cursor.row;
  } else {
    vmargin_beg = edit->vmargin_beg;
  }

  if (ignore_cursor_pos ||
      (CURSOR_IS_INSIDE_HMARGIN(edit) &&
       edit->cursor.row >= vmargin_beg && edit->cursor.row <= edit->vmargin_end)) {
    if (size > edit->vmargin_end - vmargin_beg + 1) {
      size = edit->vmargin_end - vmargin_beg + 1;
    } else {
      copy_area(edit, edit->hmargin_beg, vmargin_beg + size,
                edit->hmargin_end - edit->hmargin_beg + 1,
                edit->vmargin_end - vmargin_beg + 1 - size,
                edit, edit->hmargin_beg, vmargin_beg);
    }

    erase_area(edit, edit->hmargin_beg, edit->vmargin_end + 1 - size,
               edit->hmargin_end - edit->hmargin_beg + 1, size);

    return 1;
  } else {
    return 0;
  }
}

static int apply_relative_origin(vt_edit_t *edit, int *col, int *row, u_int *num_cols,
                                 u_int *num_rows) {
  if (edit->is_relative_origin) {
    if (((*row) += edit->vmargin_beg) > edit->vmargin_end ||
        ((*col) += edit->hmargin_beg) > edit->hmargin_end) {
      return 0;
    }

    if ((*row) + (*num_rows) > edit->vmargin_end + 1) {
      (*num_rows) = edit->vmargin_end + 1 - (*row);
    }

    if ((*col) + (*num_cols) > edit->hmargin_end + 1) {
      (*num_cols) = edit->hmargin_end + 1 - (*col);
    }
  } else {
    if ((*row) >= edit->model.num_rows || (*col) >= edit->model.num_cols) {
      return 0;
    }

    if ((*row) + (*num_rows) > edit->model.num_rows) {
      (*num_rows) = edit->model.num_rows - (*row);
    }

    if ((*col) + (*num_cols) > edit->model.num_cols) {
      (*num_cols) = edit->model.num_cols - (*col);
    }
  }

  return 1;
}

/*
 * max_cols is always greater than 0, and it never becomes less than 0 at the last call
 * of this function.
 * vt_line_set_modified() should be executed by the caller of this function.
 */
static u_int replace_chars_in_line(vt_line_t *line, vt_char_t **chars, u_int max_cols) {
  u_int num_chars;

  vt_line_reset(line);
  num_chars = vt_str_cols_to_len(*chars, &max_cols);
  vt_line_overwrite(line, 0, *chars, num_chars, max_cols);
  (*chars) += num_chars;

  return max_cols;
}

static void resize_hadjustment(vt_edit_t *edit, u_int new_cols, u_int old_cols) {
  int cursor_row = edit->cursor.row;
  int cursor_col = edit->cursor.col;
  u_int row;
  vt_char_t *orig_buffer;
  u_int max_buf_len = 0;

  for (row = 0; row < edit->model.num_rows;) {
    vt_line_t *line = vt_model_get_line(&edit->model, row);
    vt_line_t *line_tmp;
    int expand_wrap_lines;
    u_int count;
    u_int buf_len = 0;
    u_int line_nchars;

    line_nchars = vt_line_get_num_filled_chars_except_sp(line);

    if (vt_line_is_continued_to_next(line) && old_cols < new_cols) {
      expand_wrap_lines = 1;
    } else if (vt_str_cols(line->chars, line_nchars) > new_cols) {
      expand_wrap_lines = 0;
    } else {
      row++;
      continue;
    }

    count = 0;
    line_tmp = line;
    while (1) {
      buf_len += line_tmp->num_filled_chars;

      if (!vt_line_is_continued_to_next(line_tmp)) {
        break;
      } else if (row + count + 1 >= edit->model.num_rows) {
        /* XXX The bottom line should not be wrapped, but it can happen. */
        vt_line_set_continued_to_next(line_tmp, 0);
        break;
      }

      line_tmp = vt_model_get_line(&edit->model, row + (++count));
    }

    if (buf_len > 0 &&
        (buf_len <= max_buf_len || (orig_buffer = vt_str_alloca((max_buf_len = buf_len))))) {
      u_int num_wrap_rows;
      u_int old_num_wrap_rows = 0;
      u_int num_filled_chars = 0;
      u_int num_filled_cols = 0;
      u_int line_ncols;
      int cursor_adjusted = 0;
      vt_char_t *buffer = orig_buffer;

      vt_str_init(buffer, buf_len);

      line_tmp = line;
      count = 0;
      while (1) {
        line_nchars = vt_line_get_num_filled_chars_except_sp(line_tmp);
        line_ncols = vt_str_cols(line_tmp->chars, line_nchars);

        vt_str_copy(buffer + num_filled_chars, line_tmp->chars, line_nchars);
        num_filled_chars += line_nchars;
        num_filled_cols += line_ncols;

        /*
         * cursor_adjusted flag is necessary because 'cursor_row = cursor_row + n - count'
         * below increments cursor_row and 'row + count == cursor_row' will get true again.
         */
        if (!cursor_adjusted && row + count == cursor_row) {
          int n;

          /*
           * XXX
           * This calculation is incorrect if a character at the end of line
           * is full width.
           */
          cursor_col = edit->cursor.col + count * old_cols;
          n = cursor_col / new_cols;
          cursor_row = cursor_row + n - count;
          cursor_col -= (n * new_cols);

          if (edit->cursor.char_index >= line_nchars) {
            /* Copy trailing spaces */
            vt_str_copy(buffer + num_filled_chars, line_tmp->chars + line_nchars,
                        edit->cursor.char_index + 1 - line_nchars);
            num_filled_chars += (edit->cursor.char_index + 1 - line_ncols);
            num_filled_cols += vt_str_cols(line_tmp->chars + line_nchars,
                                      edit->cursor.char_index + 1 - line_nchars);
          }

          cursor_adjusted = 1;
        }

        if (!vt_line_is_continued_to_next(line_tmp)) {
          break;
        }

        /*
         * If 'row + count == cursor_row && cursor_col >= line_ncols' above is true,
         * vt_line_is_continued_to_next(line_tmp) is always false.
         */
        vt_str_copy(buffer + num_filled_chars, line_tmp->chars + line_nchars,
                    line_tmp->num_filled_chars - line_nchars);
        num_filled_chars += (line_tmp->num_filled_chars - line_nchars);
        num_filled_cols += (vt_str_cols(line_tmp->chars, line_tmp->num_filled_chars) - line_ncols);

        line_tmp = vt_model_get_line(&edit->model, row + (++count));
        old_num_wrap_rows ++;
      }

      if (num_filled_cols == 0) {
        num_wrap_rows = 0;
      } else {
        num_wrap_rows = (num_filled_cols - 1) / new_cols;
      }

      if (expand_wrap_lines) {
        u_int next_row = row + vt_edit_replace(edit, row, buffer, num_filled_cols, new_cols);

        if (old_num_wrap_rows > num_wrap_rows) {
          for (count = row + old_num_wrap_rows + 1; count < edit->model.num_rows; count++) {
            vt_line_copy(vt_model_get_line(&edit->model,
                                           count - (old_num_wrap_rows - num_wrap_rows)),
                         vt_model_get_line(&edit->model, count));
          }

          for (count -= (old_num_wrap_rows - num_wrap_rows);
               count < edit->model.num_rows; count++) {
            vt_line_reset(vt_model_get_line(&edit->model, count));
          }

          if (cursor_row > row + old_num_wrap_rows) {
            cursor_row -= (old_num_wrap_rows - num_wrap_rows);
          }
        }

        row = next_row;
      } else {
        u_int empty_rows;

        /* old_num_wrap_rows is never greater than num_wrap_rows if expand_wrap_lines is false. */
        num_wrap_rows -= old_num_wrap_rows;

        for (empty_rows = 0; empty_rows < num_wrap_rows; empty_rows++) {
          int r = edit->model.num_rows - empty_rows - 1;

          if (r <= cursor_row) {
            break;
          }

          line = vt_model_get_line(&edit->model, r);
          if (vt_line_get_num_filled_chars_except_sp(line) > 0) {
            break;
          }
        }

        if (empty_rows > 0) {
          if (cursor_row > row + old_num_wrap_rows) {
            cursor_row += empty_rows;
          }

          num_wrap_rows -= empty_rows;

          for (count = edit->model.num_rows - 1 - empty_rows; count > row; count--) {
            vt_line_copy(vt_model_get_line(&edit->model, count + empty_rows),
                         vt_model_get_line(&edit->model, count));
          }
        }

        if (num_wrap_rows > 0) {
          count = 0;
          do {
            if (count >= row) {
              line = vt_model_get_line(&edit->model, row);
              /*
               * new_cols is always less than num_filled_cols here because this line is
               * scrolled out below but at least one line should remain in the screen.
               */
              num_filled_cols -= replace_chars_in_line(line, &buffer, new_cols);
              vt_line_set_continued_to_next(line, 1);
            } else {
              line = vt_model_get_line(&edit->model, count);
              if (cursor_row < row) {
                cursor_row--;
              }
            }

            if (edit->is_logging) {
              (*edit->scroll_listener->receive_scrolled_out_line)(edit->scroll_listener->self,
                                                                  line);
            }
          } while (++count < num_wrap_rows);

          if (num_wrap_rows <= row) {
            for (count = 0; count < row - num_wrap_rows; count++) {
              vt_line_copy(vt_model_get_line(&edit->model, count),
                           vt_model_get_line(&edit->model, num_wrap_rows + count));
            }
            row -= num_wrap_rows;
          } else {
            row = 0;
          }
        }

        row += vt_edit_replace(edit, row, buffer, num_filled_cols, new_cols);
      }

      vt_str_final(orig_buffer, buf_len);
    }
  }

  if (cursor_row < 0) {
    cursor_row = cursor_col = 0;
  } else if (cursor_row >= edit->model.num_rows) {
    vt_line_t *line;

    cursor_row = edit->model.num_rows - 1;
    line = vt_model_get_line(&edit->model, cursor_row);
    /* cursor_col points space character just after the end character except space. */
    cursor_col = vt_str_cols(line->chars, vt_line_get_num_filled_chars_except_sp(line));
  }

#if 0
  if (edit->cursor.col != cursor_col || edit->cursor.row != cursor_row)
#endif
  {
    vt_cursor_goto_by_col(&edit->cursor, cursor_col, cursor_row);
  }
}

/* --- global functions --- */

void vt_set_resize_mode(vt_resize_mode_t mode) {
  resize_mode = mode;
}

vt_resize_mode_t vt_get_resize_mode_by_name(const char *name) {
  vt_resize_mode_t mode;

  for (mode = 0; mode < RZ_MODE_MAX; mode++) {
    if (strcmp(resize_mode_name_table[mode], name) == 0) {
      return mode;
    }
  }

  /* default value */
  return RZ_WRAP;
}

int vt_edit_init(vt_edit_t *edit, vt_edit_scroll_event_listener_t *scroll_listener,
                 u_int num_cols, u_int num_rows, u_int tab_size, int is_logging,
                 int use_bce) {
  if (!vt_model_init(&edit->model, num_cols, num_rows)) {
    return 0;
  }

  vt_cursor_init(&edit->cursor, &edit->model);

  vt_line_assure_boundary(CURSOR_LINE(edit), 0);

  vt_char_init(&edit->bce_ch);
  vt_char_copy(&edit->bce_ch, vt_sp_ch());

  edit->use_bce = use_bce;

  edit->is_logging = is_logging;

  reset_wraparound_checker(edit);

  edit->vmargin_beg = 0;
  edit->vmargin_end = vt_model_end_row(&edit->model);
  edit->scroll_listener = scroll_listener;

  edit->use_margin = 0;
  edit->hmargin_beg = 0;
  edit->hmargin_end = num_cols - 1;

  if ((edit->tab_stops = malloc(TAB_STOPS_SIZE(edit))) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " malloc failed.\n");
#endif

    return 0;
  }

  vt_edit_set_tab_size(edit, tab_size);

  edit->is_relative_origin = 0;
  edit->is_auto_wrap = 1;

  return 1;
}

void vt_edit_final(vt_edit_t *edit) {
  vt_model_final(&edit->model);

  free(edit->tab_stops);

  vt_char_final(&edit->bce_ch);
}

int vt_edit_clone(vt_edit_t *dst_edit, vt_edit_t *src_edit) {
  u_int row;
  u_int num_rows;
  vt_line_t *src_line;
  vt_line_t *dst_line;

  memcpy(((char*)dst_edit) + sizeof(vt_model_t), ((char*)src_edit) + sizeof(vt_model_t),
         sizeof(vt_edit_t) - sizeof(vt_model_t));

  if (!(dst_edit->tab_stops = malloc(TAB_STOPS_SIZE(src_edit)))) {
    return 0;
  }
  memcpy(dst_edit->tab_stops, src_edit->tab_stops, TAB_STOPS_SIZE(src_edit));

  dst_edit->cursor.model = &dst_edit->model;

  num_rows = vt_edit_get_rows(src_edit);

  if (!vt_model_init(&dst_edit->model, vt_edit_get_cols(src_edit), num_rows)) {
    free(dst_edit->tab_stops);

    return 0;
  }

  for (row = 0; row < num_rows; row++) {
    dst_line = vt_edit_get_line(dst_edit, row);

    if ((src_line = vt_edit_get_line(src_edit, row)) == src_edit->wraparound_ready_line) {
      dst_edit->wraparound_ready_line = dst_line;
    }

    vt_line_copy(dst_line, src_line);
  }

  return 1;
}

int vt_edit_resize(vt_edit_t *edit, u_int new_cols, u_int new_rows) {
  u_int old_cols;
  u_int row;
  u_int old_filled_rows;
  u_int slide;
  int ret = 1;

#ifdef CURSOR_DEBUG
  vt_cursor_dump(&edit->cursor);
#endif

  old_cols = edit->model.num_cols;

  if (resize_mode == RZ_WRAP && old_cols > new_cols) {
    resize_hadjustment(edit, new_cols, old_cols);
    ret = 2;
  }

  if ((old_filled_rows = vt_model_get_num_filled_rows(&edit->model)) <= edit->cursor.row) {
    old_filled_rows = edit->cursor.row + 1;
  }

  if (old_filled_rows > new_rows) {
    slide = old_filled_rows - new_rows;
  } else {
    slide = 0;
  }

  if (edit->is_logging) {
    int scroll_all = 0;

    if (resize_mode == RZ_SCROLL) {
      for (row = 0; row < old_filled_rows; row++) {
        vt_line_t *line = vt_model_get_line(&edit->model, row);
        if (vt_str_cols(line->chars, vt_line_get_num_filled_chars_except_sp(line)) > new_cols) {
          scroll_all = 1;
          break;
        }
      }
    }

    if (scroll_all) {
      for (row = 0; row < old_filled_rows; row++) {
        (*edit->scroll_listener->receive_scrolled_out_line)(edit->scroll_listener->self,
                                                            vt_model_get_line(&edit->model, row));
      }
      vt_edit_goto_home(edit);
      vt_edit_clear_below(edit);
      ret = 3;
    } else {
      for (row = 0; row < slide; row++) {
        (*edit->scroll_listener->receive_scrolled_out_line)(edit->scroll_listener->self,
                                                            vt_model_get_line(&edit->model, row));
      }
    }
  }

  if (!vt_model_resize(&edit->model, new_cols, new_rows, slide)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " vt_model_resize() failed.\n");
#endif

    return 0;
  }

  if (slide > edit->cursor.row) {
    vt_cursor_goto_by_char(&edit->cursor, 0, 0);
  } else {
    if ((edit->cursor.row -= slide) >= new_rows) {
      edit->cursor.col = new_cols;
      edit->cursor.row = new_rows - 1;
    }

    if (edit->cursor.col >= new_cols) {
      /* col points space character just after the end character except space. */
      int col = vt_line_get_num_filled_chars_except_sp(
                  vt_model_get_line(&edit->model,edit->cursor.row));

      vt_cursor_goto_by_col(&edit->cursor, col, edit->cursor.row);
    }
  }

  if (resize_mode == RZ_WRAP && old_cols < new_cols) {
    if (edit->is_logging &&
        (*edit->scroll_listener->top_line_is_wrapped)(edit->scroll_listener->self)) {
      /*
       * XXX
       * +-------+    +---------+    +---------+
       * |abcdefg|    |abcdefg  |    |abcdefg  | backlog
       * +-------+ -> +---------+ -> |hij      |
       * |hij    |    |hij      |    +---------+
       * +-------+    +---------+    |         | main screen
       *                             +---------+
       */
      u_int count;
      int continued = 1;

      for (count = 0; continued; count++) {
        vt_line_t *line = vt_model_get_line(&edit->model, count);

        (*edit->scroll_listener->receive_scrolled_out_line)(edit->scroll_listener->self, line);
        continued = vt_line_is_continued_to_next(line);
        vt_line_reset(line);
      }

      vt_model_scroll_upward(&edit->model, count);
      if (edit->cursor.row < count) {
        vt_cursor_goto_by_char(&edit->cursor, 0, 0);
      } else {
        vt_cursor_goto_by_col(&edit->cursor, edit->cursor.col, edit->cursor.row - count);
      }
    }

    resize_hadjustment(edit, new_cols, old_cols);
    ret = 2;
  }

  reset_wraparound_checker(edit);

  edit->vmargin_beg = 0;
  edit->vmargin_end = vt_model_end_row(&edit->model);

  edit->use_margin = 0;
  edit->hmargin_beg = 0;
  edit->hmargin_end = new_cols - 1;

  free(edit->tab_stops);

  if ((edit->tab_stops = malloc(TAB_STOPS_SIZE(edit))) == NULL) {
    return 0;
  }

  vt_edit_set_tab_size(edit, edit->tab_size);

#ifdef CURSOR_DEBUG
  vt_cursor_dump(&edit->cursor);
#endif

  return ret;
}

int vt_edit_insert_chars(vt_edit_t *edit, vt_char_t *ins_chars, u_int num_ins_chars) {
  /*
   * edit->wraparound_ready_line is ignored.
   * esctest: SMTests.test_SM_IRM_DoesNotWrapUnlessCursorAtMargin fails by this.
   *
   * XXX
   * xterm-332, TeraTerm-4.95: Wraparound works if IRM is set.
   * rlogin-2.23.1: Wraparound is disabled if IRM is set.
   */
  reset_wraparound_checker(edit);

#ifdef COMPAT_XTERM
  if (!CURSOR_IS_INSIDE_HMARGIN(edit)) {
    return vt_edit_overwrite_chars(edit, ins_chars, num_ins_chars);
  } else
#endif
  {
    return insert_chars(edit, ins_chars, num_ins_chars, 1);
  }
}

int vt_edit_insert_blank_chars(vt_edit_t *edit, u_int num_blank_chars) {
  int count;
  vt_char_t *blank_chars;
  vt_char_t *sp_ch;

  if (!CURSOR_IS_INSIDE_HMARGIN(edit)) {
    return 0;
  }

  reset_wraparound_checker(edit);

  if ((blank_chars = vt_str_alloca(num_blank_chars)) == NULL) {
    return 0;
  }
  vt_str_init(blank_chars, num_blank_chars);

  if (edit->use_bce) {
    /*
     * If bce_ch is not used here, vttest 11.4.5 "If your terminal
     * has the ANSI 'Insert Character' function..." will fail.
     */
    sp_ch = &edit->bce_ch;
  } else {
    sp_ch = vt_sp_ch();
  }

  for (count = 0; count < num_blank_chars; count++) {
    vt_char_copy(&blank_chars[count], sp_ch);
  }

  vt_str_final(blank_chars, num_blank_chars);

  /* cursor will not moved. */
  return insert_chars(edit, blank_chars, num_blank_chars, 0);
}

int vt_edit_overwrite_chars(vt_edit_t *edit, vt_char_t *ow_chars, u_int num_ow_chars) {
  int count;
  vt_char_t *buffer;
  u_int buf_len;
  u_int num_cols;
  u_int filled_len;
  vt_line_t *line;
  int beg;
  u_int cols;
  int new_char_index;

#ifdef CURSOR_DEBUG
  vt_cursor_dump(&edit->cursor);
#endif

  buf_len = num_ow_chars + edit->model.num_cols;

  if (edit->cursor.col > edit->hmargin_end) {
    num_cols = edit->model.num_cols;
  } else {
    num_cols = edit->hmargin_end + 1;
  }

  if ((buffer = vt_str_alloca(buf_len)) == NULL) {
    return 0;
  }
  vt_str_init(buffer, buf_len);

  line = CURSOR_LINE(edit);
  filled_len = 0;

  /* before cursor(excluding cursor) */

  if (edit->cursor.col_in_char) {
    int count;

    /*
     * padding spaces before cursor.
     */
    for (count = 0; count < edit->cursor.col_in_char; count++) {
      vt_char_copy(&buffer[filled_len++], vt_sp_ch());
    }
  }

  /* appending overwriting chars */
  vt_str_copy(&buffer[filled_len], ow_chars, num_ow_chars);
  filled_len += num_ow_chars;

  /*
   * overwriting
   */

  beg = 0;
  count = 0;
  cols = 0;

  while (1) {
    u_int _cols;

    _cols = vt_char_cols(&buffer[count]);

    if (edit->cursor.col + cols + _cols > num_cols ||
        (edit->wraparound_ready_line && edit->cursor.col + cols + _cols == num_cols)) {
      vt_line_overwrite(line, edit->cursor.char_index, &buffer[beg], count - beg, cols);

      if (!edit->is_auto_wrap) {
        /*
         * ---+      ---+
         *    |         |
         * abcde  => abe|
         * (esctest: DECSET_DECAWM_OffRespectsLeftRightMargin)
         */
        if (count > 0) {
          vt_char_copy(&buffer[count - 1], &buffer[filled_len - 1]);
        }

        break;
      }

      vt_line_set_continued_to_next(line, 1);

      if (edit->cursor.row + 1 > edit->vmargin_end) {
        if (MARGIN_IS_ENABLED(edit) ? !scroll_upward_region(edit, 1, 0, 0)
                                    : !vt_edsl_scroll_upward(edit, 1)) {
          return 0;
        }

        /*
         * If edit->cursor.row == edit->vmargin_end in this situation,
         * vmargin_beg == vmargin_end.
         */
        if (edit->cursor.row + 1 <= edit->vmargin_end) {
          edit->cursor.row++;
        }
      } else {
        edit->cursor.row++;
      }

      if (edit->hmargin_beg > 0) {
        vt_cursor_goto_by_col(&edit->cursor, edit->hmargin_beg, edit->cursor.row);
      } else {
        edit->cursor.char_index = edit->cursor.col = 0;
      }

      /* Reset edit->wraparound_ready_line because it is not cursor line now. */
      reset_wraparound_checker(edit);

      beg = count;
      cols = _cols;
      line = CURSOR_LINE(edit);
    } else {
      cols += _cols;
    }

    if (++count >= filled_len) {
      break;
    }
  }

  new_char_index = edit->cursor.char_index + count - beg;

  if (edit->cursor.col + cols >= num_cols && edit->wraparound_ready_line != line) {
    /*
     * Don't use vt_line_end_char_index() instead of
     * new_char_index --, because num_cols is not
     * vt_model::num_cols but is vt_edit_t::hmargin_end + 1.
     */
    new_char_index--;
    edit->wraparound_ready_line = line;
  } else {
    reset_wraparound_checker(edit);
  }

  vt_line_overwrite(line, edit->cursor.char_index, &buffer[beg], count - beg, cols);
  vt_line_set_continued_to_next(line, 0);

  vt_cursor_moveh_by_char(&edit->cursor, new_char_index);

  vt_str_final(buffer, buf_len);

#ifdef CURSOR_DEBUG
  vt_cursor_dump(&edit->cursor);
#endif

  return 1;
}

u_int vt_edit_replace(vt_edit_t *edit, int beg_row /* starting row to be replaced */,
                      vt_char_t *chars, u_int cols /* > 0 */, u_int max_cols_per_line /* > 0 */) {
  int row = beg_row;
  vt_line_t *line;

  while (1) {
    if ((line = vt_model_get_line(&edit->model, row)) == NULL) {
      if (edit->is_logging) {
        (*edit->scroll_listener->receive_scrolled_out_line)(edit->scroll_listener->self,
                                                            vt_model_get_line(&edit->model, 0));
      }
      vt_model_scroll_upward(&edit->model, 1);

      if ((line = vt_model_get_line(&edit->model, row - 1)) == NULL) {
        break;
      }
    } else {
      row++;
    }

    cols -= replace_chars_in_line(line, &chars,
                                  cols < max_cols_per_line ? cols : max_cols_per_line);
    if (cols == 0) {
      break;
    }

    vt_line_set_continued_to_next(line, 1);
  }

  return row - beg_row;
}

/*
 * deleting cols within a line.
 */
int vt_edit_delete_cols(vt_edit_t *edit, u_int del_cols) {
  int char_index;
  vt_char_t *buffer;
  u_int buf_len;
  u_int filled_len;
  vt_line_t *cursor_line;
  u_int num_filled_cols;

#ifdef CURSOR_DEBUG
  vt_cursor_dump(&edit->cursor);
#endif

  reset_wraparound_checker(edit);

  cursor_line = CURSOR_LINE(edit);

  num_filled_cols = vt_line_get_num_filled_cols(cursor_line);

  if (!MARGIN_IS_ENABLED(edit) && edit->cursor.col + del_cols >= num_filled_cols) {
    /* no need to overwrite */
    vt_edit_clear_line_to_right(edit); /* Considering BCE */

    return 1;
  }

  /*
   * collecting chars after cursor line.
   */

  buf_len = cursor_line->num_chars - edit->cursor.col;

  if ((buffer = vt_str_alloca(buf_len)) == NULL) {
    return 0;
  }
  vt_str_init(buffer, buf_len);

  filled_len = 0;

  /* before cursor(including cursor) */

  if (edit->cursor.col_in_char) {
    int cols_after;
    int count;

#ifdef DEBUG
    if (vt_char_cols(CURSOR_CHAR(edit)) <= edit->cursor.col_in_char) {
      bl_warn_printf(BL_DEBUG_TAG " illegal col_in_char.\n");
    }
#endif

    cols_after = vt_char_cols(CURSOR_CHAR(edit)) - edit->cursor.col_in_char;

    /*
     * padding spaces before cursor.
     */
    for (count = 0; count < edit->cursor.col_in_char; count++) {
      vt_char_copy(&buffer[filled_len++], vt_sp_ch());
    }

    if (del_cols >= cols_after) {
      del_cols -= cols_after;
    } else {
      del_cols = 0;

      /*
       * padding spaces after cursor.
       */
      for (count = 0; count < cols_after - del_cols; count++) {
        vt_char_copy(&buffer[filled_len++], vt_sp_ch());
      }
    }

    char_index = edit->cursor.char_index + 1;
  } else {
    char_index = edit->cursor.char_index;
  }

  /* after cursor(excluding cursor) + del_cols */

  if (del_cols) {
    u_int cols;

    cols = vt_char_cols(vt_char_at(cursor_line, char_index++));

    if (MARGIN_IS_ENABLED(edit)) {
      if (!CURSOR_IS_INSIDE_HMARGIN(edit)) {
        return 0;
      }

      if (num_filled_cols > edit->hmargin_end + 1) {
        u_int count;
        u_int copy_len;

        while (cols < del_cols && edit->cursor.col + cols <= edit->hmargin_end) {
          cols += vt_char_cols(vt_char_at(cursor_line, char_index++));
        }
        del_cols = cols;

        while (edit->cursor.col + (cols++) <= edit->hmargin_end) {
          vt_char_copy(buffer + filled_len++, vt_char_at(cursor_line, char_index++));
        }

        for (count = 0; count < del_cols; count++) {
          vt_char_copy(buffer + (filled_len++), edit->use_bce ? &edit->bce_ch : vt_sp_ch());
        }

        copy_len = 0;
        while (char_index + copy_len < cursor_line->num_filled_chars) {
          vt_char_cols(vt_char_at(cursor_line, char_index + (copy_len++)));
        }

        vt_str_copy(buffer + filled_len, vt_char_at(cursor_line, char_index), copy_len);
        filled_len += copy_len;
      }
    } else {
      while (cols < del_cols && char_index < cursor_line->num_filled_chars) {
        cols += vt_char_cols(vt_char_at(cursor_line, char_index++));
      }

      vt_str_copy(buffer + filled_len, vt_char_at(cursor_line, char_index),
                  cursor_line->num_filled_chars - char_index);
      filled_len += (cursor_line->num_filled_chars - char_index);
    }
  }

  if (filled_len > 0) {
    /*
     * overwriting.
     */

    vt_edit_clear_line_to_right(edit); /* Considering BCE */
    vt_line_overwrite(cursor_line, edit->cursor.char_index, buffer, filled_len,
                      vt_str_cols(buffer, filled_len));
  } else {
    vt_line_reset(cursor_line);
  }

  vt_str_final(buffer, buf_len);

  if (edit->cursor.col_in_char) {
    vt_cursor_moveh_by_char(&edit->cursor, edit->cursor.char_index + edit->cursor.col_in_char);
  }

#ifdef CURSOR_DEBUG
  vt_cursor_dump(&edit->cursor);
#endif

  return 1;
}

int vt_edit_clear_cols(vt_edit_t *edit, u_int cols) {
  vt_line_t *cursor_line;

  reset_wraparound_checker(edit);

  if (edit->cursor.col + cols >= edit->model.num_cols) {
    return vt_edit_clear_line_to_right(edit);
  }

  cursor_line = CURSOR_LINE(edit);

  if (edit->cursor.col_in_char) {
    vt_line_fill(cursor_line, edit->use_bce ? &edit->bce_ch : vt_sp_ch(),
                 edit->cursor.char_index, edit->cursor.col_in_char);

    vt_cursor_char_is_cleared(&edit->cursor);
  }

  vt_line_fill(cursor_line, edit->use_bce ? &edit->bce_ch : vt_sp_ch(), edit->cursor.char_index,
               cols);

  return 1;
}

int vt_edit_insert_new_line(vt_edit_t *edit) {
  reset_wraparound_checker(edit);

  if (MARGIN_IS_ENABLED(edit)) {
    return scroll_downward_region(edit, 1, 1, 0);
  } else {
    return vt_edsl_insert_new_line(edit);
  }
}

int vt_edit_delete_line(vt_edit_t *edit) {
  reset_wraparound_checker(edit);

  if (MARGIN_IS_ENABLED(edit)) {
    return scroll_upward_region(edit, 1, 1, 0);
  } else {
    return vt_edsl_delete_line(edit);
  }
}

int vt_edit_clear_line_to_right(vt_edit_t *edit) {
  vt_line_t *cursor_line;

  reset_wraparound_checker(edit);

  cursor_line = CURSOR_LINE(edit);

  if (edit->cursor.col_in_char) {
    vt_line_fill(cursor_line, edit->use_bce ? &edit->bce_ch : vt_sp_ch(), edit->cursor.char_index,
                 edit->cursor.col_in_char);
    vt_cursor_char_is_cleared(&edit->cursor);
  }

  if (edit->use_bce) {
    vt_line_clear_with(cursor_line, edit->cursor.char_index, &edit->bce_ch);
  } else {
    vt_line_clear(CURSOR_LINE(edit), edit->cursor.char_index);
  }

  return 1;
}

int vt_edit_clear_line_to_left(vt_edit_t *edit) {
  vt_line_t *cursor_line;

  reset_wraparound_checker(edit);

  cursor_line = CURSOR_LINE(edit);

  vt_line_fill(cursor_line, edit->use_bce ? &edit->bce_ch : vt_sp_ch(), 0, edit->cursor.col + 1);

  vt_cursor_left_chars_in_line_are_cleared(&edit->cursor);

  return 1;
}

int vt_edit_clear_below(vt_edit_t *edit) {
  reset_wraparound_checker(edit);

  if (!vt_edit_clear_line_to_right(edit)) {
    return 0;
  }

  if (edit->use_bce) {
    int row;

    for (row = edit->cursor.row + 1; row < edit->model.num_rows; row++) {
      vt_line_clear_with(vt_model_get_line(&edit->model, row), 0, &edit->bce_ch);
    }

    return 1;
  } else {
    return vt_edit_clear_lines(edit, edit->cursor.row + 1,
                               edit->model.num_rows - edit->cursor.row - 1);
  }
}

int vt_edit_clear_above(vt_edit_t *edit) {
  reset_wraparound_checker(edit);

  if (!vt_edit_clear_line_to_left(edit)) {
    return 0;
  }

  if (edit->use_bce) {
    int row;

    for (row = 0; row < edit->cursor.row; row++) {
      vt_line_clear_with(vt_model_get_line(&edit->model, row), 0, &edit->bce_ch);
    }

    return 1;
  } else {
    return vt_edit_clear_lines(edit, 0, edit->cursor.row);
  }
}

vt_protect_store_t *vt_edit_save_protected_chars(vt_edit_t *edit,
                                                 int beg_row, /* -1: cursor row (unless relative) */
                                                 int end_row, /* -1: cursor row (unless relative) */
                                                 int relative) {
  vt_protect_store_t *save = NULL;
  vt_char_t *dst;
  vt_char_t *src;
  int row;
  vt_line_t *line;

  if (relative) {
    if (edit->is_relative_origin) {
      if ((beg_row += edit->vmargin_beg) > edit->vmargin_end) {
        return NULL;
      }
      if ((end_row += edit->vmargin_beg) > edit->vmargin_end) {
        end_row = edit->vmargin_end;
      }
    }
  } else {
    if (beg_row == -1) {
      beg_row = vt_cursor_row(edit);
    }
    if (end_row == -1) {
      end_row = vt_cursor_row(edit);
    }
  }

  for (row = beg_row; row <= end_row; row++) {
    if ((line = vt_edit_get_line(edit, row))) {
      u_int num = vt_line_get_num_filled_chars_except_sp(line);
      u_int count;

      src = line->chars;
      for (count = 0; count < num; count++, src++) {
        if (vt_char_is_protected(src)) {
          if (!save) {
            if (!(save = malloc(sizeof(vt_protect_store_t) +
                                sizeof(vt_char_t) * (vt_edit_get_cols(edit) + 1) *
                                                    (end_row - row + 1)))) {
              return NULL;
            }

            dst = save->chars = (vt_char_t*)(save + 1);
            vt_str_init(dst, (vt_edit_get_cols(edit) + 1) * (end_row - row + 1));
            dst += count;
            save->beg_row = row;
            save->end_row = end_row;
          }

          vt_char_copy(dst++, src);
        } else if (save) {
          dst += vt_char_cols(src);
        }
      }
    }

    if (save) {
      vt_char_copy(dst++, vt_nl_ch());
    }
  }

  return save;
}

void vt_edit_restore_protected_chars(vt_edit_t *edit, vt_protect_store_t *save) {
  int row;
  int col;
  u_int cols;
  vt_line_t *line;
  vt_char_t *src_p;

  if (save == NULL) {
    return;
  }

  src_p = save->chars;

  for (row = save->beg_row; row <= save->end_row; row++) {
    if ((line = vt_edit_get_line(edit, row))) {
      for (col = 0; !vt_char_equal(src_p, vt_nl_ch()); src_p++) {
        if (vt_char_is_protected(src_p)) {
          cols = vt_char_cols(src_p);
          vt_line_overwrite(line,
                            /* cols_rest must be always 0, so pass NULL. */
                            vt_convert_col_to_char_index(line, NULL, col, BREAK_BOUNDARY),
                            src_p, 1, cols);
        } else {
          cols = 1;
        }
        col += cols;
      }
      src_p++;
    }
  }

  vt_str_final(save->chars, (vt_edit_get_cols(edit) + 1) * (save->end_row - save->beg_row + 1));
  free(save);
}

int vt_edit_set_vmargin(vt_edit_t *edit, int beg, int end) {
  /*
   * If beg and end is -1, use default(full size of window).
   * (see vt_parser.c)
   *
   * For compatibility with xterm:
   * 1. if beg and end are smaller than 0, ignore the sequence.
   * 2. if end is not larger than beg, ignore the sequence.
   * 3. if beg and end are out of window, ignore the sequence.
   */

  if (beg < 0) {
    if (beg == -1) {
      beg = 0;
    } else {
      return 0;
    }
  }

  if (end < 0) {
    if (end == -1) {
      end = vt_model_end_row(&edit->model);
    } else {
      return 0;
    }
  }

  if (beg >= end) {
    return 0;
  }

  if (beg >= edit->model.num_rows) {
    return 0;
  }

  if (end >= edit->model.num_rows) {
    end = vt_model_end_row(&edit->model);
  }

  edit->vmargin_beg = beg;
  edit->vmargin_end = end;

  return 1;
}

void vt_edit_scroll_leftward(vt_edit_t *edit, u_int size) {
  copy_area(edit, edit->hmargin_beg + size, edit->vmargin_beg,
            edit->hmargin_end - edit->hmargin_beg + 1 - size,
            edit->vmargin_end - edit->vmargin_beg + 1,
            edit, edit->hmargin_beg, edit->vmargin_beg);
  erase_area(edit, edit->hmargin_end + 1 - size, edit->vmargin_beg,
             size, edit->vmargin_end - edit->vmargin_beg + 1);
}

void vt_edit_scroll_rightward(vt_edit_t *edit, u_int size) {
  copy_area(edit, edit->hmargin_beg, edit->vmargin_beg,
            edit->hmargin_end - edit->hmargin_beg + 1 - size,
            edit->vmargin_end - edit->vmargin_beg + 1,
            edit, edit->hmargin_beg + size, edit->vmargin_beg);
  erase_area(edit, edit->hmargin_beg, edit->vmargin_beg,
             size, edit->vmargin_end - edit->vmargin_beg + 1);
}

int vt_edit_scroll_leftward_from_cursor(vt_edit_t *edit, u_int width) {
  int src;
  u_int height;

  if (!CURSOR_IS_INSIDE_HMARGIN(edit) || !CURSOR_IS_INSIDE_VMARGIN(edit)) {
    return 0;
  }

  height = edit->vmargin_end - edit->vmargin_beg + 1;

  if ((src = edit->cursor.col + width) <= edit->hmargin_end) {
    copy_area(edit, src, edit->vmargin_beg, edit->hmargin_end - src + 1, height,
              edit, edit->cursor.col, edit->vmargin_beg);
  } else {
    width = edit->hmargin_end - edit->cursor.col + 1;
  }

  erase_area(edit, edit->hmargin_end - width + 1, edit->vmargin_beg, width, height);

  return 1;
}

int vt_edit_scroll_rightward_from_cursor(vt_edit_t *edit, u_int width) {
  int dst;
  u_int height;

  if (!CURSOR_IS_INSIDE_HMARGIN(edit) || !CURSOR_IS_INSIDE_VMARGIN(edit)) {
    return 0;
  }

  height = edit->vmargin_end - edit->vmargin_beg + 1;

  if ((dst = edit->cursor.col + width) <= edit->hmargin_end) {
    copy_area(edit, edit->cursor.col, edit->vmargin_beg, edit->hmargin_end - dst + 1, height,
              edit, dst, edit->vmargin_beg);
  } else {
    width = edit->hmargin_end - edit->cursor.col + 1;
  }

  erase_area(edit, edit->cursor.col, edit->vmargin_beg, width, height);

  return 1;
}

int vt_edit_scroll_upward(vt_edit_t *edit, u_int size) {
  int cursor_row;
  int cursor_col;

  cursor_row = edit->cursor.row;
  cursor_col = edit->cursor.col;

  if (MARGIN_IS_ENABLED(edit)) {
    scroll_upward_region(edit, size, 0, 1);
  } else {
    vt_edsl_scroll_upward(edit, size);
  }

  vt_cursor_goto_by_col(&edit->cursor, cursor_col, cursor_row);

  return 1;
}

int vt_edit_scroll_downward(vt_edit_t *edit, u_int size) {
  int cursor_row;
  int cursor_col;

  cursor_row = edit->cursor.row;
  cursor_col = edit->cursor.col;

  if (MARGIN_IS_ENABLED(edit)) {
    scroll_downward_region(edit, size, 0, 1);
  } else {
    vt_edsl_scroll_downward(edit, size);
  }

  vt_cursor_goto_by_col(&edit->cursor, cursor_col, cursor_row);

  return 1;
}

void vt_edit_set_use_hmargin(vt_edit_t *edit, int use) {
  if (!use) {
    edit->use_margin = 0;
    edit->hmargin_beg = 0;
    edit->hmargin_end = edit->model.num_cols - 1;
  } else {
    edit->use_margin = 1;
  }
}

int vt_edit_set_hmargin(vt_edit_t *edit, int beg, int end) {
  if (!edit->use_margin) {
    /*
     * The terminal only recognizes DECSLRM if vertical split screen mode (DECLRMM) is set.
     * (see https://www.vt100.net/docs/vt510-rm/DECSLRM.html)
     */
    return 0;
  }

  /*
   * If beg and end is -1, use default(full size of window).
   * (see vt_parser.c)
   *
   * For compatibility with xterm:
   * 1. if beg and end are smaller than 0, ignore the sequence.
   * 2. if end is not larger than beg, ignore the sequence.
   * 3. if beg and end are out of window, ignore the sequence.
   */

  if (beg < 0) {
    if (beg == -1) {
      beg = 0;
    } else {
      return 0;
    }
  }

  if (end < 0) {
    if (end == -1) {
      end = edit->model.num_cols - 1;
    } else {
      return 0;
    }
  }

  if (beg >= end) {
    return 0;
  }

  if (beg >= edit->model.num_cols) {
    return 0;
  }

  if (end >= edit->model.num_cols) {
    end = edit->model.num_cols - 1;
  }

  edit->hmargin_beg = beg;
  edit->hmargin_end = end;

  return 1;
}

int vt_edit_forward_tabs(vt_edit_t *edit, u_int num) { return horizontal_tabs(edit, num, 1); }

int vt_edit_backward_tabs(vt_edit_t *edit, u_int num) {
#if 0
  /* compat with xterm 332 CBT behavior (esctest: CHATests.test_CHA_IgnoresScrollRegion) */
  int orig_hmargin_beg = edit->hmargin_beg;
  int ret;

  edit->hmargin_beg = 0;
  ret = horizontal_tabs(edit, num, 0);
  edit->hmargin_beg = orig_hmargin_beg;

  return ret;
#else
  /* compat with RLogin 2.23.1 / Teraterm 4.95 */
  return horizontal_tabs(edit, num, 0);
#endif
}

void vt_edit_set_tab_size(vt_edit_t *edit, u_int tab_size) {
  int col;
  u_int8_t *tab_stops;

  if (tab_size == 0) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " tab size 0 is not acceptable.\n");
#endif

    return;
  }

  vt_edit_clear_all_tab_stops(edit);

  col = 0;
  tab_stops = edit->tab_stops;

  while (1) {
    if (col % tab_size == 0) {
      (*tab_stops) |= (1 << (col % 8));
    }

    col++;

    if (col >= edit->model.num_cols) {
      tab_stops++;

      break;
    } else if (col % 8 == 7) {
      tab_stops++;
    }
  }

#ifdef __DEBUG
  {
    int i;

    bl_debug_printf(BL_DEBUG_TAG " tab stops =>\n");

    for (i = 0; i < edit->model.num_cols; i++) {
      if (vt_edit_is_tab_stop(edit, i)) {
        bl_msg_printf("*");
      } else {
        bl_msg_printf(" ");
      }
    }
    bl_msg_printf("\n");
  }
#endif

  edit->tab_size = tab_size;
}

void vt_edit_set_tab_stop(vt_edit_t *edit) {
  edit->tab_stops[edit->cursor.col / 8] |= (1 << (edit->cursor.col % 8));
}

void vt_edit_clear_tab_stop(vt_edit_t *edit) {
  edit->tab_stops[edit->cursor.col / 8] &= ~(1 << (edit->cursor.col % 8));
}

void vt_edit_clear_all_tab_stops(vt_edit_t *edit) {
  memset(edit->tab_stops, 0, TAB_STOPS_SIZE(edit));
}

void vt_edit_set_modified_all(vt_edit_t *edit) {
  u_int count;

  for (count = 0; count < edit->model.num_rows; count++) {
    vt_line_set_modified_all(vt_model_get_line(&edit->model, count));
  }
}

void vt_edit_goto_beg_of_line(vt_edit_t *edit) {
  reset_wraparound_checker(edit);

  if (edit->hmargin_beg > 0 && edit->cursor.col >= edit->hmargin_beg) {
    vt_cursor_goto_by_col(&edit->cursor, edit->hmargin_beg, edit->cursor.row);
  } else {
    vt_cursor_goto_beg_of_line(&edit->cursor);
  }
}

/*
 * Note that this function ignores edit->is_relative_origin.
 */
void vt_edit_goto_home(vt_edit_t *edit) {
  reset_wraparound_checker(edit);

  vt_cursor_goto_home(&edit->cursor);
}

int vt_edit_go_forward(vt_edit_t *edit, int flag /* WARPAROUND | SCROLL */
                       ) {
  u_int num_cols;

#ifdef CURSOR_DEBUG
  vt_cursor_dump(&edit->cursor);
#endif

  if (CURSOR_IS_INSIDE_HMARGIN(edit)) {
    num_cols = edit->hmargin_end + 1;
  } else {
    num_cols = edit->model.num_cols;
  }

  reset_wraparound_checker(edit);

  if (edit->cursor.col + 1 >= num_cols) {
    if (!(flag & WRAPAROUND)) {
      return 0;
    }

    if (vt_is_scroll_lowerlimit(edit, edit->cursor.row)) {
      if (!(flag & SCROLL) || (MARGIN_IS_ENABLED(edit) ? !scroll_upward_region(edit, 1, 0, 0)
                                                       : !vt_edsl_scroll_upward(edit, 1))) {
        return 0;
      }
    }

    vt_cursor_cr_lf(&edit->cursor);
  } else if (!vt_cursor_go_forward(&edit->cursor)) {
    vt_line_break_boundary(CURSOR_LINE(edit), 1);
    vt_cursor_go_forward(&edit->cursor);
  }

#ifdef CURSOR_DEBUG
  vt_cursor_dump(&edit->cursor);
#endif

  return 1;
}

int vt_edit_go_back(vt_edit_t *edit, int flag /* WRAPAROUND | SCROLL */
                    ) {
#ifdef CURSOR_DEBUG
  vt_cursor_dump(&edit->cursor);
#endif

  reset_wraparound_checker(edit);

  /*
   * full width char check.
   */

  if (edit->cursor.col_in_char) {
#ifdef __DEBUG
    bl_debug_printf(BL_DEBUG_TAG " cursor is at 2nd byte of multi byte char.\n");
#endif

    edit->cursor.col--;
    edit->cursor.col_in_char--;

    return 1;
  }

  /*
   * moving backward.
   */

  if (edit->cursor.char_index == 0 || edit->cursor.char_index == edit->hmargin_beg) {
    if (!(flag & WRAPAROUND)) {
      return 0;
    }

    if (vt_is_scroll_upperlimit(edit, edit->cursor.row)) {
      if (!(flag & SCROLL) || (MARGIN_IS_ENABLED(edit) ? !scroll_downward_region(edit, 1, 0, 0)
                                                       : !vt_edsl_scroll_downward(edit, 1))) {
        return 0;
      }
    }

    if (edit->cursor.row == 0) {
      return 0;
    }

    edit->cursor.row--;
    edit->cursor.char_index = vt_line_end_char_index(CURSOR_LINE(edit));
  } else {
    edit->cursor.char_index--;
  }

  edit->cursor.col_in_char = vt_char_cols(CURSOR_CHAR(edit)) - 1;
  edit->cursor.col = vt_convert_char_index_to_col(CURSOR_LINE(edit), edit->cursor.char_index, 0) +
                     edit->cursor.col_in_char;

#ifdef CURSOR_DEBUG
  vt_cursor_dump(&edit->cursor);
#endif

  return 1;
}

int vt_edit_go_upward(vt_edit_t *edit, int flag /* SCROLL */
                      ) {
#ifdef CURSOR_DEBUG
  vt_cursor_dump(&edit->cursor);
#endif

  reset_wraparound_checker(edit);

  if (vt_is_scroll_upperlimit(edit, edit->cursor.row)) {
    if (!(flag & SCROLL) || (MARGIN_IS_ENABLED(edit) ? !scroll_downward_region(edit, 1, 0, 0)
                                                     : !vt_edit_scroll_downward(edit, 1))) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " cursor cannot go upward(reaches scroll lower limit).\n");
#endif

      return 0;
    }
  } else {
    if (!vt_cursor_goto_by_col(&edit->cursor, edit->cursor.col, edit->cursor.row - 1)) {
      return 0;
    }
  }

#ifdef CURSOR_DEBUG
  vt_cursor_dump(&edit->cursor);
#endif

  return 1;
}

int vt_edit_go_downward(vt_edit_t *edit, int flag /* SCROLL */
                        ) {
#ifdef CURSOR_DEBUG
  vt_cursor_dump(&edit->cursor);
#endif

  reset_wraparound_checker(edit);

  if (vt_is_scroll_lowerlimit(edit, edit->cursor.row)) {
    if (!(flag & SCROLL) || (MARGIN_IS_ENABLED(edit) ? !scroll_upward_region(edit, 1, 0, 0)
                                                     : !vt_edit_scroll_upward(edit, 1))) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " cursor cannot go downward(reaches scroll lower limit).\n");
#endif

      return 0;
    }
  } else {
    if (!vt_cursor_goto_by_col(&edit->cursor, edit->cursor.col, edit->cursor.row + 1)) {
      return 0;
    }
  }

#ifdef CURSOR_DEBUG
  vt_cursor_dump(&edit->cursor);
#endif

  return 1;
}

int vt_edit_goto(vt_edit_t *edit, int col, int row) {
  reset_wraparound_checker(edit);

  if (edit->is_relative_origin) {
    if ((row += edit->vmargin_beg) > edit->vmargin_end) {
      row = edit->vmargin_end;
    }

    if ((col += edit->hmargin_beg) > edit->hmargin_end) {
      col = edit->hmargin_end;
    }
  }

  return vt_cursor_goto_by_col(&edit->cursor, col, row);
}

void vt_edit_set_last_column_flag(vt_edit_t *edit, int flag) {
  if (flag) {
    edit->wraparound_ready_line = CURSOR_LINE(edit);
  } else {
    reset_wraparound_checker(edit);
  }
}

int vt_edit_restore_cursor(vt_edit_t *edit) {
  if (vt_cursor_restore(&edit->cursor)) {
    reset_wraparound_checker(edit);

    return 1;
  } else {
    return 0;
  }
}

void vt_edit_fill_area(vt_edit_t *edit, int code /* Unicode */, int is_protected,
                       int col, int row, u_int num_cols, u_int num_rows) {
  int char_index;
  u_int cols_rest;
  vt_line_t *line;
  vt_char_t ch;

  if (!apply_relative_origin(edit, &col, &row, &num_cols, &num_rows)) {
    return;
  }

  vt_char_init(&ch);
  vt_char_set(&ch, code,
              code <= 0x7f ? US_ASCII : ISO10646_UCS4_1, /* XXX biwidth is not supported. */
              0, 0, 0,
              /*
               * xterm-332 and Tera Term 4.95 don't use BCE for DECALN(ESC#8), but use for DECFRA.
               * rlogin-2.23.1 use BCE for both of them.
               *
               * (Test)
               * echo -e "\x1b[0;33;44m\x1b[60;1;1;10;10\$x"
               * echo -e "\x1b[0;33;44m\x1b#8"
               */
              edit->use_bce ? vt_char_fg_color(&edit->bce_ch) : VT_FG_COLOR,
              edit->use_bce ? vt_char_bg_color(&edit->bce_ch) : VT_BG_COLOR,
              0, 0, 0, 0, is_protected);

  for (; num_rows > 0; num_rows--) {
    line = vt_model_get_line(&edit->model, row++);

    char_index = vt_convert_col_to_char_index(line, &cols_rest, col, BREAK_BOUNDARY);
    if (cols_rest > 0) {
      vt_line_fill(line, edit->use_bce ? &edit->bce_ch : vt_sp_ch(), char_index, cols_rest);
      char_index += cols_rest;
    }

    vt_line_fill(line, &ch, char_index, num_cols);
  }

  vt_char_final(&ch);
}

void vt_edit_copy_area(vt_edit_t *src_edit, int src_col, int src_row, u_int num_copy_cols,
                      u_int num_copy_rows, vt_edit_t *dst_edit, int dst_col, int dst_row) {
  if (src_edit->is_relative_origin) {
    if ((src_row += src_edit->vmargin_beg) > src_edit->vmargin_end ||
        (dst_row += dst_edit->vmargin_beg) > dst_edit->vmargin_end ||
        (src_col += src_edit->hmargin_beg) > src_edit->hmargin_end ||
        (dst_col += dst_edit->hmargin_beg) > dst_edit->hmargin_end) {
      return;
    }

    if (src_row + num_copy_rows > src_edit->vmargin_end + 1) {
      num_copy_rows = src_edit->vmargin_end + 1 - src_row;
    }

    if (dst_row + num_copy_rows > dst_edit->vmargin_end + 1) {
      num_copy_rows = dst_edit->vmargin_end + 1 - dst_row;
    }

    if (src_col + num_copy_cols > src_edit->hmargin_end + 1) {
      num_copy_cols = src_edit->hmargin_end + 1 - src_col;
    }

    if (dst_col + num_copy_cols > dst_edit->hmargin_end + 1) {
      num_copy_cols = dst_edit->hmargin_end + 1 - dst_col;
    }
  } else {
    if (src_row >= src_edit->model.num_rows ||
        dst_row >= dst_edit->model.num_rows ||
        src_col >= src_edit->model.num_cols ||
        dst_col >= dst_edit->model.num_cols) {
      return;
    }

    if (src_row + num_copy_rows > src_edit->model.num_rows) {
      num_copy_rows = src_edit->model.num_rows - src_row;
    }

    if (dst_row + num_copy_rows > dst_edit->model.num_rows) {
      num_copy_rows = dst_edit->model.num_rows - dst_row;
    }

    if (src_col + num_copy_cols > src_edit->model.num_cols) {
      num_copy_cols = src_edit->model.num_cols - src_col;
    }

    if (dst_col + num_copy_cols > dst_edit->model.num_cols) {
      num_copy_cols = dst_edit->model.num_cols - dst_col;
    }
  }

  copy_area(src_edit, src_col, src_row, num_copy_cols, num_copy_rows,
            dst_edit, dst_col, dst_row);
}

void vt_edit_erase_area(vt_edit_t *edit, int col, int row, u_int num_cols, u_int num_rows) {
  if (!apply_relative_origin(edit, &col, &row, &num_cols, &num_rows)) {
    return;
  }

  erase_area(edit, col, row, num_cols, num_rows);
}

void vt_edit_change_attr_area(vt_edit_t *edit, int col, int row, u_int num_cols, u_int num_rows,
                              void (*func)(vt_char_t*, int, int, int, int, int, int, int),
                              int attr) {
  u_int count;
  vt_line_t *line;
  int char_index;
  int end_char_index;
  u_int cols_rest;
  int bold;
  int italic;
  int underline_style;
  int blinking;
  int reversed;
  int crossed_out;
  int overlined;

  if (attr == 0) {
    bold = italic = underline_style = blinking = reversed = crossed_out = overlined = -1;
  } else {
    bold = italic = underline_style = blinking = reversed = crossed_out = overlined = 0;

    if (attr == 1) {
      bold = 1;
    } else if (attr == 3) {
      italic = 1;
    } else if (attr == 4) {
      underline_style = LS_UNDERLINE_SINGLE;
    } else if (attr == 5 || attr == 6) {
      blinking = 1;
    } else if (attr == 7) {
      reversed = 1;
    } else if (attr == 9) {
      crossed_out = 1;
    } else if (attr == 21) {
      underline_style = LS_UNDERLINE_DOUBLE;
    } else if (attr == 22) {
      bold = -1;
    } else if (attr == 23) {
      italic = -1;
    } else if (attr == 24) {
      underline_style = -1;
    } else if (attr == 25) {
      blinking = -1;
    } else if (attr == 27) {
      reversed = -1;
    } else if (attr == 29) {
      crossed_out = -1;
    } else if (attr == 53) {
      overlined = 1;
    } else if (attr == 55) {
      overlined = -1;
    } else {
      return;
    }
  }

  /*
   * XXX
   * apply_relative_origin() adjusts arguments regarding edit->use_rect_attr_select as true
   * all the time.
   */
  if (!apply_relative_origin(edit, &col, &row, &num_cols, &num_rows)) {
    return;
  }

  for (count = 0; count < num_rows; count++) {
    if (count == 1 && !edit->use_rect_attr_select) {
      int old_col;

      old_col = col;
      col = edit->is_relative_origin ? edit->hmargin_beg : 0;
      num_cols += (old_col - col);
    }

    if (!(line = vt_edit_get_line(edit, row + count))) {
      continue;
    }

    char_index = vt_convert_col_to_char_index(line, &cols_rest, col, BREAK_BOUNDARY);
    if (char_index >= line->num_filled_chars && attr > 7) {
      continue;
    }

    if (cols_rest > 0) {
      char_index++;
    }

    if (edit->use_rect_attr_select || count + 1 == num_rows) {
      end_char_index =
          vt_convert_col_to_char_index(line, NULL, col + num_cols - 1, BREAK_BOUNDARY);
    } else {
      end_char_index = vt_convert_col_to_char_index(
          line, NULL, edit->is_relative_origin ? edit->hmargin_end : vt_edit_get_cols(edit) - 1,
          BREAK_BOUNDARY);
    }

    vt_line_assure_boundary(line, end_char_index);

    vt_line_set_modified(line, char_index, end_char_index);

    for (; char_index <= end_char_index; char_index++) {
      (*func)(vt_char_at(line, char_index), bold, italic, underline_style, blinking, reversed,
              crossed_out, overlined);
    }
  }
}

u_int16_t vt_edit_get_checksum(vt_edit_t *edit, int col, int row, u_int num_cols, u_int num_rows) {
  int count;
  u_int16_t checksum;
  vt_line_t *line;
  int char_index;
  u_int cols_rest;

  if (!apply_relative_origin(edit, &col, &row, &num_cols, &num_rows)) {
    return 0;
  }

  checksum = 0;
  for (count = 0; count < num_rows; count++) {
    if ((line = vt_edit_get_line(edit, row + count))) {
      int end_char_index;
      vt_char_t *ch;

      if ((char_index = vt_convert_col_to_char_index(line, &cols_rest, col, BREAK_BOUNDARY)) >=
          line->num_filled_chars) {
        continue;
      }

      if (cols_rest > 0) {
        char_index ++;
        checksum --; /* 2nd column of full width character is 0xFFFF (compat with xterm) */
      }

      if ((end_char_index = vt_convert_col_to_char_index(line, &cols_rest,
                                                         col + num_cols, BREAK_BOUNDARY)) >
#if 0
          line->num_filled_chars
#else
          vt_line_get_num_filled_chars_except_sp(line) /* compat with xterm */
#endif
          ) {
#if 0
        end_char_index = line->num_filled_chars;
#else
        end_char_index = vt_line_get_num_filled_chars_except_sp(line); /* compat with xterm */
#endif
      }

      if (cols_rest > 0) {
        end_char_index++;
        checksum ++; /* col + num_cols - 1 is 1st column of full width character */
      }

      for(; char_index < end_char_index; char_index++) {
        ch = vt_char_at(line, char_index);
        checksum += vt_char_code(ch);
        if (vt_char_cols(ch) == 2) {
          checksum --; /* 2nd column of full width character is 0xFFFF (compat with xterm) */
        }
      }
    }
  }

  return checksum;
}

void vt_edit_clear_size_attr(vt_edit_t *edit) {
  u_int count;

  for (count = 0; count < edit->model.num_rows; count++) {
    vt_line_set_size_attr(vt_edit_get_line(edit, count), 0);
  }
}

int vt_edit_cursor_logical_col(vt_edit_t *edit) {
  if (edit->is_relative_origin) {
    if (edit->cursor.col > edit->hmargin_beg) {
      return edit->cursor.col - edit->hmargin_beg;
    } else {
      return 0;
    }
  }

  return edit->cursor.col;
}

int vt_edit_cursor_logical_row(vt_edit_t *edit) {
  if (edit->is_relative_origin) {
    if (edit->cursor.row > edit->vmargin_beg) {
      return edit->cursor.row - edit->vmargin_beg;
    } else {
      return 0;
    }
  }

  return edit->cursor.row;
}

/*
 * for debugging.
 */

#ifdef DEBUG

void vt_edit_dump(vt_edit_t *edit) {
  int row;
  vt_line_t *line;

  bl_debug_printf(BL_DEBUG_TAG " ===> dumping edit...[cursor(index)%d (col)%d (row)%d]\n",
                  edit->cursor.char_index, edit->cursor.col, edit->cursor.row);

  for (row = 0; row < edit->model.num_rows; row++) {
    int char_index;

    line = vt_model_get_line(&edit->model, row);

    if (vt_line_is_modified(line)) {
      if (vt_line_is_cleared_to_end(line)) {
        bl_msg_printf("!%.2d-END", vt_line_get_beg_of_modified(line));
      } else {
        bl_msg_printf("!%.2d-%.2d", vt_line_get_beg_of_modified(line),
                      vt_line_get_end_of_modified(line));
      }
    } else {
      bl_msg_printf("      ");
    }

    bl_msg_printf("[%.2d %.2d]", line->num_filled_chars, vt_line_get_num_filled_cols(line));

    if (line->num_filled_chars > 0) {
      for (char_index = 0; char_index < line->num_filled_chars; char_index++) {
        if (edit->cursor.row == row && edit->cursor.char_index == char_index) {
          bl_msg_printf("**");
        }

        vt_char_dump(vt_char_at(line, char_index));

        if (edit->cursor.row == row && edit->cursor.char_index == char_index) {
          bl_msg_printf("**");
        }
      }
    }

    bl_msg_printf("\n");
  }

  bl_debug_printf(BL_DEBUG_TAG " <=== end of edit.\n\n");
}

void vt_edit_dump_updated(vt_edit_t *edit) {
  int row;

  for (row = 0; row < edit->model.num_rows; row++) {
    bl_msg_printf("(%.2d)%d ", row, vt_line_is_modified(vt_model_get_line(&edit->model, row)));
  }

  bl_msg_printf("\n");
}

#endif
