/*
 *	$Id$
 */

#include  "../x_window.h"

#include  <cairo/cairo.h>
#include  <cairo/cairo-ft.h>	/* FcChar32 */
#include  <cairo/cairo-xlib.h>

#include  <ml_char.h>	/* UTF_MAX_SIZE */


/* Implemented in x_font_ft.c */
size_t  x_convert_ucs_to_utf8( u_char *  utf8 ,	u_int32_t  ucs) ;


/* --- global functions --- */

int
x_window_set_use_cairo(
	x_window_t *  win ,
	int  use_cairo
	)
{
	if( use_cairo)
	{
		if( ( win->cairo_draw = cairo_create( cairo_xlib_surface_create(
						win->disp->display , win->my_window ,
						win->disp->visual ,
						ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win)))))
		{
			return  1 ;
		}
	}
	else
	{
		cairo_destroy( win->cairo_draw) ;
		win->cairo_draw = NULL ;

		return  1 ;
	}

	return  0 ;
}

int
x_window_cairo_draw_string8(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	int  x ,
	int  y ,
	u_char *  str ,
	size_t  len
	)
{
	u_char *  buf ;
	size_t  count ;
	char *  p ;

	/* Max utf8 size of 0x80 - 0xff is 2 */
	if( ! ( p = buf = alloca( 2 * len + 1)))
	{
		return  0 ;
	}

	for( count = 0 ; count < len ; count++)
	{
		p += x_convert_ucs_to_utf8( p , (u_int32_t)(str[count])) ;
	}
	*p = '\0' ;

	cairo_set_scaled_font( win->cairo_draw , font->cairo_font) ;
	cairo_set_source_rgb( win->cairo_draw ,
		(double)fg_color->red / 255.0 , (double)fg_color->green / 255.0 ,
		(double)fg_color->blue / 255.0) ;
	cairo_move_to( win->cairo_draw , x + win->margin , y + win->margin) ;
	cairo_show_text( win->cairo_draw , buf) ;

	if( font->is_double_drawing)
	{
		cairo_move_to( win->cairo_draw , x + win->margin + 1 , y + win->margin) ;
		cairo_show_text( win->cairo_draw , buf) ;
	}

	return  1 ;
}

int
x_window_cairo_draw_string32(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	int  x ,
	int  y ,
	FcChar32 *  str ,
	u_int  len
	)
{
	u_char *  buf ;
	u_int  count ;
	char *  p ;

	if( ! ( p = buf = alloca( UTF_MAX_SIZE * len + 1)))
	{
		return  0 ;
	}

	for( count = 0 ; count < len ; count++)
	{
		p += x_convert_ucs_to_utf8( p , str[count]) ;
	}
	*p = '\0' ;

	cairo_set_scaled_font( win->cairo_draw , font->cairo_font) ;
	cairo_set_source_rgb( win->cairo_draw ,
		(double)fg_color->red / 255.0 , (double)fg_color->green / 255.0 ,
		(double)fg_color->blue / 255.0) ;
	cairo_move_to( win->cairo_draw , x + win->margin , y + win->margin) ;
	cairo_show_text( win->cairo_draw , buf) ;

	if( font->is_double_drawing)
	{
		cairo_move_to( win->cairo_draw , x + win->margin + 1 , y + win->margin) ;
		cairo_show_text( win->cairo_draw , buf) ;
	}

	return  1 ;
}

int
cairo_resize(
	x_window_t *  win
	)
{
	cairo_xlib_surface_set_size( cairo_get_target( win->cairo_draw) ,
			ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win)) ;

	return  1 ;
}
