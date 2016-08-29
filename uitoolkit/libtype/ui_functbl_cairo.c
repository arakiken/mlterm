/*
 *	$Id$
 */

#include "../xlib/ui_type_loader.h"

/* Dummy declaration */
void ui_window_set_use_cairo(void);
void ui_window_cairo_draw_string8(void);
void ui_window_cairo_draw_string32(void);
void cairo_resize(void);
void cairo_set_font(void);
void cairo_unset_font(void);
void cairo_calculate_char_width(void);
void cairo_set_clip(void);
void cairo_unset_clip(void);
void cairo_set_ot_font(void);
void ft_convert_text_to_glyphs(void);

/* --- global variables --- */

void* ui_type_cairo_func_table[MAX_TYPE_FUNCS] = {
    (void*)TYPE_API_COMPAT_CHECK_MAGIC, ui_window_set_use_cairo, ui_window_cairo_draw_string8,
    ui_window_cairo_draw_string32, cairo_resize, cairo_set_font, cairo_unset_font,
    cairo_calculate_char_width, cairo_set_clip, cairo_unset_clip, cairo_set_ot_font,
    ft_convert_text_to_glyphs,

};
