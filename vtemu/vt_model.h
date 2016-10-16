/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_MODEL_H__
#define __VT_MODEL_H__

#include <pobl/bl_types.h>

#include "vt_str.h"
#include "vt_line.h"

typedef struct vt_model {
  /* private */
  vt_line_t* lines;

  /* public(readonly) */
  u_int16_t num_of_cols; /* 0 - 65536 */
  u_int16_t num_of_rows; /* 0 - 65536 */

  /* private */
  int beg_row; /* used for scrolling */

} vt_model_t;

int vt_model_init(vt_model_t* model, u_int num_of_cols, u_int num_of_rows);

int vt_model_final(vt_model_t* model);

int vt_model_reset(vt_model_t* model);

int vt_model_resize(vt_model_t* model, u_int* slide, u_int num_of_cols, u_int num_of_rows);

u_int vt_model_get_num_of_filled_rows(vt_model_t* model);

int vt_model_end_row(vt_model_t* model);

vt_line_t* vt_model_get_line(vt_model_t* model, int row);

int vt_model_scroll_upward(vt_model_t* model, u_int size);

int vt_model_scroll_downward(vt_model_t* model, u_int size);

#ifdef DEBUG

void vt_model_dump(vt_model_t* model);

#endif

#endif
