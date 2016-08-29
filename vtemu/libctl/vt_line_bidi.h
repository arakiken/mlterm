/*
 *	$Id$
 */

#ifndef __VT_LINE_BIDI_H__
#define __VT_LINE_BIDI_H__

#include "../vt_line.h"
#include "vt_bidi.h" /* vt_bidi_mode_t */

#define vt_line_is_using_bidi(line) ((line)->ctl_info_type == VINFO_BIDI)

int vt_line_set_use_bidi(vt_line_t* line, int flag);

int vt_line_bidi_render(vt_line_t* line, vt_bidi_mode_t bidi_mode, const char* separators);

int vt_line_bidi_visual(vt_line_t* line);

int vt_line_bidi_logical(vt_line_t* line);

int vt_line_bidi_convert_logical_char_index_to_visual(vt_line_t* line, int char_index,
                                                      int* ltr_rtl_meet_pos);

#endif
