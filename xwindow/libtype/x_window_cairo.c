/*
 *	$Id$
 */

#include  "../x_window.h"

#include  <cairo/cairo.h>
#include  <cairo/cairo-ft.h>	/* FcChar32 */
#include  <cairo/cairo-xlib.h>

#include  <ml_char.h>	/* UTF_MAX_SIZE */


/* Implemented in x_font_ft.c */
size_t  x_convert_ucs4_to_utf8( u_char *  utf8 , u_int32_t  ucs) ;


/* --- static functions --- */

static int
show_text(
	cairo_t *  cr ,
	cairo_scaled_font_t *  font ,
	x_color_t *  fg_color ,
	int  x ,
	int  y ,
	u_char *  str ,		/* NULL-terminated UTF8 */
	int  is_double_drawing
	)
{
#if  CAIRO_VERSION_ENCODE(1,4,0) <= CAIRO_VERSION
	if( cairo_get_user_data( cr , 1) != font)
#endif
	{
		cairo_set_scaled_font( cr , font) ;
	#if  CAIRO_VERSION_ENCODE(1,4,0) <= CAIRO_VERSION
		cairo_set_user_data( cr , 1 , font , NULL) ;
	#endif
	}

#if  CAIRO_VERSION_ENCODE(1,4,0) <= CAIRO_VERSION
	/*
	 * If cairo_get_user_data() returns NULL, it means that source rgb value is default one
	 * (black == 0).
	 */
	if( (u_long)cairo_get_user_data( cr , 2) != fg_color->pixel)
#endif
	{
		cairo_set_source_rgba( cr ,
			(double)fg_color->red / 255.0 , (double)fg_color->green / 255.0 ,
			(double)fg_color->blue / 255.0 , (double)fg_color->alpha / 255.0) ;
	#if  CAIRO_VERSION_ENCODE(1,4,0) <= CAIRO_VERSION
		cairo_set_user_data( cr , 2 , fg_color->pixel , NULL) ;
	#endif
	}

#if  CAIRO_VERSION_ENCODE(1,8,0) > CAIRO_VERSION
	cairo_move_to( cr , x , y) ;
	cairo_show_text( cr , str) ;

	if( is_double_drawing)
	{
		cairo_move_to( cr , x + 1 , y) ;
		cairo_show_text( cr , str) ;
	}
#else
	static cairo_glyph_t *  glyphs ;
	static int  num_of_glyphs ;
	cairo_glyph_t *  orig_glyphs ;

	orig_glyphs = glyphs ;

	if( cairo_scaled_font_text_to_glyphs( font , x , y , str , -1 ,
			&glyphs , &num_of_glyphs , NULL , NULL , NULL) == CAIRO_STATUS_SUCCESS)
	{
		cairo_show_glyphs( cr , glyphs , num_of_glyphs) ;

		if( is_double_drawing)
		{
			int  count ;

			for( count = 0 ; count < num_of_glyphs ; count++)
			{
				glyphs[count].x += 1.0 ;
			}

			cairo_show_glyphs( cr , glyphs , num_of_glyphs) ;
		}
	}

	if( orig_glyphs != glyphs)
	{
		cairo_glyph_free( orig_glyphs) ;
	}
#endif

	return  1 ;
}


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
	u_char *  p ;

	/* Removing trailing spaces. */
	while( 1)
	{
		if( len == 0)
		{
			return  1 ;
		}

		if( *(str + len - 1) == ' ')
		{
			len-- ;
		}
		else
		{
			break ;
		}
	}

	/* Max utf8 size of 0x80 - 0xff is 2 */
	if( ! ( p = buf = alloca( 2 * len + 1)))
	{
		return  0 ;
	}

	for( count = 0 ; count < len ; count++)
	{
		p += x_convert_ucs4_to_utf8( p , (u_int32_t)(str[count])) ;
	}
	*p = '\0' ;

	show_text( win->cairo_draw , font->cairo_font , fg_color ,
		x + win->margin , y + win->margin , buf , font->is_double_drawing) ;

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
		p += x_convert_ucs4_to_utf8( p , str[count]) ;
	}
	*p = '\0' ;

	show_text( win->cairo_draw , font->cairo_font , fg_color ,
		x + win->margin , y + win->margin , buf , font->is_double_drawing) ;

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
