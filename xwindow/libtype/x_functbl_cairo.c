/*
 *	$Id$
 */

#include  "../x_type_loader.h"


/* Dummy declaration */
void  x_window_set_use_cairo(void) ;
void  x_window_cairo_draw_string8(void) ;
void  x_window_cairo_draw_string32(void) ;
void  cairo_resize(void) ;
void  cairo_set_font(void) ;
void  cairo_unset_font(void) ;
void  cairo_calculate_char_width(void) ;


/* --- global variables --- */

void *  x_type_cairo_func_table[MAX_TYPE_FUNCS] =
{
	(void*)TYPE_API_COMPAT_CHECK_MAGIC ,
	x_window_set_use_cairo ,
	x_window_cairo_draw_string8 ,
	x_window_cairo_draw_string32 ,
	cairo_resize ,
	cairo_set_font ,
	cairo_unset_font ,
	cairo_calculate_char_width ,

} ;
