/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "vt_screen.h"

#include <stdlib.h> /* abs */
#include <unistd.h> /* write */
#include <sys/time.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h>  /* malloc/free */
#include <pobl/bl_str.h>  /* strdup */
#include <pobl/bl_util.h> /* BL_MIN, BL_MAX */

#include "vt_char_encoding.h"
#include "vt_str_parser.h"

#define ROW_IN_LOGS(screen, row) (vt_get_num_logged_lines(&(screen)->logs) + row)

#if 1
#define EXIT_BS_AT_BOTTOM
#endif

/* --- static variables --- */

static char *word_separators = " .,:;/|@()[]{}";
static int regard_uri_as_word = 0;

/* --- static functions --- */

static int get_n_prev_char_pos(vt_screen_t *screen, int *char_index, int *row, int n) {
  int count;

  *char_index = vt_cursor_char_index(screen->edit);
  *row = vt_cursor_row(screen->edit);

  for (count = 0; count < n; count++) {
    if (*char_index == 0) {
      return 0;
    }

    (*char_index)--;
  }

  return 1;
}

static int is_word_separator(vt_char_t *ch) {
  char *p;
  char c;

  if (vt_char_cs(ch) != US_ASCII) {
    return 0;
  }

  p = word_separators;
  c = vt_char_code(ch);

  while (*p) {
    if (c == *p) {
      return 1;
    }

    p++;
  }

  return 0;
}

/*
 * callbacks of vt_edit_scroll_event_listener_t.
 */

/*
 * Don't operate vt_model_t in this function because vt_model_t is not scrolled
 * yet.
 * Operate vt_model_t in scrolled_out_lines_finished().
 */
static void receive_scrolled_out_line(void *p, vt_line_t *line) {
  vt_screen_t *screen;
  u_int logical_num_filled_chars;

  screen = p;

  if (vt_status_line_is_focused(screen)) {
    vt_line_set_size_attr(line, 0);
    line->mark = 0;

    return;
  }

  if (screen->screen_listener && screen->screen_listener->line_scrolled_out) {
    (*screen->screen_listener->line_scrolled_out)(screen->screen_listener->self);
  }

  logical_num_filled_chars = line->num_filled_chars;

  if (screen->logvis) {
    (*screen->logvis->visual_line)(screen->logvis, line);
  } else {
    line->num_filled_chars =
        vt_line_get_num_filled_chars_except_sp_with_func(line, vt_char_equal);
  }

  vt_log_add(&screen->logs, line, BL_MAX(line->num_filled_chars, logical_num_filled_chars));

/* XXX see vt_line_iscii_visual() */
#if 1
  if (line->num_chars > vt_screen_get_logical_cols(screen)) {
    /*
     * line->num_filled_chars can be more than line->num_chars
     * without vt_line_reset() here because line is visualized.
     */
    vt_line_reset(line);
    vt_line_set_updated(line);

    vt_str_final(line->chars + vt_screen_get_logical_cols(screen),
                 line->num_chars - vt_screen_get_logical_cols(screen));
    line->num_chars = vt_screen_get_logical_cols(screen);
  } else
#endif
  {
    vt_line_set_size_attr(line, 0);
    line->mark = 0;
  }

  if (vt_screen_is_backscrolling(screen) == BSM_STATIC) {
    screen->backscroll_rows++;
  }

  if (screen->search) {
    screen->search->row--;
  }
}

static void scrolled_out_lines_finished(void *p) {
  vt_screen_t *screen;

  screen = p;

  if (vt_screen_is_backscrolling(screen) == BSM_DEFAULT) {
    vt_screen_set_modified_all(screen);
  }
}

static int window_scroll_upward_region(void *p, int beg_row, int end_row, u_int size) {
  vt_screen_t *screen;

  screen = p;

  if (screen->is_backscrolling) {
    /*
     * Not necessary to scrolling window. If backscroll_mode is BSM_DEFAULT,
     * vt_screen_set_modified_all() in scrolled_out_lines_finished() later.
     */
    return 1;
  }

  if (!screen->screen_listener || !screen->screen_listener->window_scroll_upward_region) {
    return 0;
  }

  return (*screen->screen_listener->window_scroll_upward_region)(screen->screen_listener->self,
                                                                 beg_row, end_row, size);
}

static int window_scroll_downward_region(void *p, int beg_row, int end_row, u_int size) {
  vt_screen_t *screen;

  screen = p;

  if (screen->is_backscrolling) {
    /*
     * Not necessary to scrolling window. If backscroll_mode is BSM_DEFAULT,
     * vt_screen_set_modified_all() in scrolled_out_lines_finished() later.
     */
    return 1;
  }

  if (!screen->screen_listener || !screen->screen_listener->window_scroll_downward_region) {
    return 0;
  }

  return (*screen->screen_listener->window_scroll_downward_region)(screen->screen_listener->self,
                                                                   beg_row, end_row, size);
}

static int top_line_is_wrapped(void *p) {
  vt_screen_t *screen;
  u_int num;

  screen = p;

  if ((num = vt_get_num_logged_lines(&screen->logs)) > 0) {
    if (vt_line_is_continued_to_next(vt_log_get(&screen->logs, num - 1))) {
      return 1;
    }
  }

  return 0;
}

static int modify_region(vt_screen_t *screen, /* visual */
                         int *end_char_index, int *end_row) {
  int row;
  vt_line_t *line;

  /* Removing empty lines of the end. */

  row = *end_row;

  while ((line = vt_screen_get_line(screen, row)) == NULL || vt_line_is_empty(line)) {
    if (--row < 0 && abs(row) > vt_get_num_logged_lines(&screen->logs)) {
      return 0;
    }
  }

  if (row < *end_row) {
    if (vt_line_is_rtl(line)) {
      *end_char_index = vt_line_beg_char_index_regarding_rtl(line);
    } else {
      if ((*end_char_index = vt_line_get_num_filled_chars_except_sp(line)) > 0) {
        (*end_char_index)--;
      }
    }

    *end_row = row;
  }

  return 1;
}

static void convert_col_to_char_index(vt_screen_t *screen, /* visual */
                                      vt_line_t *line, int *beg_char_index,
                                      int *end_char_index, /* end + 1 */
                                      int beg_col, int end_col) {
  int padding;
  int beg;
  int end;
  u_int rest;

  if (vt_line_is_rtl(line) &&
      (padding = vt_screen_get_cols(screen) - vt_line_get_num_filled_cols(line)) > 0) {
    beg_col -= padding;
    end_col -= padding;
  }

  *beg_char_index = vt_convert_col_to_char_index(line, &rest, beg_col, 0);

  if ((end = vt_line_get_num_filled_chars_except_sp(line)) <= *beg_char_index ||
      (end == *beg_char_index + 1 && rest > 0)) {
    *beg_char_index = end;
  } else if ((beg = vt_line_beg_char_index_regarding_rtl(line)) > *beg_char_index) {
    *beg_char_index = beg;
  }

  *end_char_index = vt_convert_col_to_char_index(line, NULL, end_col, 0) + 1;
  if (end < *end_char_index) {
    *end_char_index = end;
  }
}

static int reverse_or_restore_color_rect(vt_screen_t *screen, /* visual */
                                         int beg_col, int beg_row, int end_col, int end_row,
                                         int (*func)(vt_line_t *, int)) {
  int row;
  int beg_char_index;
  int end_char_index;
  vt_line_t *line;

  if (beg_col > end_col) {
    int col;

    col = beg_col;
    beg_col = end_col;
    end_col = col;
  }

  for (row = beg_row; row <= end_row; row++) {
    if ((line = vt_screen_get_line(screen, row))) {
      int char_index;

      convert_col_to_char_index(screen, line, &beg_char_index, &end_char_index, beg_col, end_col);

      for (char_index = beg_char_index; char_index < end_char_index; char_index++) {
        (*func)(line, char_index);
      }
    }
  }

  return 1;
}

static int reverse_or_restore_color(vt_screen_t *screen, /* visual */
                                    int beg_char_index, int beg_row, int end_char_index,
                                    int end_row, int (*func)(vt_line_t *, int)) {
  /*
   * LTR line:    a<aa bbb ccc>
   *                ^beg     ^end
   * RTL line:    c<cc bbb aaa>
   *                ^end     ^beg
   *              ^beg_regarding_rtl
   * <>: selected region
   */

  int char_index;
  int row;
  vt_line_t *line;
  u_int size_except_sp;
  int beg_regarding_rtl;

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " reverse/restore region: %d %d %d %d\n", beg_char_index, beg_row,
                  end_char_index, end_row);
#endif

  if (!modify_region(screen, &end_char_index, &end_row)) {
    return 0;
  }

  /* Removing empty lines of the beginning. */

  row = beg_row;

  while (1) {
    if (!(line = vt_screen_get_line(screen, row)) || vt_line_is_empty(line)) {
      goto next_line;
    }

    size_except_sp = vt_line_get_num_filled_chars_except_sp(line);
    beg_regarding_rtl = vt_line_beg_char_index_regarding_rtl(line);

    if (vt_line_is_rtl(line)) {
      if (row > beg_row || beg_char_index >= size_except_sp) {
        beg_char_index = BL_MAX(size_except_sp, 1) - 1;
      } else if (beg_char_index < beg_regarding_rtl) {
        goto next_line;
      }
    } else {
      if (row > beg_row || beg_char_index < beg_regarding_rtl) {
        beg_char_index = beg_regarding_rtl;
      } else if (beg_char_index >= size_except_sp) {
        goto next_line;
      }
    }

    break;

  next_line:
    if (++row > end_row) {
      return 0;
    }
  }

  if (row < end_row) {
    if (vt_line_is_rtl(line)) {
      for (char_index = beg_regarding_rtl; char_index <= beg_char_index; char_index++) {
        (*func)(line, char_index);
      }
    } else {
      for (char_index = beg_char_index; char_index < size_except_sp; char_index++) {
        (*func)(line, char_index);
      }
    }

    for (row++; row < end_row; row++) {
      if (vt_line_is_empty((line = vt_screen_get_line(screen, row)))) {
        continue;
      }

      size_except_sp = vt_line_get_num_filled_chars_except_sp(line);

      for (char_index = vt_line_beg_char_index_regarding_rtl(line); char_index < size_except_sp;
           char_index++) {
        (*func)(line, char_index);
      }
    }

    if (vt_line_is_empty((line = vt_screen_get_line(screen, row)))) {
      return 1;
    }

    size_except_sp = vt_line_get_num_filled_chars_except_sp(line);
    beg_regarding_rtl = vt_line_beg_char_index_regarding_rtl(line);

    if (vt_line_is_rtl(line)) {
      beg_char_index = BL_MAX(size_except_sp, 1) - 1;
    } else {
      beg_char_index = beg_regarding_rtl;
    }
  }

  /* row == end_row */

  if (vt_line_is_rtl(line)) {
    if (end_char_index < size_except_sp) {
      for (char_index = BL_MAX(end_char_index, beg_regarding_rtl); char_index <= beg_char_index;
           char_index++) {
        (*func)(line, char_index);
      }
    }
  } else {
    if (end_char_index >= beg_regarding_rtl) {
      for (char_index = beg_char_index; char_index < BL_MIN(end_char_index + 1, size_except_sp);
           char_index++) {
        (*func)(line, char_index);
      }
    }
  }

  return 1;
}

static u_int check_or_copy_region_rect(
    vt_screen_t *screen, /* visual */
    vt_char_t *chars,    /* Behavior is undefined if chars is insufficient. */
    u_int num_chars, int beg_col, int beg_row, int end_col, int end_row) {
  int row;
  vt_line_t *line;
  u_int region_size;
  int beg_char_index;
  int end_char_index;

  if (beg_col > end_col) {
    int col;

    col = beg_col;
    beg_col = end_col;
    end_col = col;
  }

  region_size = 0;

  for (row = beg_row; row <= end_row; row++) {
    if ((line = vt_screen_get_line(screen, row))) {
      u_int size;

      convert_col_to_char_index(screen, line, &beg_char_index, &end_char_index, beg_col, end_col);

      size = end_char_index - beg_char_index;

      if (chars && num_chars >= region_size + size) {
        vt_line_copy_logical_str(line, chars + region_size, beg_char_index, size);
      }

      region_size += size;

      if (row < end_row) {
        if (chars && num_chars >= region_size + 1) {
          vt_char_copy(chars + region_size, vt_nl_ch());
        }

        region_size++;
      }
    }
  }

  return region_size;
}

static u_int check_or_copy_region(
    vt_screen_t *screen,                    /* visual */
    vt_char_t *chars,                       /* Behavior is undefined if chars is insufficient. */
    u_int num_chars, int beg_char_index, /* can be over size_except_sp */
    int beg_row, int end_char_index,        /* can be over size_except_sp */
    int end_row) {
  /*
   * LTR line:    a<aa bbb ccc>
   *                ^beg     ^end
   * RTL line:    c<cc bbb aaa>
   *                ^end     ^beg
   *              ^beg_regarding_rtl
   * <>: selected region
   */

  vt_line_t *line;
  u_int size;
  u_int region_size;
  u_int size_except_sp;
  int beg_regarding_rtl;
  int row;

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " check/copy region: %d %d %d %d\n", beg_char_index, beg_row,
                  end_char_index, end_row);
#endif

  if (!modify_region(screen, &end_char_index, &end_row)) {
    return 0;
  }

  row = beg_row;

  /* Removing empty lines of the beginning. */
  while (1) {
    if (!(line = vt_screen_get_line(screen, row)) || vt_line_is_empty(line)) {
      goto next_line;
    }

    size_except_sp = vt_line_get_num_filled_chars_except_sp(line);
    beg_regarding_rtl = vt_line_beg_char_index_regarding_rtl(line);

    if (vt_line_is_rtl(line)) {
      if (row > beg_row || beg_char_index >= size_except_sp) {
        beg_char_index = BL_MAX(size_except_sp, 1) - 1;
      } else if (beg_char_index < beg_regarding_rtl) {
        goto next_line;
      }
    } else {
      if (row > beg_row || beg_char_index < beg_regarding_rtl) {
        beg_char_index = beg_regarding_rtl;
      } else if (beg_char_index >= size_except_sp) {
        goto next_line;
      }
    }

    break;

  next_line:
    if (++row > end_row) {
      return 0;
    }
  }

  region_size = 0;

  if (row < end_row) {
    if (vt_line_is_rtl(line)) {
      size = beg_char_index - beg_regarding_rtl + 1;

      if (chars && num_chars >= region_size + size) {
        vt_line_copy_logical_str(line, chars + region_size, beg_regarding_rtl, size);
      }
    } else {
      size = size_except_sp - beg_char_index;

      if (chars && num_chars >= region_size + size) {
        vt_line_copy_logical_str(line, chars + region_size, beg_char_index, size);
      }
    }

    region_size += size;

    if (!vt_line_is_continued_to_next(line)) {
      if (chars && num_chars > region_size) {
        vt_char_copy(chars + region_size, vt_nl_ch());
      }
      region_size++;
    }

    for (row++; row < end_row; row++) {
      line = vt_screen_get_line(screen, row);

      beg_regarding_rtl = vt_line_beg_char_index_regarding_rtl(line);
      size = vt_line_get_num_filled_chars_except_sp(line) - beg_regarding_rtl;

      if (chars && num_chars >= region_size + size) {
        vt_line_copy_logical_str(line, chars + region_size, beg_regarding_rtl, size);
      }

      region_size += size;

      if (!vt_line_is_continued_to_next(line)) {
        if (chars && num_chars > region_size) {
          vt_char_copy(chars + region_size, vt_nl_ch());
        }
        region_size++;
      }
    }

    if (vt_line_is_empty((line = vt_screen_get_line(screen, row)))) {
      return region_size;
    }

    size_except_sp = vt_line_get_num_filled_chars_except_sp(line);
    beg_regarding_rtl = vt_line_beg_char_index_regarding_rtl(line);

    if (vt_line_is_rtl(line)) {
      beg_char_index = BL_MAX(size_except_sp, 1) - 1;
    } else {
      beg_char_index = beg_regarding_rtl;
    }
  }

  /* row == end_row */

  if (size_except_sp == 0) {
    /* do nothing */
  } else if (vt_line_is_rtl(line)) {
    if (end_char_index < size_except_sp) {
      if (end_char_index < beg_regarding_rtl) {
        end_char_index = beg_regarding_rtl;
      }

      if (beg_row == end_row && beg_char_index < end_char_index) {
        int tmp = end_char_index;
        end_char_index = beg_char_index;
        beg_char_index = tmp;
      }

      size = beg_char_index - end_char_index + 1;

      if (chars && num_chars >= region_size + size) {
        vt_line_copy_logical_str(line, chars + region_size, end_char_index, size);
      }

      region_size += size;
    }
  } else {
    if (end_char_index >= beg_regarding_rtl) {
      if (end_char_index >= size_except_sp) {
        end_char_index = size_except_sp - 1;
      }

      size = end_char_index - beg_char_index + 1;

      if (chars && num_chars >= region_size + size) {
        vt_line_copy_logical_str(line, chars + region_size, beg_char_index, size);
      }

      region_size += size;
    }
  }

  return region_size;
}

static u_int32_t get_msec_time(void) {
#ifdef HAVE_GETTIMEOFDAY
  struct timeval tv;

  gettimeofday(&tv, NULL);

  /* XXX '/ 1000' => '>> 10' and '* 1000' => '<< 10' */
  return (tv.tv_sec << 10) + (tv.tv_usec >> 10);
#else
  return time(NULL) << 10;
#endif
}

static void change_edit(vt_screen_t *screen, vt_edit_t *edit) {
  vt_screen_disable_local_echo(screen);

  if (edit != screen->status_edit) {
    if (screen->logvis) {
      (*screen->logvis->init)(screen->logvis, &edit->model, &edit->cursor);
    }

    if (screen->main_edit) {
      screen->main_edit = edit;
    }

    if (screen->edit != screen->status_edit) {
      vt_edit_set_modified_all(edit);
    }
  } else {
    screen->main_edit = screen->edit;
  }

  edit->bce_ch = screen->edit->bce_ch;

  screen->edit = edit;
}

static vt_edit_t *get_edit(vt_screen_t *screen, u_int page_id) {
  if (page_id == 0) {
    if (vt_screen_is_alternative_edit(screen)) {
      return &screen->alt_edit;
    } else {
      return &screen->normal_edit;
    }
  } else if (page_id <= MAX_PAGE_ID) {
    if (screen->page_edits == NULL) {
      int count;

      if (!(screen->page_edits = malloc(sizeof(vt_edit_t) * MAX_PAGE_ID))) {
        return NULL;
      }

      for (count = 0; count < MAX_PAGE_ID; count++) {
        vt_edit_init(screen->page_edits + count, &screen->edit_scroll_listener,
                     vt_edit_get_cols(&screen->normal_edit),
                     vt_edit_get_rows(&screen->normal_edit),
                     vt_edit_get_tab_size(&screen->normal_edit), 1,
                     screen->normal_edit.use_bce);
      }
    }

    return screen->page_edits + (page_id - 1);
  }

  return NULL;
}

static vt_edit_t *status_edit_new(vt_edit_t *main_edit) {
  vt_edit_t *status_edit;

  if ((status_edit = malloc(sizeof(vt_edit_t)))) {
    vt_edit_init(status_edit, main_edit->scroll_listener,
                 vt_edit_get_cols(main_edit), 1, vt_edit_get_tab_size(main_edit), 1,
                 main_edit->use_bce);
  }

  return status_edit;
}

static u_int get_wrap_lines_in_logs(vt_logs_t *logs, u_int *cols, u_int *rows, int beg) {
  vt_line_t *line;
  u_int num_chars = 0;
  int row;
  u_int num;

  for (row = beg; row > 0 && vt_line_is_continued_to_next(vt_log_get(logs, row - 1)); row--);

  *rows = beg - row + 1;
  *cols = 0;

  for(; row < beg; row++) {
    line = vt_log_get(logs, row);
    vt_line_ctl_logical(line); /* logged lines are always visualized. */
    *cols += vt_str_cols(line->chars, line->num_filled_chars);
    num_chars += line->num_filled_chars;
    vt_line_ctl_visual(line);
  }

  line = vt_log_get(logs, row);
  vt_line_ctl_logical(line); /* logged lines are always visualized. */
  if (!vt_line_is_continued_to_next(line)) {
    num = vt_line_get_num_filled_chars_except_sp(line);
  } else {
    num = line->num_filled_chars;
  }
  *cols += vt_str_cols(line->chars, num);
  num_chars += num;
  vt_line_ctl_visual(line);

  return num_chars;
}

static u_int get_wrap_lines_in_edit(vt_edit_t *edit, vt_char_t *chars,
                                    u_int *cols, u_int *rows, int beg) {
  u_int num_chars = 0;
  vt_line_t *line;
  int row = beg;

  if (cols) {
    *cols = 0;
  }

  while ((line = vt_edit_get_line(edit, row++))) {
    if (!vt_line_is_continued_to_next(line)) {
      u_int num = vt_line_get_num_filled_chars_except_sp(line);

      if (chars) {
        vt_str_copy(chars + num_chars, line->chars, num);
      } else {
        *cols += vt_str_cols(line->chars, num);
      }
      num_chars += num;

      break;
    } else {
      if (chars) {
        vt_str_copy(chars + num_chars, line->chars, line->num_filled_chars);
      } else {
        *cols += vt_str_cols(line->chars, line->num_filled_chars);
      }
      num_chars += line->num_filled_chars;
    }
  }

  if (rows) {
    *rows = row - beg;
  }

  return num_chars;
}

/*
 * Don't call this if vt_edit_is_logging(screen->edit) is false.
 * comb_logical() is not called for lines reverted to the main screen, so
 * dynamic combining is corrupt.
 */
static void rollback(vt_screen_t *screen, u_int dst_cols, u_int dst_rows) {
  int src_row;
  int src_end_row; /* end row in backlog */
  u_int dst_row;
  u_int n_scrollback;
  vt_line_t *line;
  vt_char_t *chars;
  vt_char_t *chars_in_edit;
  u_int src_cols_in_edit;
  u_int src_nchars_in_edit;
  u_int src_rows_in_edit;
  u_int src_nchars;
  u_int max_nchars;
  u_int src_cols;
  u_int count;
  int cursor_col = vt_cursor_col(screen->edit);
  int cursor_row = vt_cursor_row(screen->edit);

  if ((src_end_row = vt_get_num_logged_lines(&screen->logs)) == 0) {
    return;
  }
  src_row = (--src_end_row);

  if (vt_line_is_continued_to_next(vt_log_get(&screen->logs, src_row))) {
    src_nchars_in_edit = get_wrap_lines_in_edit(screen->edit, NULL,
                                                &src_cols_in_edit, &src_rows_in_edit, 0);
    src_cols = src_cols_in_edit;
    dst_rows += src_rows_in_edit;

    if ((chars_in_edit = vt_str_alloca(src_nchars_in_edit)) == NULL) {
      return;
    }
    vt_str_init(chars_in_edit, src_nchars_in_edit);

    get_wrap_lines_in_edit(screen->edit, chars_in_edit, NULL, NULL, 0);
  } else {
    src_nchars_in_edit = src_cols = src_cols_in_edit = src_rows_in_edit = 0;
  }
  max_nchars = src_nchars = 0;

  count = 0;
  while (1) {
    u_int n;
    u_int r;
    u_int c;
    u_int tmp;

    n = get_wrap_lines_in_logs(&screen->logs, &c, &r, src_row);

    if (src_cols + c == 0) {
      tmp = 1;
    } else {
      tmp = (src_cols + c - 1) / dst_cols + 1;
    }
    if (tmp > dst_rows - count) {
      break;
    }

    if (count == 0) {
      n += src_nchars_in_edit;

      if (cursor_row < src_rows_in_edit) {
        /*
         * XXX
         * This calculation is incorrect if a character at the end of line
         * is full width.
         */
        cursor_col += (c + cursor_row * dst_cols);
        cursor_row = cursor_col / dst_cols;
        cursor_col -= (cursor_row * dst_cols);
        /*
         * The number of rows reverted to the screen is added to cursor_row by
         * 'cursor_row += (dst_rows - src_rows_in_edit)' below.
         */
        cursor_row -= (cursor_row + 1 - src_rows_in_edit);
      }
    }

    if (n > max_nchars) {
      max_nchars = n;
    }

    count += tmp;
    src_cols = 0;
    src_nchars += n;
    if ((src_row -= r) < 0) {
      break;
    }
  }

  if ((dst_rows = count) == 0) {
    return;
  }

  n_scrollback = src_end_row - src_row;
  src_row++;

  if ((chars = vt_str_alloca(max_nchars)) == NULL) {
    return;
  }
  vt_str_init(chars, max_nchars);

  vt_edit_scroll_downward(screen->edit, dst_rows - src_rows_in_edit);
  cursor_row += (dst_rows - src_rows_in_edit);

  dst_row = 0;
  while (src_row <= src_end_row) {
    u_int n;

    line = vt_log_get(&screen->logs, src_row++);
    src_cols = 0;
    src_nchars = 0;

    /* vt_line_ctl_visual() is not called because vt_log_rollback_index() clears it. */
    vt_line_ctl_logical(line);
    while (1) {
      if (!vt_line_is_continued_to_next(line)) {
        n = vt_line_get_num_filled_chars_except_sp(line);
        vt_str_copy(chars + src_nchars, line->chars, n);
        src_nchars += n;
        src_cols += vt_str_cols(line->chars, n);

        break;
      } else {
        vt_str_copy(chars + src_nchars, line->chars, line->num_filled_chars);
        src_nchars += line->num_filled_chars;
        src_cols += vt_str_cols(line->chars, line->num_filled_chars);

        if (src_row > src_end_row) {
          vt_str_copy(chars + src_nchars, chars_in_edit, src_nchars_in_edit);
          src_nchars += src_nchars_in_edit;
          src_cols += src_cols_in_edit;

          break;
        } else {
          line = vt_log_get(&screen->logs, src_row++);
          /* vt_line_ctl_visual() is not called because vt_log_rollback_index() clears it. */
          vt_line_ctl_logical(line);
        }
      }
    }

    if (src_nchars > 0) {
      dst_row += vt_edit_replace(screen->edit, dst_row, chars, src_cols, dst_cols);
    } else {
      dst_row++;
    }
  }

  vt_edit_goto(screen->edit, cursor_col, cursor_row);

  vt_str_final(chars, max_nchars);
  if (src_nchars_in_edit > 0) {
    vt_str_final(chars_in_edit, src_nchars_in_edit);
  }

  vt_log_rollback_index(&screen->logs, n_scrollback);
}

static int resize(vt_screen_t *screen, u_int cols /* > 0 */, u_int rows /* > 0 */, int pack) {
  /* This considers status line ('rows' contains status line) */
  u_int old_empty_rows;
  u_int empty_rows;

  old_empty_rows = vt_edit_get_rows(screen->edit) - vt_edit_get_num_filled_rows(screen->edit);

  if (screen->status_edit) {
    vt_edit_resize(screen->status_edit, cols, 1);

    if (vt_screen_has_status_line(screen) && rows >= 2) {
      rows--;
    }
  }

  switch(vt_edit_resize(&screen->normal_edit, cols, rows)) {
  case 2: /* RZ_WRAP */
    if (vt_get_num_logged_lines(&screen->logs) > 0) {
      screen->need_rewrap_logs = 1;
    }
    break;

  case 3: /* RZ_SCROLL */
    pack = 0;
    break;
  }

#ifdef DEBUG
  {
    vt_line_t *line = vt_edit_get_line(&screen->normal_edit,
                                       vt_cursor_row(&screen->normal_edit));

    if (line->num_filled_chars <= screen->normal_edit.cursor.char_index) {
      bl_debug_printf("Cursor posion (%d %d) is over the end of line (%d)\n",
                      screen->normal_edit.cursor.char_index,
                      screen->normal_edit.cursor.row,
                      line->num_filled_chars);
      abort();
    }
  }
#endif

  vt_edit_resize(&screen->alt_edit, cols, rows);

  if (screen->stored_edit) {
    vt_edit_resize(&screen->stored_edit->edit, cols, rows);
  }

  if (screen->page_edits) {
    u_int count;

    for (count = 0; count < MAX_PAGE_ID; count++) {
      vt_edit_resize(screen->page_edits + count, cols, rows);
    }
  }

  empty_rows = vt_edit_get_rows(screen->edit) - vt_edit_get_num_filled_rows(screen->edit);

  if (pack && vt_edit_is_logging(screen->edit) && empty_rows > old_empty_rows) {
    u_int n_scroll = vt_get_num_logged_lines(&screen->logs);

    if (n_scroll > 0) {
      if (n_scroll > empty_rows - old_empty_rows) {
        n_scroll = empty_rows - old_empty_rows;
      }

#if 0
      vt_edit_scroll_downward(screen->edit, n_scroll);

      for (count = 0; count < n_scroll; count++) {
        vt_line_t *src = vt_log_get(&screen->logs,
                                    vt_get_num_logged_lines(&screen->logs) - n_scroll + count);
        vt_line_t *dst = vt_edit_get_line(screen->edit, count);
        /* vt_screen_resize() is always called in logical context. */
        vt_line_ctl_logical(src);
        vt_line_copy(dst, src);
        vt_line_set_modified_all(dst);

        vt_edit_go_downward(screen->edit, 0);
      }

      vt_log_rollback_index(&screen->logs, n_scroll);
#else
      rollback(screen, cols, n_scroll);
#endif

      return 2;
    }
  }

  return 1;
}

static int rewrap_logs(vt_screen_t *screen) {
  vt_logs_t new_logs;
  u_int src_row;
  vt_char_t *buf;
  u_int max_nchars = 0;
  u_int num_cols;
  u_int num_log_lines;
  vt_line_t new_line;
  int last_line_is_wrapped;

  if ((num_log_lines = vt_get_num_logged_lines(&screen->logs)) == 0) {
    return 1;
  }

  vt_log_init(&new_logs, vt_get_log_size(&screen->logs));
  if (vt_log_size_is_unlimited(&screen->logs)) {
    vt_unlimit_log_size(&new_logs);
  }

  num_cols = vt_edit_get_cols(screen->edit);
  last_line_is_wrapped = vt_line_is_continued_to_next(vt_log_get(&screen->logs, num_log_lines - 1));

  for (src_row = 0; src_row < num_log_lines; src_row++) {
    vt_line_t *line = vt_log_get(&screen->logs, src_row);
    vt_char_t *wrap_chars;
    u_int wrap_ncols;
    u_int len;
    u_int cols;

    vt_line_ctl_logical(line);

    if (!vt_line_is_continued_to_next(line)) {
      wrap_ncols = vt_str_cols(line->chars, vt_line_get_num_filled_chars_except_sp(line));
      if (wrap_ncols <= num_cols) {
        u_int logical_num_filled_chars = line->num_filled_chars;

        vt_line_ctl_visual(line);
        vt_log_add(&new_logs, line, BL_MAX(line->num_filled_chars, logical_num_filled_chars));

        continue;
      }

      wrap_chars = line->chars;
    } else {
      u_int count;
      u_int count2;
      u_int nchars = line->num_filled_chars;

      wrap_ncols = vt_str_cols(line->chars, nchars);

      for (count = 1; src_row + count < num_log_lines;) {
        line = vt_log_get(&screen->logs, src_row + (count++));
        vt_line_ctl_logical(line);
        if (!vt_line_is_continued_to_next(line)) {
          u_int n = vt_line_get_num_filled_chars_except_sp(line);
          nchars += n;
          wrap_ncols += vt_str_cols(line->chars, n);
          break;
        } else {
          nchars += line->num_filled_chars;
          wrap_ncols += vt_str_cols(line->chars, line->num_filled_chars);
        }
      }

      if (nchars > max_nchars) {
        if (max_nchars > 0) {
          vt_str_final(buf, max_nchars);
        }

        max_nchars = nchars;

        if ((buf = vt_str_alloca(max_nchars)) == NULL) {
          return 0;
        }
        vt_str_init(buf, max_nchars);
      }

      for (count2 = 0, len = 0; ; count2++) {
        /* vt_line_ctl_logical() was applied above. */
        line = vt_log_get(&screen->logs, src_row + count2);

        if (count2 == count - 1) {
          u_int n = vt_line_get_num_filled_chars_except_sp(line);
          vt_str_copy(buf + len, line->chars, n);
          len += n;
          break;
        } else {
          vt_str_copy(buf + len, line->chars, line->num_filled_chars);
          len += line->num_filled_chars;
        }
      }

      wrap_chars = buf;

      src_row += (count - 1);
    }

    vt_line_init(&new_line, num_cols);
    cols = 0;
    len = 0;

    while (1) {
      u_int c;
      u_int n;
      u_int logical_num_filled_chars;

      if (cols + num_cols > wrap_ncols) {
        c = wrap_ncols - cols;
      } else {
        c = num_cols;
      }
      n = vt_str_cols_to_len(wrap_chars + len, &c);

      vt_line_overwrite(&new_line, 0, wrap_chars + len, n, c);

      logical_num_filled_chars = new_line.num_filled_chars;

      if (screen->logvis) {
        /* ctl_visual_line() calls not only vt_line_ctl_visual() but also ctl_render_line(). */
        (*screen->logvis->visual_line)(screen->logvis, &new_line);
      }

      len += n;
      cols += c;

      if (cols < wrap_ncols) {
        vt_line_set_continued_to_next(&new_line, 1);
        vt_log_add(&new_logs, &new_line,
                   BL_MAX(new_line.num_filled_chars, logical_num_filled_chars));
        vt_line_reset(&new_line);
      } else {
        vt_log_add(&new_logs, &new_line,
                   BL_MAX(new_line.num_filled_chars, logical_num_filled_chars));
        vt_line_final(&new_line);
        break;
      }
    }
  }

  vt_str_final(buf, max_nchars);

  if (last_line_is_wrapped) {
    vt_line_set_continued_to_next(vt_log_get(&new_logs, vt_get_num_logged_lines(&new_logs) - 1), 1);
  }

  vt_log_final(&screen->logs);
  screen->logs = new_logs;

  if (num_log_lines != vt_get_num_logged_lines(&screen->logs)) {
    return 2;
  }

  return 1;
}

/* --- global functions --- */

void vt_set_word_separators(const char *seps) {
  static char *default_word_separators;

  if (default_word_separators) {
    if (word_separators != default_word_separators) {
      free(word_separators);
    }

    if (seps == NULL || *seps == '\0') {
      /* Fall back to default. */
      word_separators = default_word_separators;

      return;
    }
  } else if (seps == NULL || *seps == '\0') {
    /* Not changed */
    return;
  } else {
    /* Store the default value. */
    default_word_separators = word_separators;
  }

  word_separators = bl_str_unescape(seps);
}

char *vt_get_word_separators(void) { return word_separators; }

void vt_set_regard_uri_as_word(int flag) { regard_uri_as_word = flag; }

int vt_get_regard_uri_as_word(void) { return regard_uri_as_word; }

vt_screen_t *vt_screen_new(u_int cols, u_int rows, u_int tab_size, u_int num_log_lines,
                           int use_bce, vt_bs_mode_t bs_mode) {
  vt_screen_t *screen;

  if ((screen = calloc(1, sizeof(vt_screen_t))) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " malloc failed.\n");
#endif

    return NULL;
  }

  screen->edit_scroll_listener.self = screen;
  screen->edit_scroll_listener.receive_scrolled_out_line = receive_scrolled_out_line;
  screen->edit_scroll_listener.scrolled_out_lines_finished = scrolled_out_lines_finished;
  screen->edit_scroll_listener.window_scroll_upward_region = window_scroll_upward_region;
  screen->edit_scroll_listener.window_scroll_downward_region = window_scroll_downward_region;
  screen->edit_scroll_listener.top_line_is_wrapped = top_line_is_wrapped;

  if (!vt_edit_init(&screen->normal_edit, &screen->edit_scroll_listener, cols, rows, tab_size, 1,
                    use_bce)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " vt_edit_init(normal_edit) failed.\n");
#endif

    goto error1;
  }

  if (!vt_edit_init(&screen->alt_edit, &screen->edit_scroll_listener, cols, rows, tab_size, 0,
                    use_bce)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " vt_edit_init(alt_edit) failed.\n");
#endif

    goto error2;
  }

  screen->edit = &screen->normal_edit;

  if (!vt_log_init(&screen->logs, num_log_lines)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " vt_log_init failed.\n");
#endif

    goto error3;
  }

  screen->backscroll_mode = bs_mode;

  return screen;

error3:
  vt_edit_final(&screen->normal_edit);
error2:
  vt_edit_final(&screen->alt_edit);
error1:
  free(screen);

  return NULL;
}

int vt_screen_destroy(vt_screen_t *screen) {
  /*
   * this should be done before vt_edit_final() since termscr->logvis refers
   * to vt_edit_t and may have some data structure for it.
   */
  if (screen->logvis) {
    (*screen->logvis->logical)(screen->logvis);
    (*screen->logvis->destroy)(screen->logvis);
  }

  vt_edit_final(&screen->normal_edit);
  vt_edit_final(&screen->alt_edit);

  if (screen->page_edits) {
    int count;

    for (count = 0; count < MAX_PAGE_ID; count++) {
      vt_edit_final(screen->page_edits + count);
    }

    free(screen->page_edits);
  }

  if (screen->status_edit) {
    vt_edit_final(screen->status_edit);
    free(screen->status_edit);
  }

  vt_log_final(&screen->logs);

  vt_screen_search_final(screen);

  free(screen);

  return 1;
}

void vt_screen_set_listener(vt_screen_t *screen, vt_screen_event_listener_t *screen_listener) {
  screen->screen_listener = screen_listener;
}

int vt_screen_resize(vt_screen_t *screen, u_int cols, u_int rows) {
  if (cols == 0) {
    cols = 1;
  }
  if (rows == 0) {
    rows = 1;
  }

  return resize(screen, cols, rows, 1);
}

/* This considers status line */
int vt_screen_cursor_row(vt_screen_t *screen) {
  int row = vt_cursor_row(screen->edit);

  if (vt_status_line_is_focused(screen)) {
    row += vt_edit_get_rows(screen->main_edit);
  }

  return row;
}

/* This considers status line */
int vt_screen_cursor_row_in_screen(vt_screen_t *screen) {
  int row = vt_screen_cursor_row(screen);

  if (screen->backscroll_rows > 0) {
    if ((row += screen->backscroll_rows) >= vt_screen_get_rows(screen)) {
      return -1;
    }
  }

  return row;
}

u_int vt_screen_get_logical_cols(vt_screen_t *screen) {
  if (screen->logvis && screen->logvis->is_visual) {
    return (*screen->logvis->logical_cols)(screen->logvis);
  } else {
    return vt_edit_get_cols(screen->edit);
  }
}

/* This considers status line */
u_int vt_screen_get_rows(vt_screen_t *screen) {
  u_int rows;

  rows = vt_edit_get_rows(screen->edit);

  if (vt_screen_has_status_line(screen)) {
    if (vt_status_line_is_focused(screen)) {
      rows += vt_edit_get_rows(screen->main_edit);
    } else {
      rows ++;
    }
  }

  return rows;
}

/* This ignores status line */
u_int vt_screen_get_logical_rows(vt_screen_t *screen) {
  if (screen->logvis && screen->logvis->is_visual) {
    return (*screen->logvis->logical_rows)(screen->logvis);
  } else {
    return vt_edit_get_rows(screen->edit);
  }
}

int vt_screen_convert_scr_row_to_abs(vt_screen_t *screen, int row) {
  return row - screen->backscroll_rows;
}

/*
 * If 0 <= row && row <= vt_screen_get_rows(), this function always returns
 * non-NULL value.
 * This considers status line.
 */
vt_line_t *vt_screen_get_line(vt_screen_t *screen, int row) {
  if (row < 0) {
    return vt_log_get(&screen->logs, ROW_IN_LOGS(screen, row));
  } else if (vt_screen_has_status_line(screen)) {
    if (row == vt_edit_get_rows(screen->main_edit)) {
      return vt_edit_get_line(screen->status_edit, 0);
    } else {
      return vt_edit_get_line(screen->main_edit, row);
    }
  } else {
    return vt_edit_get_line(screen->edit, row);
  }
}

/*
 * If 0 <= row && row <= vt_screen_get_rows(), this function always returns
 * non-NULL value.
 * This considers status line.
 */
vt_line_t *vt_screen_get_line_in_screen(vt_screen_t *screen, int row) {
  if (screen->is_backscrolling && screen->backscroll_rows > 0) {
    row -= screen->backscroll_rows;

    if (row < 0) {
      return vt_log_get(&screen->logs, ROW_IN_LOGS(screen, row));
    }
  }

  if (vt_screen_has_status_line(screen)) {
    if (row == vt_edit_get_rows(screen->main_edit)) {
      return vt_edit_get_line(screen->status_edit, 0);
    } else {
      return vt_edit_get_line(screen->main_edit, row);
    }
  } else {
    return vt_edit_get_line(screen->edit, row);
  }
}

void vt_screen_unhighlight_cursor(vt_screen_t *screen) {
  vt_line_t *line;

  if ((line = vt_screen_get_cursor_line(screen)) && !vt_line_is_empty(line)) {
    int char_index = vt_cursor_char_index(screen->edit);
    vt_line_set_modified(line, char_index, char_index);
  }
}

void vt_screen_set_modified_all(vt_screen_t *screen) {
  int row;
  vt_line_t *line;
  u_int num_rows = vt_screen_get_rows(screen);

  for (row = 0; row < num_rows; row++) {
    line = vt_screen_get_line_in_screen(screen, row); /* Always non-NULL */
    vt_line_set_modified_all(line);
  }
}

int vt_screen_add_logical_visual(vt_screen_t *screen, vt_logical_visual_t *logvis) {
  (*logvis->init)(logvis, &screen->edit->model, &screen->edit->cursor);

  if (screen->logvis) {
    if (screen->container_logvis == NULL &&
        (screen->container_logvis = vt_logvis_container_new()) == NULL) {
      return 0;
    }

    vt_logvis_container_add(screen->container_logvis, screen->logvis);
    vt_logvis_container_add(screen->container_logvis, logvis);

    screen->logvis = screen->container_logvis;
  } else {
    screen->logvis = logvis;
  }

  return 1;
}

int vt_screen_destroy_logical_visual(vt_screen_t *screen) {
  if (screen->logvis) {
    (*screen->logvis->logical)(screen->logvis);
    (*screen->logvis->destroy)(screen->logvis);
    screen->logvis = NULL;
    screen->container_logvis = NULL;

    return 1;
  } else {
    return 0;
  }
}

void vt_screen_render(vt_screen_t *screen) {
  if (screen->logvis) {
    (*screen->logvis->render)(screen->logvis);
  }
}

int vt_screen_visual(vt_screen_t *screen) {
  if (screen->logvis) {
    return (*screen->logvis->visual)(screen->logvis);
  } else {
    return 1;
  }
}

int vt_screen_logical(vt_screen_t *screen) {
  if (screen->logvis) {
    return (*screen->logvis->logical)(screen->logvis);
  } else {
    return 1;
  }
}

vt_bs_mode_t vt_screen_is_backscrolling(vt_screen_t *screen) {
  if (screen->is_backscrolling == BSM_STATIC) {
    if (screen->backscroll_rows < vt_get_log_size(&screen->logs)) {
      return BSM_STATIC;
    } else {
      return BSM_DEFAULT;
    }
  } else {
    return screen->is_backscrolling;
  }
}

void vt_set_backscroll_mode(vt_screen_t *screen, vt_bs_mode_t mode) {
  screen->backscroll_mode = mode;

  if (screen->is_backscrolling) {
    screen->is_backscrolling = mode;
  }
}

int vt_enter_backscroll_mode(vt_screen_t *screen) {
  int ret;

  screen->is_backscrolling = screen->backscroll_mode;

  if (screen->need_rewrap_logs) {
    ret = rewrap_logs(screen);
    screen->need_rewrap_logs = 0;
  } else {
    ret = 1;
  }

  return ret;
}

void vt_exit_backscroll_mode(vt_screen_t *screen) {
  screen->is_backscrolling = 0;
  screen->backscroll_rows = 0;

  vt_screen_set_modified_all(screen);
}

int vt_screen_backscroll_to(vt_screen_t *screen, int row) {
  if (!screen->is_backscrolling) {
    return 0;
  }

  if (row > 0) {
    screen->backscroll_rows = 0;
  } else {
    screen->backscroll_rows = abs(row);
  }

  vt_screen_set_modified_all(screen);

#ifdef EXIT_BS_AT_BOTTOM
  if (screen->backscroll_rows == 0) {
    vt_exit_backscroll_mode(screen);
  }
#endif

  return 1;
}

int vt_screen_backscroll_upward(vt_screen_t *screen, u_int size) {
  vt_line_t *line;
  u_int count;
  u_int num_rows;

  if (!screen->is_backscrolling) {
    return 0;
  }

  if (screen->backscroll_rows < size) {
    size = screen->backscroll_rows;
  }

  if (size == 0) {
    return 0;
  }

  screen->backscroll_rows -= size;

  num_rows = vt_screen_get_rows(screen);

  if (!screen->screen_listener || !screen->screen_listener->window_scroll_upward_region ||
      !(*screen->screen_listener->window_scroll_upward_region)(
          screen->screen_listener->self, 0, num_rows - 1, size)) {
    for (count = 0; count < num_rows - size; count++) {
      if ((line = vt_screen_get_line_in_screen(screen, count)) == NULL) {
        break;
      }

      vt_line_set_modified_all(line);
    }
  }

  for (count = num_rows - size; count < num_rows; count++) {
    if ((line = vt_screen_get_line_in_screen(screen, count)) == NULL) {
      break;
    }

    vt_line_set_modified_all(line);
  }

#ifdef EXIT_BS_AT_BOTTOM
  if (screen->backscroll_rows == 0) {
    vt_exit_backscroll_mode(screen);
  }
#endif

  return 1;
}

int vt_screen_backscroll_downward(vt_screen_t *screen, u_int size) {
  vt_line_t *line;
  u_int count;
  int num_rows;

  if (!screen->is_backscrolling) {
    return 0;
  }

  if (vt_get_num_logged_lines(&screen->logs) < screen->backscroll_rows + size) {
    size = vt_get_num_logged_lines(&screen->logs) - screen->backscroll_rows;
  }

  if (size == 0) {
    return 0;
  }

  screen->backscroll_rows += size;

  num_rows = vt_screen_get_rows(screen);

  if (!screen->screen_listener || !screen->screen_listener->window_scroll_downward_region ||
      !(*screen->screen_listener->window_scroll_downward_region)(
          screen->screen_listener->self, 0, num_rows - 1, size)) {
    for (count = size; count < num_rows; count++) {
      if ((line = vt_screen_get_line_in_screen(screen, count)) == NULL) {
        break;
      }

      vt_line_set_modified_all(line);
    }
  }

  for (count = 0; count < size; count++) {
    if ((line = vt_screen_get_line_in_screen(screen, count)) == NULL) {
      break;
    }

    vt_line_set_modified_all(line);
  }

  return 1;
}

u_int vt_screen_backscroll_upward_to_mark(vt_screen_t *screen, int *row) {
  vt_line_t *line;
  int count = 0;

  do {
    if ((line = vt_screen_get_line(screen, *row + (++count))) == NULL) {
      return 0;
    }
  } while (!line->mark);

  *row += count;
  if (screen->backscroll_rows == 0 && *row >= 0) {
    return 0;
  }

  count = *row + screen->backscroll_rows;

  if (vt_screen_backscroll_upward(screen, count)) {
    return count;
  } else {
    return 0;
  }
}

u_int vt_screen_backscroll_downward_to_mark(vt_screen_t *screen, int *row) {
  vt_line_t *line;
  int count = 0;

  do {
    if ((line = vt_screen_get_line(screen, *row - (++count))) == NULL) {
      return 0;
    }
  } while (!line->mark);

  *row -= count;
  if (screen->backscroll_rows == 0 && *row >= 0) {
    return 0;
  }

  count = -screen->backscroll_rows - *row;

  if (vt_screen_backscroll_downward(screen, count)) {
    return count;
  } else {
    return 0;
  }
}

int vt_screen_reverse_color(vt_screen_t *screen, int beg_char_index, int beg_row,
                            int end_char_index, int end_row, int is_rect) {
  if (is_rect) {
    return reverse_or_restore_color_rect(screen, beg_char_index, beg_row, end_char_index, end_row,
                                         vt_line_reverse_color);
  } else {
    return reverse_or_restore_color(screen, beg_char_index, beg_row, end_char_index, end_row,
                                    vt_line_reverse_color);
  }
}

int vt_screen_restore_color(vt_screen_t *screen, int beg_char_index, int beg_row,
                            int end_char_index, int end_row, int is_rect) {
  if (is_rect) {
    return reverse_or_restore_color_rect(screen, beg_char_index, beg_row, end_char_index, end_row,
                                         vt_line_restore_color);
  } else {
    return reverse_or_restore_color(screen, beg_char_index, beg_row, end_char_index, end_row,
                                    vt_line_restore_color);
  }
}

u_int vt_screen_copy_region(vt_screen_t *screen, vt_char_t *chars, u_int num_chars,
                            int beg_char_index, int beg_row, int end_char_index, int end_row,
                            int is_rect) {
  if (is_rect) {
    return check_or_copy_region_rect(screen, chars, num_chars, beg_char_index, beg_row,
                                     end_char_index, end_row);
  } else {
    return check_or_copy_region(screen, chars, num_chars, beg_char_index, beg_row,
                                end_char_index, end_row);
  }
}

u_int vt_screen_get_region_size(vt_screen_t *screen, int beg_char_index, int beg_row,
                                int end_char_index, int end_row, int is_rect) {
  if (is_rect) {
    return check_or_copy_region_rect(screen, NULL, 0, beg_char_index, beg_row, end_char_index,
                                     end_row);
  } else {
    return check_or_copy_region(screen, NULL, 0, beg_char_index, beg_row, end_char_index, end_row);
  }
}

int vt_screen_get_line_region(vt_screen_t *screen, int *beg_row, int *end_char_index, int *end_row,
                              int base_row) {
  int row;
  vt_line_t *line;
  vt_line_t *next_line;

  /*
   * finding the end of line.
   */
  row = base_row;

  if ((line = vt_screen_get_line(screen, row)) == NULL || vt_line_is_empty(line)) {
    return 0;
  }

  while (1) {
    if (!vt_line_is_continued_to_next(line)) {
      break;
    }

    if ((next_line = vt_screen_get_line(screen, row + 1)) == NULL || vt_line_is_empty(next_line)) {
      break;
    }

    line = next_line;
    row++;
  }

  *end_char_index = line->num_filled_chars - 1;
  *end_row = row;

  /*
   * finding the beginning of line.
   */
  row = base_row;

  while (1) {
    if ((line = vt_screen_get_line(screen, row - 1)) == NULL || vt_line_is_empty(line) ||
        !vt_line_is_continued_to_next(line)) {
      break;
    }

    row--;
  }

  *beg_row = row;

  return 1;
}

int vt_screen_get_word_region(vt_screen_t *screen, int *beg_char_index, int *beg_row,
                              int *end_char_index, int *end_row, int base_char_index,
                              int base_row) {
  int row;
  int char_index;
  vt_line_t *line;
  vt_line_t *base_line;
  vt_char_t *ch;
  int flag;

  if ((base_line = vt_screen_get_line(screen, base_row)) == NULL || vt_line_is_empty(base_line)) {
    return 0;
  }

  if (regard_uri_as_word) {
    char *orig;
    u_int len;
    vt_char_t *str;

    orig = word_separators;
    word_separators = "\" '`<>|";
    regard_uri_as_word = 0;

    /* Not return 0 */
    vt_screen_get_word_region(screen, beg_char_index, beg_row, end_char_index, end_row,
                              base_char_index, base_row);

    word_separators = orig;
    regard_uri_as_word = 1;

    if ((len = check_or_copy_region(screen, NULL, 0, *beg_char_index, *beg_row, *end_char_index,
                                    *end_row)) > 0 &&
        (str = vt_str_alloca(len))) {
      u_int count;
      int slash_num;

      vt_str_init(str, len);
      check_or_copy_region(screen, str, len, *beg_char_index, *beg_row, *end_char_index, *end_row);

      for (slash_num = 0, count = 0; count < len; count++) {
        if (vt_char_cs(str + count) == US_ASCII) {
          switch (vt_char_code(str + count)) {
            case '/':
              if (++slash_num == 1) {
                continue;
              }

            /* Fall through */

            case '@':
              vt_str_final(str, len);

              return 1;
          }
        }
      }

      vt_str_final(str, len);
    }
  }

  if (is_word_separator(vt_char_at(base_line, base_char_index))) {
    *beg_char_index = base_char_index;
    *end_char_index = base_char_index;
    *beg_row = base_row;
    *end_row = base_row;

    return 1;
  }

  flag = vt_char_is_fullwidth(vt_char_at(base_line, base_char_index));

  /*
   * search the beg of word
   */
  row = base_row;
  char_index = base_char_index;
  line = base_line;

  while (1) {
    if (char_index == 0) {
      if ((line = vt_screen_get_line(screen, row - 1)) == NULL || vt_line_is_empty(line) ||
          !vt_line_is_continued_to_next(line)) {
        *beg_char_index = char_index;

        break;
      }

      row--;
      char_index = line->num_filled_chars - 1;
    } else {
      char_index--;
    }

    ch = vt_char_at(line, char_index);

    if (is_word_separator(ch) || flag != vt_char_is_fullwidth(ch)) {
      *beg_char_index = char_index + 1;

      break;
    }
  }

  *beg_row = row;

  /*
   * search the end of word.
   */
  row = base_row;
  char_index = base_char_index;
  line = base_line;

  while (1) {
    if (char_index == line->num_filled_chars - 1) {
      if (!vt_line_is_continued_to_next(line) ||
          (line = vt_screen_get_line(screen, row + 1)) == NULL || vt_line_is_empty(line)) {
        *end_char_index = char_index;

        break;
      }

      row++;
      char_index = 0;
    } else {
      char_index++;
    }

    ch = vt_char_at(line, char_index);

    if (is_word_separator(ch) || flag != vt_char_is_fullwidth(ch)) {
      *end_char_index = char_index - 1;

      break;
    }
  }

  *end_row = row;

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " selected word region: %d %d => %d %d %d %d\n", base_char_index,
                  base_row, *beg_char_index, *beg_row, *end_char_index, *end_row);
#endif

  return 1;
}

int vt_screen_search_init(vt_screen_t *screen,
                          int char_index, int row, /* -1: cursor position */
                          int (*match)(size_t *, size_t *, void *, u_char *, int)) {
  if (screen->search) {
    if (screen->search->match == match) {
      return 1;
    } else {
      return 0;
    }
  }

  if (!(screen->search = malloc(sizeof(*screen->search)))) {
    return 0;
  }

  screen->search->match = match;

  screen->search->char_index = char_index;
  screen->search->row = row;

  return 1;
}

void vt_screen_search_final(vt_screen_t *screen) {
  free(screen->search);
  screen->search = NULL;
}

int vt_screen_search_reset_position(vt_screen_t *screen) {
  if (!screen->search) {
    return 0;
  }

  /* char_index == -1 has special meaning. */
  screen->search->char_index = -1;
  screen->search->row = -1;

  return 1;
}

/*
 * It is assumed that this function is called in *visual* context.
 *
 * XXX
 * It is not supported to match text in multiple lines.
 */
int vt_screen_search_find(vt_screen_t *screen,
                          int *beg_char_index, /* visual position is returned */
                          int *beg_row,        /* visual position is returned */
                          int *end_char_index, /* visual position is returned */
                          int *end_row,        /* visual position is returned */
                          void *regex, int backward) {
  vt_char_t *line_str;
  u_int cols;
  ef_parser_t *parser;
  ef_conv_t *conv;
  u_char *buf;
  size_t buf_len;
  vt_line_t *line;
  int step;
  int res;

  if (!screen->search) {
    return 0;
  }

  cols = vt_screen_get_logical_cols(screen);
  if (!(line_str = vt_str_alloca(cols))) {
    return 0;
  }
  vt_str_init(line_str, cols);

  buf_len = vt_screen_get_logical_cols(screen) * VTCHAR_UTF_MAX_SIZE + 1;
  if (!(buf = alloca(buf_len))) {
    return 0;
  }

  if (!(parser = vt_str_parser_new())) {
    return 0;
  }

  if (!(conv = vt_char_encoding_conv_new(VT_UTF8))) {
    (*parser->destroy)(parser);

    return 0;
  }

  /* char_index == -1 has special meaning. */
  if (screen->search->char_index == -1) {
    screen->search->char_index = vt_screen_cursor_char_index(screen);
    screen->search->row = vt_screen_cursor_row(screen);
  }

  *beg_char_index = screen->search->char_index;
  *beg_row = screen->search->row;
  step = (backward ? -1 : 1);

#ifdef DEBUG
  bl_debug_printf(BL_DEBUG_TAG " Search start from %d %d\n", *beg_char_index, *beg_row);
#endif

  res = 0;

  for (; (line = vt_screen_get_line(screen, *beg_row)); (*beg_row) += step) {
    size_t line_len;
    size_t match_beg;
    size_t match_len;

    if ((line_len = vt_line_get_num_filled_chars_except_sp(line)) == 0) {
      continue;
    }

    /*
     * Visual => Logical
     */
    vt_line_copy_logical_str(line, line_str, 0, line_len);

    (*parser->init)(parser);
    if (backward) {
      vt_str_parser_set_str(parser, line_str, (*beg_row != screen->search->row)
                                                  ? line_len
                                                  : BL_MIN(*beg_char_index + 1, line_len));
      *beg_char_index = 0;
    } else {
      if (*beg_row != screen->search->row) {
        *beg_char_index = 0;
      } else if (line_len <= *beg_char_index) {
        continue;
      }

      vt_str_parser_set_str(parser, line_str + (*beg_char_index), line_len - *beg_char_index);
    }

    (*conv->init)(conv);
    *(buf + (*conv->convert)(conv, buf, buf_len - 1, parser)) = '\0';

    if ((*screen->search->match)(&match_beg, &match_len, regex, buf, backward)) {
      size_t count;
      u_int comb_size;
      int beg;
      int end;
      vt_char_t *comb;

      comb = vt_get_combining_chars(line_str + (*beg_char_index), &comb_size);

      for (count = 0; count < match_beg; count++) {
        /* Ignore 2nd and following bytes. */
        if ((buf[count] & 0xc0) != 0x80) {
          while (comb_size > 0 && vt_char_cs(comb) == PICTURE_CHARSET) {
            comb_size--;
            comb++;
          }

          if (comb_size > 0) {
            comb_size--;
            comb++;
          } else if ((++(*beg_char_index)) >= line_len - 1) {
            break;
          } else {
            comb = vt_get_combining_chars(line_str + (*beg_char_index), &comb_size);
          }
        }
      }

      *end_char_index = (*beg_char_index) - 1;
      for (; count < match_beg + match_len; count++) {
        /* Ignore 2nd and following bytes. */
        if ((buf[count] & 0xc0) != 0x80) {
          while (comb_size > 0 && vt_char_cs(comb) == PICTURE_CHARSET) {
            comb_size--;
            comb++;
          }

          if (comb_size > 0) {
            comb_size--;
            comb++;
          } else if ((++(*end_char_index)) >= line_len - 1) {
            break;
          } else {
            comb = vt_get_combining_chars(line_str + (*end_char_index), &comb_size);
          }
        }
      }

      if (*end_char_index < *beg_char_index) {
        continue;
      }

      *end_row = *beg_row;

      if (backward) {
        if (*beg_char_index > 0) {
          screen->search->char_index = *beg_char_index - 1;
          screen->search->row = *beg_row;
        } else {
          screen->search->char_index = vt_screen_get_logical_cols(screen) - 1;
          screen->search->row = *beg_row - 1;
        }
      } else {
        screen->search->char_index = *beg_char_index + 1;
        screen->search->row = *beg_row;
      }

#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " Search position x %d y %d\n", screen->search->char_index,
                      screen->search->row);
#endif

      /*
       * Logical => Visual
       *
       * XXX Incomplete
       * Current implementation have problems like this.
       *  (logical)RRRLLLNNN => (visual)NNNLLLRRR
       *  Searching LLLNNN =>           ^^^^^^ hits but only NNNL is reversed.
       */
      beg = vt_line_convert_logical_char_index_to_visual(line, *beg_char_index, NULL);
      end = vt_line_convert_logical_char_index_to_visual(line, *end_char_index, NULL);

      if (beg > end) {
        *beg_char_index = end;
        *end_char_index = beg;
      } else {
        *beg_char_index = beg;
        *end_char_index = end;
      }

      if (vt_line_is_rtl(line)) {
        int char_index;

        /* XXX for x_selection */
        char_index = -(*beg_char_index);
        *beg_char_index = -(*end_char_index);
        *end_char_index = char_index;
      }

      res = 1;

      break;
    }
  }

  (*parser->destroy)(parser);
  (*conv->destroy)(conv);
  vt_str_final(line_str, vt_screen_get_logical_cols(screen));

  return res;
}

int vt_screen_blink(vt_screen_t *screen) {
  int has_blinking_char;

  has_blinking_char = 0;

  if (screen->has_blinking_char) {
    int char_index;
    int row;
    vt_line_t *line;
    u_int num_rows = vt_screen_get_rows(screen);

    /* '+ screen->backscroll_rows' prohibit backlog lines to blink. */
    for (row = 0; row + screen->backscroll_rows < num_rows; row++) {
      line = vt_screen_get_line(screen, row); /* Always non-NULL */

      for (char_index = 0; char_index < line->num_filled_chars; char_index++) {
        if (vt_char_is_blinking(line->chars + char_index)) {
          vt_line_set_modified(line, char_index, char_index);
          has_blinking_char = 1;
        }
      }
    }

    if (screen->backscroll_rows == 0) {
      screen->has_blinking_char = has_blinking_char;
    }
  }

  return has_blinking_char;
}

/*
 * VT100 commands
 *
 * !! Notice !!
 * These functions are called under logical order mode.
 */

vt_char_t *vt_screen_get_n_prev_char(vt_screen_t *screen, int n) {
  int char_index;
  int row;
  vt_line_t *line;

  if (!get_n_prev_char_pos(screen, &char_index, &row, 1)) {
    return NULL;
  }

  if ((line = vt_edit_get_line(screen->edit, row)) == NULL) {
    return NULL;
  }

  return vt_char_at(line, char_index);
}

int vt_screen_combine_with_prev_char(vt_screen_t *screen, u_int32_t code, ef_charset_t cs,
                                     int is_fullwidth, int is_awidth, int is_comb,
                                     vt_color_t fg_color, vt_color_t bg_color, int is_bold,
                                     int is_italic, int line_style, int is_blinking,
                                     int is_protected) {
  int char_index;
  int row;
  vt_char_t *ch;
  vt_line_t *line;

  if (!get_n_prev_char_pos(screen, &char_index, &row, 1)) {
    return 0;
  }

  if ((line = vt_edit_get_line(screen->edit, row)) == NULL) {
    return 0;
  }

  if ((ch = vt_char_at(line, char_index)) == NULL) {
    return 0;
  }

  if (!vt_char_combine(ch, code, cs, is_fullwidth, is_awidth, is_comb, fg_color, bg_color, is_bold,
                       is_italic, line_style, is_blinking, is_protected)) {
    return 0;
  }

  vt_line_set_modified(line, char_index, char_index);

  return 1;
}

int vt_screen_insert_chars(vt_screen_t *screen, vt_char_t *chars, u_int len) {
  return vt_edit_insert_chars(screen->edit, chars, len);
}

int vt_screen_insert_new_lines(vt_screen_t *screen, u_int size) {
  u_int count;

  if (size > vt_edit_get_rows(screen->edit)) {
    size = vt_edit_get_rows(screen->edit);
  }

  for (count = 0; count < size; count++) {
    vt_edit_insert_new_line(screen->edit);
  }

  return 1;
}

int vt_screen_overwrite_chars(vt_screen_t *screen, vt_char_t *chars, u_int len) {
  return vt_edit_overwrite_chars(screen->edit, chars, len);
}

int vt_screen_delete_lines(vt_screen_t *screen, u_int size) {
  u_int count;

  if (size > vt_edit_get_rows(screen->edit)) {
    size = vt_edit_get_rows(screen->edit);
  }

  for (count = 0; count < size; count++) {
    if (!vt_edit_delete_line(screen->edit)) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " deleting nonexisting lines.\n");
#endif

      return 0;
    }
  }

  return 1;
}

int vt_screen_go_forward(vt_screen_t *screen, u_int size, int scroll) {
  u_int count;

  for (count = 0; count < size; count++) {
    if (!vt_edit_go_forward(screen->edit, 0)) {
      if (scroll) {
        if (size > vt_edit_get_cols(screen->edit)) {
          size = vt_edit_get_cols(screen->edit);
          if (size <= count) {
            break;
          }
        }

        vt_edit_scroll_leftward(screen->edit, size - count);

        break;
      }
#ifdef DEBUG
      else {
        bl_warn_printf(BL_DEBUG_TAG " cursor cannot go forward any more.\n");
      }
#endif

      return 0;
    }
  }

  return 1;
}

int vt_screen_go_back(vt_screen_t *screen, u_int size, int scroll) {
  u_int count;

  for (count = 0; count < size; count++) {
    if (!vt_edit_go_back(screen->edit, 0)) {
      if (scroll) {
        if (size > vt_edit_get_cols(screen->edit)) {
          size = vt_edit_get_cols(screen->edit);
          if (size <= count) {
            break;
          }
        }

        vt_edit_scroll_rightward(screen->edit, size - count);

        break;
      }
#ifdef DEBUG
      else {
        bl_warn_printf(BL_DEBUG_TAG " cursor cannot go back any more.\n");
      }
#endif

      return 0;
    }
  }

  return 1;
}

int vt_screen_go_upward(vt_screen_t *screen, u_int size) {
  u_int count;

  for (count = 0; count < size; count++) {
    if (!vt_edit_go_upward(screen->edit, 0)) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " cursor cannot go upward any more.\n");
#endif

      return 0;
    }
  }

  return 1;
}

int vt_screen_go_downward(vt_screen_t *screen, u_int size) {
  u_int count;

  for (count = 0; count < size; count++) {
    if (!vt_edit_go_downward(screen->edit, 0)) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " cursor cannot go downward any more.\n");
#endif

      return 0;
    }
  }

  return 1;
}

int vt_screen_use_normal_edit(vt_screen_t *screen) {
  if (screen->edit != &screen->normal_edit) {
    int col = screen->edit->cursor.col;
    int row = screen->edit->cursor.row;

    change_edit(screen, &screen->normal_edit);
    vt_edit_goto(screen->edit, col, row);

    return 1;
  } else {
    return 0;
  }
}

int vt_screen_use_alternative_edit(vt_screen_t *screen) {
  if (screen->edit != &screen->alt_edit) {
    int col = screen->edit->cursor.col;
    int row = screen->edit->cursor.row;

    change_edit(screen, &screen->alt_edit);
    vt_edit_goto(screen->edit, col, row);

    return 1;
  } else {
    return 0;
  }
}

/*
 * This function must be called in logical context in order to swap
 * stored_edit->edit and screen->edit in the same conditions.
 */
int vt_screen_enable_local_echo(vt_screen_t *screen) {
  if (!screen->stored_edit) {
    if (!(screen->stored_edit = malloc(sizeof(vt_stored_edit_t)))) {
      return 0;
    }

    screen->stored_edit->edit = *screen->edit;

    /*
     * New data is allocated in screen->edit, not in stored_edit->edit
     * because screen->edit allocated here will be deleted and
     * stored_edit->edit will be revived in vt_screen_disable_local_echo().
     */
    if (!vt_edit_clone(screen->edit, &screen->stored_edit->edit)) {
      free(screen->stored_edit);
      screen->stored_edit = NULL;

      return 0;
    }

    screen->edit->is_logging = 0;
  }

  screen->stored_edit->time = get_msec_time();

  return 1;
}

int vt_screen_local_echo_wait(vt_screen_t *screen, int msec /* 0: stored_edit->time = 0 (>=
                                                               get_msec_time() is always false.) */
                              ) {
  if (screen->stored_edit) {
    if (msec == 0) {
      screen->stored_edit->time = 0;
    } else if (screen->stored_edit->time + msec >= get_msec_time()) {
      return 1;
    }
  }

  return 0;
}

/*
 * This function must be called in logical context in order to swap
 * stored_edit->edit and screen->edit in the same conditions.
 */
int vt_screen_disable_local_echo(vt_screen_t *screen) {
  u_int row;
  u_int num_rows;
  vt_line_t *old_line;
  vt_line_t *new_line;

  if (!screen->stored_edit) {
    return 0;
  }

  num_rows = vt_edit_get_rows(screen->edit);

  /*
   * Set modified flag to the difference between the current edit and the stored
   * one
   * to restore the screen before starting local echo.
   */
  for (row = 0; row < num_rows; row++) {
    if ((new_line = vt_edit_get_line(&screen->stored_edit->edit, row)) &&
        /* old_line is always non-NULL */
        (vt_line_is_modified((old_line = vt_edit_get_line(screen->edit, row))) ||
         old_line->num_filled_chars != new_line->num_filled_chars
#if 1
         || !vt_str_bytes_equal(old_line->chars, new_line->chars, new_line->num_filled_chars)
#endif
         )) {
      vt_line_set_modified_all(new_line);
    }
  }

  vt_edit_final(screen->edit);
  *screen->edit = screen->stored_edit->edit;

  free(screen->stored_edit);
  screen->stored_edit = NULL;

  return 1;
}

void vt_screen_copy_area(vt_screen_t *screen, int src_col, int src_row, u_int num_copy_cols,
                         u_int num_copy_rows, u_int src_page,
                         int dst_col, int dst_row, u_int dst_page) {
  vt_edit_t *src_edit;
  vt_edit_t *dst_edit;

  if (src_page > MAX_PAGE_ID) {
    src_page = MAX_PAGE_ID;
  }
  if (dst_page > MAX_PAGE_ID) {
    dst_page = MAX_PAGE_ID;
  }

  if ((src_edit = get_edit(screen, src_page)) && (dst_edit = get_edit(screen, dst_page))) {
    vt_edit_copy_area(src_edit, src_col, src_row, num_copy_cols, num_copy_rows,
                      dst_edit, dst_col, dst_row);
  }
}

u_int16_t vt_screen_get_checksum(vt_screen_t *screen, int col, int row,
                             u_int num_cols, u_int num_rows, int page) {
  vt_edit_t *edit;

  if (page > MAX_PAGE_ID) {
    page = MAX_PAGE_ID;
  }

  if ((edit = get_edit(screen, page))) {
    return vt_edit_get_checksum(edit, col, row, num_cols, num_rows);
  }

  return 0;
}

void vt_screen_enable_blinking(vt_screen_t *screen) { screen->has_blinking_char = 1; }

int vt_screen_write_content(vt_screen_t *screen, int fd, ef_conv_t *conv, int clear_at_end,
                            vt_write_content_area_t area) {
  int beg;
  int end;
  vt_char_t *buf;
  u_int buf_len;
  u_int num;
  u_char conv_buf[512];
  ef_parser_t *vt_str_parser;

#if 1
  /* for sig_error in vt_term_manager.c */
  vt_screen_logical(screen);
#endif

  if (area == WCA_CURSOR_LINE) {
    beg = vt_screen_cursor_row(screen);
    end = beg + 1;
  } else {
    end = vt_screen_get_rows(screen);
    if (area == WCA_ALL) {
      beg = -vt_screen_get_num_logged_lines(screen);
    } else /* if (area == WCA_SCREEN) */ {
      beg = 0;
    }
  }

  /* WCA_CURSOR_LINE may cause num == 0. */
  if ((buf_len = vt_screen_get_region_size(screen, 0, beg, 0, end, 0)) == 0) {
    return 0;
  }

  /* buf_len could become huge size which gets alloca() to fail. */
  if ((buf = vt_str_new(buf_len)) == NULL) {
    return 0;
  }

  vt_screen_copy_region(screen, buf, buf_len, 0, beg, 0, end, 0);

  if (!(vt_str_parser = vt_str_parser_new())) {
    vt_str_destroy(buf, buf_len);

    return 0;
  }

  (*vt_str_parser->init)(vt_str_parser);
  vt_str_parser_set_str(vt_str_parser, buf, buf_len);
  (*conv->init)(conv);

  while (!vt_str_parser->is_eos &&
         (num = (*conv->convert)(conv, conv_buf, sizeof(conv_buf), vt_str_parser)) > 0) {
    write(fd, conv_buf, num);
  }

  if (clear_at_end) {
    write(fd, "\n\x1b[1000S\x1b[H", 11);
  }

  (*vt_str_parser->destroy)(vt_str_parser);
  vt_str_destroy(buf, buf_len);

  return 1;
}

int vt_screen_get_page_id(vt_screen_t *screen) {
  if (screen->edit == &screen->normal_edit ||
      screen->edit == &screen->alt_edit /* alt edit is regarded as 0 */) {
    return 0;
  } else if (screen->page_edits) {
    return screen->edit - screen->page_edits + 1;
  }

  return -1;
}

int vt_screen_goto_page(vt_screen_t *screen, u_int page_id) {
  vt_edit_t *edit;

  if (page_id > MAX_PAGE_ID) {
    page_id = MAX_PAGE_ID;
  }

  if ((edit = get_edit(screen, page_id))) {
    vt_edit_goto(edit, screen->edit->cursor.col, screen->edit->cursor.row);
    if (vt_edit_get_tab_size(edit) != vt_edit_get_tab_size(screen->edit)) {
      vt_edit_set_tab_size(edit, vt_edit_get_tab_size(screen->edit));
    }
    edit->vmargin_beg = screen->edit->vmargin_beg;
    edit->vmargin_end = screen->edit->vmargin_end;
    edit->hmargin_beg = screen->edit->hmargin_beg;
    edit->hmargin_end = screen->edit->hmargin_end;
    edit->use_margin = screen->edit->use_margin;
    edit->is_logging = screen->edit->is_logging;
    edit->is_relative_origin = screen->edit->is_relative_origin;
    edit->is_auto_wrap = screen->edit->is_auto_wrap;
    edit->use_bce = screen->edit->use_bce;
    edit->use_rect_attr_select = screen->edit->use_rect_attr_select;

    change_edit(screen, edit);

    return 1;
  }

  return 0;
}

int vt_screen_goto_next_page(vt_screen_t *screen, u_int offset) {
  int page_id = vt_screen_get_page_id(screen);

  if (page_id != -1) {
    return vt_screen_goto_page(screen, page_id + offset);
  }

  return 0;
}

int vt_screen_goto_prev_page(vt_screen_t *screen, u_int offset) {
  int page_id = vt_screen_get_page_id(screen);

  if (page_id != -1) {
    if (page_id < offset) {
      page_id = offset;
    }

    return vt_screen_goto_page(screen, page_id - offset);
  }

  return 0;
}

void vt_screen_set_use_status_line(vt_screen_t *screen, int use) {
  if (use) {
    if (!vt_screen_has_status_line(screen)) {
      if ((!screen->status_edit && !(screen->status_edit = status_edit_new(screen->edit))) ||
          /* XXX status line is *not* supported on vertical mode */
          (screen->logvis && !screen->logvis->is_reversible)) {
        return;
      }

      screen->main_edit = screen->edit;
      screen->has_status_line = 1;
      vt_edit_set_modified_all(screen->status_edit);
      /*
       * XXX
       * I don't know whether the cursor goes to home position or not,
       * but vttest 11.2.6.2.1 seems to assume it.
       * (https://vt100.net/docs/vt510-rm/DECSSDT.html doesn't describe it.)
       */
#if 1
      vt_edit_goto(screen->status_edit, 0, 0);
#endif

      resize(screen, vt_edit_get_cols(screen->main_edit),
             vt_edit_get_rows(screen->main_edit), 0);
    }
  } else {
    if (vt_screen_has_status_line(screen)) {
      screen->has_status_line = 0;

      resize(screen, vt_edit_get_cols(screen->main_edit),
             vt_edit_get_rows(screen->main_edit) + 1, 0);
      screen->main_edit = NULL;
    }
  }
}

void vt_focus_status_line(vt_screen_t *screen) {
  if (!screen->status_edit && !(screen->status_edit = status_edit_new(screen->edit))) {
    return;
  }

  if (!vt_status_line_is_focused(screen)) {
    change_edit(screen, screen->status_edit);
  }
}

void vt_focus_main_screen(vt_screen_t *screen) {
  if (screen->main_edit && vt_status_line_is_focused(screen)) {
    change_edit(screen, screen->main_edit);
  }
}
