/*
 *	$Id$
 */

#include "../xlib/ui_type_loader.h"

#include <stdio.h> /* NULL */

/* Dummy declaration */
void ui_window_set_use_xft(void);
void ui_window_xft_draw_string8(void);
void ui_window_xft_draw_string32(void);
void xft_set_font(void);
void xft_unset_font(void);
void xft_calculate_char_width(void);
void xft_set_clip(void);
void xft_unset_clip(void);
void xft_set_ot_font(void);
void ft_convert_text_to_glyphs(void);

/* --- global variables --- */

void* ui_type_xft_func_table[MAX_TYPE_FUNCS] = {
    (void*)TYPE_API_COMPAT_CHECK_MAGIC, ui_window_set_use_xft, ui_window_xft_draw_string8,
    ui_window_xft_draw_string32, NULL, xft_set_font, xft_unset_font, xft_calculate_char_width,
    xft_set_clip, xft_unset_clip, xft_set_ot_font, ft_convert_text_to_glyphs,

};
