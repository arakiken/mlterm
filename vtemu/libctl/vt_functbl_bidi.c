/*
 *	$Id$
 */

#include "../vt_ctl_loader.h"

/* Dummy declaration */
void vt_line_set_use_bidi(void);
void vt_line_bidi_convert_logical_char_index_to_visual(void);
void vt_line_bidi_convert_visual_char_index_to_logical(void);
void vt_line_bidi_copy_logical_str(void);
void vt_line_bidi_is_rtl(void);
void vt_is_rtl_char(void);
void vt_bidi_copy(void);
void vt_bidi_reset(void);
void vt_line_bidi_need_shape(void);
void vt_line_bidi_render(void);
void vt_line_bidi_visual(void);
void vt_line_bidi_logical(void);

/* --- global variables --- */

void* vt_ctl_bidi_func_table[MAX_CTL_BIDI_FUNCS] = {
    (void*)CTL_API_COMPAT_CHECK_MAGIC, vt_line_set_use_bidi,
    vt_line_bidi_convert_logical_char_index_to_visual,
    vt_line_bidi_convert_visual_char_index_to_logical, vt_line_bidi_copy_logical_str,
    vt_line_bidi_is_rtl, vt_shape_arabic, vt_is_arabic_combining, vt_is_rtl_char, vt_bidi_copy,
    vt_bidi_reset, vt_line_bidi_need_shape, vt_line_bidi_render, vt_line_bidi_visual,
    vt_line_bidi_logical,

};
