/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_EDIT_UTIL_H__
#define __VT_EDIT_UTIL_H__

#include <pobl/bl_debug.h>

#include "vt_edit.h"

#define CURSOR_LINE(edit) vt_get_cursor_line(&(edit)->cursor)
#define CURSOR_CHAR(edit) vt_get_cursor_char(&(edit)->cursor)

int vt_edit_clear_lines(vt_edit_t* edit, int start, u_int size);

int vt_edit_copy_lines(vt_edit_t* edit, int dst_row, int src_row, u_int size, int mark_changed);

#endif
