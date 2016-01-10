/*
 *	$Id$
 */

#include  "../xlib/x_type_loader.h"

#include  <stdio.h>		/* NULL */


/* Dummy declaration */
void  x_window_set_use_xft(void) ;
void  x_window_xft_draw_string8(void) ;
void  x_window_xft_draw_string32(void) ;
void  xft_set_font(void) ;
void  xft_unset_font(void) ;
void  xft_calculate_char_width(void) ;
void  xft_set_clip(void) ;
void  xft_unset_clip(void) ;
void  xft_set_otf(void) ;
void  ft_convert_text_to_glyphs(void) ;


/* --- global variables --- */

void *  x_type_xft_func_table[MAX_TYPE_FUNCS] =
{
	(void*)TYPE_API_COMPAT_CHECK_MAGIC ,
	x_window_set_use_xft ,
	x_window_xft_draw_string8 ,
	x_window_xft_draw_string32 ,
	NULL ,
	xft_set_font ,
	xft_unset_font ,
	xft_calculate_char_width ,
	xft_set_clip ,
	xft_unset_clip ,
	xft_set_otf ,
	ft_convert_text_to_glyphs ,

} ;
