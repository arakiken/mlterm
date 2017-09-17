/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "vt_logical_visual.h"

#include <pobl/bl_mem.h>   /* realloc/free */
#include <pobl/bl_debug.h> /* bl_msg_printf */
#include <pobl/bl_str.h>   /* strcmp */
#include "vt_ctl_loader.h"
#include "vt_shape.h" /* vt_is_arabic_combining */

#define CURSOR_LINE(logvis) (vt_model_get_line((logvis)->model, (logvis)->cursor->row))

#define MSB32 0x80000000

#if 0
#define __DEBUG
#endif

#if 0
#define CURSOR_DEBUG
#endif

typedef struct container_logical_visual {
  vt_logical_visual_t logvis;

  /*
   * visual : children[0] => children[1] => ... => children[n]
   * logical: children[n] => ... => children[1] => children[0]
   */
  vt_logical_visual_t **children;
  u_int num_of_children;

} container_logical_visual_t;

typedef struct comb_logical_visual {
  vt_logical_visual_t logvis;

  int cursor_logical_char_index;
  int cursor_logical_col;

} comb_logical_visual_t;

typedef struct vert_logical_visual {
  vt_logical_visual_t logvis;

  vt_model_t logical_model;
  vt_model_t visual_model;

  int cursor_logical_char_index;
  int cursor_logical_col;
  int cursor_logical_row;

  int8_t is_init;

} vert_logical_visual_t;

typedef struct ctl_logical_visual {
  vt_logical_visual_t logvis;

  int cursor_logical_char_index;
  int cursor_logical_col;
  u_int32_t cursor_meet_pos_info;
  vt_bidi_mode_t bidi_mode;
  const char *separators;
  void *term;

} ctl_logical_visual_t;

/* --- static variables --- */

/* Order of this table must be same as x_vertical_mode_t. */
static char *vertical_mode_name_table[] = {
    "none", "mongol", "cjk",
};

/* --- static functions --- */

static int container_delete(vt_logical_visual_t *logvis) {
  container_logical_visual_t *container;
  int count;

  container = (container_logical_visual_t*)logvis;

  if (container->num_of_children) {
    for (count = container->num_of_children - 1; count >= 0; count--) {
      (*container->children[count]->delete)(container->children[count]);
    }
  }

  free(container->children);

  free(logvis);

  return 1;
}

static int container_init(vt_logical_visual_t *logvis, vt_model_t *model, vt_cursor_t *cursor) {
  container_logical_visual_t *container;
  u_int count;

  logvis->model = model;
  logvis->cursor = cursor;

  container = (container_logical_visual_t*)logvis;

  for (count = 0; count < container->num_of_children; count++) {
    (*container->children[count]->init)(container->children[count], model, cursor);
  }

  return 1;
}

static u_int container_logical_cols(vt_logical_visual_t *logvis) {
  container_logical_visual_t *container;

  container = (container_logical_visual_t*)logvis;

  if (container->num_of_children > 0) {
    return (*container->children[container->num_of_children - 1]->logical_cols)(
        container->children[container->num_of_children - 1]);
  } else {
    return logvis->model->num_of_cols;
  }
}

static u_int container_logical_rows(vt_logical_visual_t *logvis) {
  container_logical_visual_t *container;

  container = (container_logical_visual_t*)logvis;

  if (container->num_of_children > 0) {
    return (*container->children[container->num_of_children - 1]->logical_rows)(
        container->children[container->num_of_children - 1]);
  } else {
    return logvis->model->num_of_rows;
  }
}

static int container_render(vt_logical_visual_t *logvis) {
  container_logical_visual_t *container;
  u_int count;

  container = (container_logical_visual_t*)logvis;

  /*
   * XXX
   * only the first children can render correctly.
   */
  for (count = 0; count < container->num_of_children; count++) {
    (*container->children[count]->render)(container->children[count]);
  }

  return 1;
}

static int container_visual(vt_logical_visual_t *logvis) {
  container_logical_visual_t *container;
  u_int count;

  if (logvis->is_visual) {
    return 0;
  }

  container = (container_logical_visual_t*)logvis;

  for (count = 0; count < container->num_of_children; count++) {
    (*container->children[count]->visual)(container->children[count]);
  }

  logvis->is_visual = 1;

  return 1;
}

static int container_logical(vt_logical_visual_t *logvis) {
  container_logical_visual_t *container;
  int count;

  if (!logvis->is_visual) {
    return 0;
  }

  container = (container_logical_visual_t*)logvis;

  if (container->num_of_children == 0) {
    return 1;
  }

  for (count = container->num_of_children - 1; count >= 0; count--) {
    (*container->children[count]->logical)(container->children[count]);
  }

  logvis->is_visual = 0;

  return 1;
}

static int container_visual_line(vt_logical_visual_t *logvis, vt_line_t *line) {
  container_logical_visual_t *container;
  u_int count;

  container = (container_logical_visual_t*)logvis;

  for (count = 0; count < container->num_of_children; count++) {
    (*container->children[count]->visual_line)(container->children[count], line);
  }

  return 1;
}

/*
 * dynamic combining
 */

static int comb_delete(vt_logical_visual_t *logvis) {
  free(logvis);

  return 1;
}

static int comb_init(vt_logical_visual_t *logvis, vt_model_t *model, vt_cursor_t *cursor) {
  logvis->model = model;
  logvis->cursor = cursor;

  return 1;
}

static u_int comb_logical_cols(vt_logical_visual_t *logvis) { return logvis->model->num_of_cols; }

static u_int comb_logical_rows(vt_logical_visual_t *logvis) { return logvis->model->num_of_rows; }

static int comb_render(vt_logical_visual_t *logvis) { return (*logvis->visual)(logvis); }

static int comb_visual(vt_logical_visual_t *logvis) {
  int row;

  if (logvis->is_visual) {
    return 0;
  }

  ((comb_logical_visual_t*)logvis)->cursor_logical_char_index = logvis->cursor->char_index;
  ((comb_logical_visual_t*)logvis)->cursor_logical_col = logvis->cursor->col;

  for (row = 0; row < logvis->model->num_of_rows; row++) {
    vt_line_t *line;
    int dst_pos;
    int src_pos;
    vt_char_t *cur;

    line = vt_model_get_line(logvis->model, row);

    dst_pos = 0;
    cur = line->chars;
    for (src_pos = 0; src_pos < line->num_of_filled_chars; src_pos++) {
      if (dst_pos > 0 &&
          (vt_char_is_comb(cur) ||
           vt_is_arabic_combining(dst_pos >= 2 ? vt_char_at(line, dst_pos - 2) : NULL,
                                  vt_char_at(line, dst_pos - 1), cur))) {
        vt_char_combine_simple(vt_char_at(line, dst_pos - 1), cur);

#if 0
        /*
         * This doesn't work as expected, for example, when
         * one of combined two characters are deleted.
         */
        if (vt_line_is_modified(line)) {
          int beg;
          int end;

          beg = vt_line_get_beg_of_modified(line);
          end = vt_line_get_end_of_modified(line);

          if (beg > dst_pos - 1) {
            beg--;
          }

          if (end > dst_pos - 1) {
            end--;
          }

          vt_line_set_updated(line);
          vt_line_set_modified(line, beg, end);
        }
#endif
      } else {
        vt_char_copy(vt_char_at(line, dst_pos++), cur);
      }

      if (row == logvis->cursor->row && src_pos == logvis->cursor->char_index) {
        logvis->cursor->char_index = dst_pos - 1;
        logvis->cursor->col =
            vt_convert_char_index_to_col(CURSOR_LINE(logvis), logvis->cursor->char_index, 0) +
            logvis->cursor->col_in_char;
      }

      cur++;
    }

#if 1
    if (vt_line_is_modified(line)) {
      /*
       * (Logical)    AbcdEfgHij  (bcdfg are combining characters)
       * => (Visual)  AEH
       * => (Logical) AbcEfgHij
       *                 ^^^^^^^ (^ means redrawn characters)
       * => (Visual)  AE
       *                 ^^^^^^^
       *              ^^^^^^^^^^ <= vt_line_set_modified( line , 0 , ...)
       * => (Logical) AkcEfgHij
       *               ^
       * => (Visual)  AEH
       *               ^
       *              ^^ <= vt_line_set_modified( line , 0 , ...)
       */
      vt_line_set_modified(line, 0, vt_line_get_end_of_modified(line));
    }
#endif

    line->num_of_filled_chars = dst_pos;
  }

  logvis->is_visual = 1;

  return 1;
}

static int comb_logical(vt_logical_visual_t *logvis) {
  vt_char_t *buf;
  int row;

  if (!logvis->is_visual) {
    return 0;
  }

  if ((buf = vt_str_alloca(logvis->model->num_of_cols)) == NULL) {
    return 0;
  }

  for (row = 0; row < logvis->model->num_of_rows; row++) {
    vt_line_t *line;
    int src_pos;
    u_int src_len;
    vt_char_t *c;

    line = vt_model_get_line(logvis->model, row);

    vt_str_copy(buf, line->chars, line->num_of_filled_chars);

    src_len = line->num_of_filled_chars;
    line->num_of_filled_chars = 0;
    c = buf;
    for (src_pos = 0; src_pos < src_len && line->num_of_filled_chars < line->num_of_chars;
         src_pos++) {
      vt_char_t *comb;
      u_int size;

      if ((comb = vt_get_combining_chars(c, &size))
#if 1
          /* XXX Hack for inline pictures (see x_picture.c) */
          &&
          vt_char_cs(comb) != PICTURE_CHARSET
#endif
          ) {
        int count;

        vt_char_copy(vt_char_at(line, line->num_of_filled_chars++), vt_get_base_char(c));

        for (count = 0; count < size; count++) {
          if (line->num_of_filled_chars >= line->num_of_chars) {
            break;
          }

#if 0
          /*
           * This doesn't work as expected, for example, when
           * one of combined two characters are deleted.
           */
          if (vt_line_is_modified(line)) {
            int beg;
            int end;
            int is_cleared_to_end;

            beg = vt_line_get_beg_of_modified(line);
            end = vt_line_get_end_of_modified(line);

            if (beg > src_pos) {
              beg++;
            }

            if (end > src_pos) {
              end++;
            }

            vt_line_set_updated(line);
            vt_line_set_modified(line, beg, end);
          }
#endif

          vt_char_copy(vt_char_at(line, line->num_of_filled_chars++), comb);

          comb++;
        }
      } else {
        vt_char_copy(vt_char_at(line, line->num_of_filled_chars++), c);
      }

      c++;
    }
  }

  vt_str_final(buf, logvis->model->num_of_cols);

  logvis->cursor->char_index = ((comb_logical_visual_t*)logvis)->cursor_logical_char_index;
  logvis->cursor->col = ((comb_logical_visual_t*)logvis)->cursor_logical_col;

  logvis->is_visual = 0;

  return 1;
}

static int comb_visual_line(vt_logical_visual_t *logvis, vt_line_t *line) {
  int dst_pos;
  int src_pos;
  vt_char_t *cur;

  dst_pos = 0;
  cur = line->chars;
  for (src_pos = 0; src_pos < line->num_of_filled_chars; src_pos++) {
    if (dst_pos > 0 && (vt_char_is_comb(cur) ||
                        vt_is_arabic_combining(dst_pos >= 2 ? vt_char_at(line, dst_pos - 2) : NULL,
                                               vt_char_at(line, dst_pos - 1), cur))) {
      vt_char_combine_simple(vt_char_at(line, dst_pos - 1), cur);
    } else {
      vt_char_copy(vt_char_at(line, dst_pos++), cur);
    }

    cur++;
  }

  line->num_of_filled_chars = dst_pos;

  return 1;
}

/*
 * vertical view logical <=> visual methods
 */

static int vert_delete(vt_logical_visual_t *logvis) {
  vert_logical_visual_t *vert_logvis;

  if (logvis->model) {
    vert_logvis = (vert_logical_visual_t*)logvis;
    vt_model_final(&vert_logvis->visual_model);
  }

  free(logvis);

  return 1;
}

static int vert_init(vt_logical_visual_t *logvis, vt_model_t *model, vt_cursor_t *cursor) {
  vert_logical_visual_t *vert_logvis;

  vert_logvis = (vert_logical_visual_t*)logvis;

  if (vert_logvis->is_init) {
    vt_model_resize(&vert_logvis->visual_model, NULL, model->num_of_rows, model->num_of_cols);
  } else {
    vt_model_init(&vert_logvis->visual_model, model->num_of_rows, model->num_of_cols);
    vert_logvis->is_init = 1;
  }

  vert_logvis->logical_model = *model;

  logvis->model = model;
  logvis->cursor = cursor;

  return 1;
}

static u_int vert_logical_cols(vt_logical_visual_t *logvis) {
  if (logvis->is_visual) {
    return ((vert_logical_visual_t*)logvis)->logical_model.num_of_cols;
  } else {
    return logvis->model->num_of_cols;
  }
}

static u_int vert_logical_rows(vt_logical_visual_t *logvis) {
  if (logvis->is_visual) {
    return ((vert_logical_visual_t*)logvis)->logical_model.num_of_rows;
  } else {
    return logvis->model->num_of_rows;
  }
}

static int vert_render(vt_logical_visual_t *logvis) { return 1; }

static void vert_set_modified(vt_line_t *vis_line, vt_line_t *log_line, int log_char_index) {
  /*
   * a:hankaku AA:zenkaku
   *
   * 012 3     0 1
   * 012345    0123
   * abAABB => AABB  : change_beg_col=0, change_end_col=3, beg_of_modified=0, end_of_modified=1
   *
   * 0 aa   => AA : should be redraw from 0 column to 3 column
   * 1 bb      BB
   * 2 AA
   * 3 BB
   *
   *    ||
   *
   * call vt_line_set_modified() from vt_line_get_beg_of_modified(log_line) to
   * log_line->change_end_col.
   */
  if (vt_line_is_modified(log_line) && vt_line_get_beg_of_modified(log_line) <= log_char_index &&
      (vt_line_is_cleared_to_end(log_line) || log_char_index <= log_line->change_end_col)) {
    vt_line_set_modified(vis_line, vis_line->num_of_filled_chars - 1,
                         vis_line->num_of_filled_chars - 1);
  }
}

static int vert_visual_intern(vt_logical_visual_t *logvis, vt_vertical_mode_t mode) {
  vert_logical_visual_t *vert_logvis;
  vt_line_t *log_line;
  vt_line_t *vis_line;
  int row;
  int count;

  if (logvis->is_visual) {
    return 0;
  }

#ifdef CURSOR_DEBUG
  bl_debug_printf(BL_DEBUG_TAG " logical cursor [col %d index %d row %d]\n", logvis->cursor->col,
                  logvis->cursor->char_index, logvis->cursor->row);
#endif

  vert_logvis = (vert_logical_visual_t*)logvis;

  if (vert_logvis->logical_model.num_of_rows != logvis->model->num_of_rows ||
      vert_logvis->logical_model.num_of_cols != logvis->model->num_of_cols) {
    /* vt_model_t is resized */

    vt_model_resize(&vert_logvis->visual_model, NULL, logvis->model->num_of_rows,
                    logvis->model->num_of_cols);
  }

  vt_model_reset(&vert_logvis->visual_model);

  if (mode & VERT_LTR) {
    /* Mongol */

    count = -1;
  } else {
    /* CJK */

    count = logvis->model->num_of_rows;
  }

  while (1) {
    if (mode & VERT_LTR) {
      /* Mongol */

      if (++count >= logvis->model->num_of_rows) {
        break;
      }
    } else {
      /* CJK */

      if (--count < 0) {
        break;
      }
    }

    log_line = vt_model_get_line(logvis->model, count);

    for (row = 0; row < log_line->num_of_filled_chars; row++) {
      vis_line = vt_model_get_line(&vert_logvis->visual_model, row);

      if (vis_line == NULL || vis_line->num_of_filled_chars >= vis_line->num_of_chars) {
        continue;
      }

      vt_char_copy(vt_char_at(vis_line, vis_line->num_of_filled_chars++),
                   vt_char_at(log_line, row));

      vert_set_modified(vis_line, log_line, row);
    }

    for (; row < vert_logvis->visual_model.num_of_rows; row++) {
      vis_line = vt_model_get_line(&vert_logvis->visual_model, row);

      if (vis_line == NULL || vis_line->num_of_filled_chars >= vis_line->num_of_chars) {
        continue;
      }

      vt_char_copy(vt_char_at(vis_line, vis_line->num_of_filled_chars++), vt_sp_ch());

      vert_set_modified(vis_line, log_line, row);
    }
  }

  vert_logvis->logical_model = *logvis->model;
  *logvis->model = vert_logvis->visual_model;

  vert_logvis->cursor_logical_char_index = logvis->cursor->char_index;
  vert_logvis->cursor_logical_col = logvis->cursor->col;
  vert_logvis->cursor_logical_row = logvis->cursor->row;

  logvis->cursor->row = vert_logvis->cursor_logical_char_index;
  logvis->cursor->char_index = logvis->cursor->col = 0;

  if (mode & VERT_LTR) {
    /* Mongol */

    logvis->cursor->col = logvis->cursor->char_index = vert_logvis->cursor_logical_row;
  } else {
    /* CJK */

    logvis->cursor->col = logvis->cursor->char_index =
        vert_logvis->logical_model.num_of_rows - vert_logvis->cursor_logical_row - 1;
  }

#ifdef CURSOR_DEBUG
  bl_debug_printf(BL_DEBUG_TAG " visual cursor [col %d index %d row %d]\n", logvis->cursor->col,
                  logvis->cursor->char_index, logvis->cursor->row);
#endif

  logvis->is_visual = 1;

  return 1;
}

static int cjk_vert_visual(vt_logical_visual_t *logvis) {
  return vert_visual_intern(logvis, VERT_RTL);
}

static int mongol_vert_visual(vt_logical_visual_t *logvis) {
  return vert_visual_intern(logvis, VERT_LTR);
}

static int vert_logical(vt_logical_visual_t *logvis) {
  vert_logical_visual_t *vert_logvis;

  if (!logvis->is_visual) {
    return 0;
  }

  vert_logvis = (vert_logical_visual_t*)logvis;

  *logvis->model = vert_logvis->logical_model;

  logvis->cursor->char_index = vert_logvis->cursor_logical_char_index;
  logvis->cursor->col = vert_logvis->cursor_logical_col;
  logvis->cursor->row = vert_logvis->cursor_logical_row;

#ifdef CURSOR_DEBUG
  bl_debug_printf(BL_DEBUG_TAG " logical cursor [col %d index %d row %d]\n", logvis->cursor->col,
                  logvis->cursor->char_index, logvis->cursor->row);
#endif

  logvis->is_visual = 0;

  return 1;
}

static int vert_visual_line(vt_logical_visual_t *logvis, vt_line_t *line) { return 1; }

#if !defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_FRIBIDI) || defined(USE_IND) || \
    defined(USE_OT_LAYOUT)

/*
 * Ctl logical <=> visual methods
 */

static int ctl_delete(vt_logical_visual_t *logvis) {
  int row;

  if (logvis->model) {
    for (row = 0; row < logvis->model->num_of_rows; row++) {
      vt_line_unuse_ctl(&logvis->model->lines[row]);
    }
  }

  free(logvis);

  return 1;
}

static int ctl_init(vt_logical_visual_t *logvis, vt_model_t *model, vt_cursor_t *cursor) {
  int row;

  if (logvis->model) {
    for (row = 0; row < logvis->model->num_of_rows; row++) {
      vt_line_unuse_ctl(&logvis->model->lines[row]);
    }
  }

  logvis->model = model;
  logvis->cursor = cursor;

  return 1;
}

static u_int ctl_logical_cols(vt_logical_visual_t *logvis) { return logvis->model->num_of_cols; }

static u_int ctl_logical_rows(vt_logical_visual_t *logvis) { return logvis->model->num_of_rows; }

static void ctl_render_line(vt_logical_visual_t *logvis, vt_line_t *line) {
  if (!vt_line_is_empty(line) && vt_line_is_modified(line)) {
    vt_line_ctl_render(line, ((ctl_logical_visual_t*)logvis)->bidi_mode,
                       ((ctl_logical_visual_t*)logvis)->separators,
                       ((ctl_logical_visual_t*)logvis)->term);
  }
}

static int ctl_render(vt_logical_visual_t *logvis) {
  if (!logvis->is_visual) {
    int row;

    /*
     * all lines(not only filled lines) should be rendered.
     */
    for (row = 0; row < logvis->model->num_of_rows; row++) {
      ctl_render_line(logvis, vt_model_get_line(logvis->model, row));
    }
  }

  return 1;
}

static int ctl_visual(vt_logical_visual_t *logvis) {
  int row;

  if (logvis->is_visual) {
    return 0;
  }

#ifdef CURSOR_DEBUG
  bl_debug_printf(BL_DEBUG_TAG " [cursor(index)%d (col)%d (row)%d (ltrmeet)%d] ->",
                  logvis->cursor->char_index, logvis->cursor->col, logvis->cursor->row,
                  ((ctl_logical_visual_t*)logvis)->cursor_meet_pos_info);
#endif

  for (row = 0; row < logvis->model->num_of_rows; row++) {
    if (!vt_line_ctl_visual(vt_model_get_line(logvis->model, row))) {
#ifdef __DEBUG
      bl_debug_printf(BL_DEBUG_TAG " visualize row %d failed.\n", row);
#endif
    }
  }

  ((ctl_logical_visual_t*)logvis)->cursor_logical_char_index = logvis->cursor->char_index;
  ((ctl_logical_visual_t*)logvis)->cursor_logical_col = logvis->cursor->col;

  logvis->cursor->char_index = vt_line_convert_logical_char_index_to_visual(
      CURSOR_LINE(logvis), logvis->cursor->char_index,
      &((ctl_logical_visual_t*)logvis)->cursor_meet_pos_info);
  /*
   * XXX
   * col_in_char should not be plused to col, because the character pointed by
   * vt_line_bidi_convert_logical_char_index_to_visual() is not the same as the
   * one
   * in logical order.
   */
  logvis->cursor->col =
      vt_convert_char_index_to_col(CURSOR_LINE(logvis), logvis->cursor->char_index, 0) +
      logvis->cursor->col_in_char;

#ifdef CURSOR_DEBUG
  bl_msg_printf("-> [cursor(index)%d (col)%d (row)%d (ltrmeet)%d]\n", logvis->cursor->char_index,
                logvis->cursor->col, logvis->cursor->row,
                ((bidi_logical_visual_t*)logvis)->cursor_meet_pos_info);
#endif

  logvis->is_visual = 1;

  return 1;
}

static int ctl_logical(vt_logical_visual_t *logvis) {
  int row;

  if (!logvis->is_visual) {
    return 0;
  }

#ifdef CURSOR_DEBUG
  bl_debug_printf(BL_DEBUG_TAG " [cursor(index)%d (col)%d (row)%d] ->", logvis->cursor->char_index,
                  logvis->cursor->col, logvis->cursor->row);
#endif

  for (row = 0; row < logvis->model->num_of_rows; row++) {
    vt_line_t *line;

    line = vt_model_get_line(logvis->model, row);

    if (!vt_line_ctl_logical(line)) {
#ifdef __DEBUG
      bl_debug_printf(BL_DEBUG_TAG " logicalize row %d failed.\n", row);
#endif
    }

#if 1
    /* XXX See vt_iscii_visual */
    if (line->num_of_chars > logvis->model->num_of_cols) {
      vt_str_final(line->chars + logvis->model->num_of_cols,
                   line->num_of_chars - logvis->model->num_of_cols);
      line->num_of_chars = logvis->model->num_of_cols;

      /*
       * line->num_of_filled_chars is equal or less than line->num_of_chars
       * because line is logicalized.
       */
    }
#endif
  }

  if (((ctl_logical_visual_t*)logvis)->cursor_meet_pos_info & MSB32) {
    /* cursor position is adjusted */
    vt_line_t *line = vt_model_get_line(logvis->model, logvis->cursor->row);
    int idx = vt_line_convert_visual_char_index_to_logical(line, logvis->cursor->char_index);
    vt_line_set_modified(line, idx, idx);
  }

  logvis->cursor->char_index = ((ctl_logical_visual_t*)logvis)->cursor_logical_char_index;
  logvis->cursor->col = ((ctl_logical_visual_t*)logvis)->cursor_logical_col;

#ifdef CURSOR_DEBUG
  bl_msg_printf("-> [cursor(index)%d (col)%d (row)%d]\n", logvis->cursor->char_index,
                logvis->cursor->col, logvis->cursor->row);
#endif

  logvis->is_visual = 0;

  return 1;
}

static int ctl_visual_line(vt_logical_visual_t *logvis, vt_line_t *line) {
  ctl_render_line(logvis, line);
  vt_line_ctl_visual(line);

  return 1;
}

#endif

/* --- global functions --- */

vt_logical_visual_t *vt_logvis_container_new(void) {
  container_logical_visual_t *container;

  if ((container = calloc(1, sizeof(container_logical_visual_t))) == NULL) {
    return NULL;
  }

  container->logvis.delete = container_delete;
  container->logvis.init = container_init;
  container->logvis.logical_cols = container_logical_cols;
  container->logvis.logical_rows = container_logical_rows;
  container->logvis.render = container_render;
  container->logvis.visual = container_visual;
  container->logvis.logical = container_logical;
  container->logvis.visual_line = container_visual_line;

  container->logvis.is_reversible = 1;

  return (vt_logical_visual_t*)container;
}

/*
 * logvis_comb can coexist with another logvise, but must be added to
 * logvis_container first of all.
 * vert_logvis, ctl_logvis and iscii_logvis can't coexist with each other
 * for now.
 */
int vt_logvis_container_add(vt_logical_visual_t *logvis, vt_logical_visual_t *child) {
  void *p;
  container_logical_visual_t *container;

  container = (container_logical_visual_t*)logvis;

  if ((p = realloc(container->children,
                   (container->num_of_children + 1) * sizeof(vt_logical_visual_t))) == NULL) {
    return 0;
  }

  container->children = p;

  container->children[container->num_of_children++] = child;

  if (!child->is_reversible) {
    container->logvis.is_reversible = 0;
  }

  return 1;
}

vt_logical_visual_t *vt_logvis_comb_new(void) {
  comb_logical_visual_t *comb_logvis;

  if ((comb_logvis = calloc(1, sizeof(comb_logical_visual_t))) == NULL) {
    return NULL;
  }

  comb_logvis->logvis.delete = comb_delete;
  comb_logvis->logvis.init = comb_init;
  comb_logvis->logvis.logical_cols = comb_logical_cols;
  comb_logvis->logvis.logical_rows = comb_logical_rows;
  comb_logvis->logvis.render = comb_render;
  comb_logvis->logvis.visual = comb_visual;
  comb_logvis->logvis.logical = comb_logical;
  comb_logvis->logvis.visual_line = comb_visual_line;

  comb_logvis->logvis.is_reversible = 1;

  return (vt_logical_visual_t*)comb_logvis;
}

vt_logical_visual_t *vt_logvis_vert_new(vt_vertical_mode_t vertical_mode) {
  vert_logical_visual_t *vert_logvis;

  if (vertical_mode != VERT_RTL && vertical_mode != VERT_LTR) {
    return NULL;
  }

  if ((vert_logvis = calloc(1, sizeof(vert_logical_visual_t))) == NULL) {
    return NULL;
  }

  vert_logvis->logvis.delete = vert_delete;
  vert_logvis->logvis.init = vert_init;
  vert_logvis->logvis.logical_cols = vert_logical_cols;
  vert_logvis->logvis.logical_rows = vert_logical_rows;
  vert_logvis->logvis.render = vert_render;
  vert_logvis->logvis.logical = vert_logical;
  vert_logvis->logvis.visual_line = vert_visual_line;

  if (vertical_mode == VERT_RTL) {
    /*
     * CJK type vertical view
     */

    vert_logvis->logvis.visual = cjk_vert_visual;
  } else /* if( vertical_mode == VERT_LTR) */
  {
    /*
     * mongol type vertical view
     */

    vert_logvis->logvis.visual = mongol_vert_visual;
  }

  return (vt_logical_visual_t*)vert_logvis;
}

vt_vertical_mode_t vt_get_vertical_mode(char *name) {
  vt_vertical_mode_t mode;

  for (mode = 0; mode < VERT_MODE_MAX; mode++) {
    if (strcmp(vertical_mode_name_table[mode], name) == 0) {
      return mode;
    }
  }

  /* default value */
  return 0;
}

char *vt_get_vertical_mode_name(vt_vertical_mode_t mode) {
  if (mode < 0 || VERT_MODE_MAX <= mode) {
    /* default value */
    mode = 0;
  }

  return vertical_mode_name_table[mode];
}

#if !defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_FRIBIDI) || defined(USE_IND) || \
    defined(USE_OT_LAYOUT)

vt_logical_visual_t *vt_logvis_ctl_new(vt_bidi_mode_t bidi_mode, const char *separators,
                                       void *term) {
  ctl_logical_visual_t *ctl_logvis;

#ifndef USE_OT_LAYOUT
#ifndef NO_DYNAMIC_LOAD_CTL
  if (!vt_load_ctl_bidi_func(VT_LINE_SET_USE_BIDI) &&
      !vt_load_ctl_iscii_func(VT_LINE_SET_USE_ISCII)) {
    return NULL;
  }
#endif
#endif

  if ((ctl_logvis = calloc(1, sizeof(ctl_logical_visual_t))) == NULL) {
    return NULL;
  }

  ctl_logvis->bidi_mode = bidi_mode;
  ctl_logvis->separators = separators;
  ctl_logvis->term = term;

  ctl_logvis->logvis.delete = ctl_delete;
  ctl_logvis->logvis.init = ctl_init;
  ctl_logvis->logvis.logical_cols = ctl_logical_cols;
  ctl_logvis->logvis.logical_rows = ctl_logical_rows;
  ctl_logvis->logvis.render = ctl_render;
  ctl_logvis->logvis.visual = ctl_visual;
  ctl_logvis->logvis.logical = ctl_logical;
  ctl_logvis->logvis.visual_line = ctl_visual_line;

  ctl_logvis->logvis.is_reversible = 1;

  return (vt_logical_visual_t*)ctl_logvis;
}

int vt_logical_visual_cursor_is_rtl(vt_logical_visual_t *logvis) {
  if (logvis->init == ctl_init) {
    vt_line_t *line;
    int ret = 0;

    if ((line = vt_model_get_line(logvis->model, logvis->cursor->row))) {
      int lidx = ((ctl_logical_visual_t*)logvis)->cursor_logical_char_index;
      int vidx1 = vt_line_convert_logical_char_index_to_visual(line, lidx > 0 ? lidx - 1 : 0, NULL);
      int vidx2 = vt_line_convert_logical_char_index_to_visual(line, lidx, NULL);
      int vidx3 = vt_line_convert_logical_char_index_to_visual(line, lidx + 1, NULL);

      if (vt_line_is_rtl(line) ? (vidx1 >= vidx2 && vidx2 >= vidx3) :
                                 (vidx1 > vidx2 || vidx2 > vidx3)) {
        ret = 1;
      }
    }

    if (((ctl_logical_visual_t*)logvis)->cursor_meet_pos_info & MSB32) {
      ret = !ret;
    }

    return ret;
  } else if (logvis->init == container_init) {
    u_int count;
    container_logical_visual_t *container = (container_logical_visual_t*)logvis;

    for (count = 0; count < container->num_of_children; count++) {
      if (vt_logical_visual_cursor_is_rtl(container->children[count])) {
        return 1;
      }
    }
  }

  return 0;
}

#endif
