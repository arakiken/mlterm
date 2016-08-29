/*
 *	$Id$
 */

#include "vt_edit_util.h"

#include <pobl/bl_debug.h>

/* --- global functions --- */

/*
 * used in vt_edit/vt_edit_scroll
 */
int vt_edit_clear_lines(vt_edit_t* edit, int beg_row, u_int size) {
  int count;

  if (size == 0) {
    return 0;
  }

  if (beg_row > vt_model_end_row(&edit->model)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " line %d is already empty.\n", beg_row);
#endif

    return 0;
  }

  if (edit->use_bce) {
    for (count = 0; count < size; count++) {
      vt_line_clear_with(vt_model_get_line(&edit->model, beg_row + count), 0, &edit->bce_ch);
    }
  } else {
    for (count = 0; count < size; count++) {
      vt_line_reset(vt_model_get_line(&edit->model, beg_row + count));
    }
  }

  if (beg_row <= edit->cursor.row && edit->cursor.row <= beg_row + size - 1) {
    u_int brk_size;

    if ((brk_size = vt_line_break_boundary(CURSOR_LINE(edit), edit->cursor.col + 1)) == 0) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " critical error.\n");
#endif

      edit->cursor.char_index = edit->cursor.col = 0;
    } else {
      edit->cursor.char_index = edit->cursor.col = brk_size - 1;
    }

    edit->cursor.col_in_char = 0;
  }

  return 1;
}
