/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_EDIT_SCROLL_H__
#define __VT_EDIT_SCROLL_H__

#include "vt_edit.h"

int vt_edsl_scroll_upward(vt_edit_t *edit, u_int size);

int vt_edsl_scroll_downward(vt_edit_t *edit, u_int size);

#if 0
int vt_edsl_scroll_upward_in_all(vt_edit_t *edit, u_int size);

int vt_edsl_scroll_downward_in_all(vt_edit_t *edit, u_int size);
#endif

int vt_is_scroll_upperlimit(vt_edit_t *edit, int row);

int vt_is_scroll_lowerlimit(vt_edit_t *edit, int row);

int vt_edsl_delete_line(vt_edit_t *edit);

int vt_edsl_insert_new_line(vt_edit_t *edit);

#endif
