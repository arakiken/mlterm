/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "vt_model.h"

#include <pobl/bl_mem.h> /* malloc/free */
#include <pobl/bl_debug.h>
#include <pobl/bl_util.h>

/* --- global functions --- */

int vt_model_init(vt_model_t* model, u_int num_of_cols, u_int num_of_rows) {
  int count;

  if (num_of_rows == 0 || num_of_cols == 0) {
    return 0;
  }

  model->num_of_rows = num_of_rows;
  model->num_of_cols = num_of_cols;

  if ((model->lines = calloc(sizeof(vt_line_t), model->num_of_rows)) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG "calloc() failed.\n");
#endif

    return 0;
  }

  for (count = 0; count < model->num_of_rows; count++) {
    if (!vt_line_init(&model->lines[count], model->num_of_cols)) {
      return 0;
    }
  }

  model->beg_row = 0;

  return 1;
}

int vt_model_final(vt_model_t* model) {
  int count;

  for (count = 0; count < model->num_of_rows; count++) {
    vt_line_final(&model->lines[count]);
  }

  free(model->lines);

  return 1;
}

int vt_model_reset(vt_model_t* model) {
  int count;

  for (count = 0; count < model->num_of_rows; count++) {
    vt_line_reset(&model->lines[count]);
    vt_line_set_updated(&model->lines[count]);
  }

  return 1;
}

int vt_model_resize(vt_model_t* model, u_int* slide, u_int num_of_cols, u_int num_of_rows) {
  int old_row;
  int new_row;
  int count;
  u_int copy_rows;
  vt_line_t* lines_p;
  u_int filled_rows;

  if (num_of_cols == 0 || num_of_rows == 0) {
    return 0;
  }

  if (num_of_cols == model->num_of_cols && num_of_rows == model->num_of_rows) {
    /* not resized */

    return 0;
  }

  if ((filled_rows = vt_model_get_num_of_filled_rows(model)) == 0 ||
      (lines_p = calloc(sizeof(vt_line_t), num_of_rows)) == NULL) {
    return 0;
  }

  if (num_of_rows >= filled_rows) {
    old_row = 0;
    copy_rows = filled_rows;
  } else {
    old_row = filled_rows - num_of_rows;
    copy_rows = num_of_rows;
  }

  if (slide) {
    *slide = old_row;
  }

  /* updating existing lines. */
  for (new_row = 0; new_row < copy_rows; new_row++) {
    vt_line_init(&lines_p[new_row], num_of_cols);

    vt_line_copy(&lines_p[new_row], vt_model_get_line(model, old_row));
    old_row++;

    vt_line_set_modified_all(&lines_p[new_row]);
  }

  /* freeing old data. */
  for (count = 0; count < model->num_of_rows; count++) {
    vt_line_final(&model->lines[count]);
  }
  free(model->lines);
  model->lines = lines_p;

  /* update empty lines. */
  for (; new_row < num_of_rows; new_row++) {
    vt_line_init(&lines_p[new_row], num_of_cols);

    vt_line_set_modified_all(&lines_p[new_row]);
  }

  model->num_of_rows = num_of_rows;
  model->num_of_cols = num_of_cols;

  model->beg_row = 0;

  return 1;
}

u_int vt_model_get_num_of_filled_rows(vt_model_t* model) {
  u_int filled_rows;

  for (filled_rows = model->num_of_rows; filled_rows > 0; filled_rows--) {
#if 0
    /*
     * This is problematic, since the value of 'slide' can be incorrect when
     * cursor is located at the line which contains white spaces alone.
     */
    if (vt_line_get_num_of_filled_chars_except_spaces(vt_model_get_line(model, filled_rows - 1)) >
        0)
#else
    if (!vt_line_is_empty(vt_model_get_line(model, filled_rows - 1)))
#endif
    {
      return filled_rows;
    }
  }

  return 0;
}

int vt_model_end_row(vt_model_t* model) { return model->num_of_rows - 1; }

vt_line_t* vt_model_get_line(vt_model_t* model, int row) {
  if (row < 0 || model->num_of_rows <= row) {
#ifdef __DEBUG
    bl_debug_printf(BL_DEBUG_TAG " row %d is out of range.\n", row);
#endif

    return NULL;
  }

  if (model->beg_row + row < model->num_of_rows) {
    return &model->lines[model->beg_row + row];
  } else {
    return &model->lines[model->beg_row + row - model->num_of_rows];
  }
}

int vt_model_scroll_upward(vt_model_t* model, u_int size) {
  if (size > model->num_of_rows) {
    size = model->num_of_rows;
  }

  if (model->beg_row + size >= model->num_of_rows) {
    model->beg_row = model->beg_row + size - model->num_of_rows;
  } else {
    model->beg_row += size;
  }

  return 1;
}

int vt_model_scroll_downward(vt_model_t* model, u_int size) {
  if (size > model->num_of_rows) {
    size = model->num_of_rows;
  }

  if (model->beg_row < size) {
    model->beg_row = model->num_of_rows - (size - model->beg_row);
  } else {
    model->beg_row -= size;
  }

  return 1;
}

#ifdef DEBUG

void vt_model_dump(vt_model_t* model) {
  int row;
  vt_line_t* line;

  for (row = 0; row < model->num_of_rows; row++) {
    line = vt_model_get_line(model, row);

    if (vt_line_is_modified(line)) {
      bl_msg_printf("!%.2d-%.2d", vt_line_get_beg_of_modified(line),
                    vt_line_get_end_of_modified(line));
    } else {
      bl_msg_printf("      ");
    }

    bl_msg_printf("[%.2d %.2d]", line->num_of_filled_chars, vt_line_get_num_of_filled_cols(line));

    vt_str_dump(line->chars, line->num_of_filled_chars);
  }
}

#endif
