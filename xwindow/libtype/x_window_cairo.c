/*
 *	$Id$
 */

#include  "../x_window.h"

#include  <cairo/cairo.h>
#include  <cairo/cairo-ft.h>	/* FcChar32 */
#include  <cairo/cairo-xlib.h>

#include  <ml_char.h>	/* UTF_MAX_SIZE */


#define  DIVIDE_ROUNDINGUP(a,b) ( ((int)((a)*10 + (b)*10 - 1)) / ((int)((b)*10)) )

typedef struct
{
	FcCharSet *  charset ;
	cairo_scaled_font_t *  next ;

} user_data_t ;

/* Implemented in x_font_ft.c */
size_t  x_convert_ucs4_to_utf8( u_char *  utf8 , u_int32_t  ucs) ;


/* --- static functions --- */

static void
adjust_glyphs(
	x_font_t *  font ,
	cairo_glyph_t *  glyphs ,
	int  num_of_glyphs
	)
{
	if( ! font->is_var_col_width)
	{
		int  count ;
		double  prev_x ;
		int  adjust ;
		int  std_width ;

		adjust = 0 ;
		prev_x = glyphs[0].x ;
		for( count = 1 ; count < num_of_glyphs ; count++)
		{
			int  w ;

			w = glyphs[count].x - prev_x ;
			prev_x = glyphs[count].x ;

			if( ! adjust)
			{
				if( w == font->width)
				{
					continue ;
				}
				adjust = 1 ;
				std_width = font->width - font->x_off * 2 ;
			}

			glyphs[count].x = glyphs[count - 1].x + font->width ;
			glyphs[count - 1].x += ((std_width - w) / 2) ;
		}
	}
}

static int
show_text(
	cairo_t *  cr ,
	cairo_scaled_font_t *  xfont ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	int  x ,
	int  y ,
	u_char *  str ,		/* NULL-terminated UTF8 */
	int  double_draw_gap
	)
{
#if  CAIRO_VERSION_ENCODE(1,4,0) <= CAIRO_VERSION
	if( cairo_get_user_data( cr , 1) != xfont)
#endif
	{
		cairo_set_scaled_font( cr , xfont) ;
	#if  CAIRO_VERSION_ENCODE(1,4,0) <= CAIRO_VERSION
		cairo_set_user_data( cr , 1 , xfont , NULL) ;
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

	if( double_draw_gap)
	{
		cairo_move_to( cr , x + double_draw_gap , y) ;
		cairo_show_text( cr , str) ;
	}

	return  1 ;
#else
	static cairo_glyph_t *  glyphs ;
	static int  num_of_glyphs ;
	cairo_glyph_t *  orig_glyphs ;
	u_char *  str2 ;
	size_t  str_len ;

	orig_glyphs = glyphs ;

	if( ! ( str2 = alloca( (str_len = strlen(str)) + 2)))
	{
		return  0 ;
	}

	memcpy( str2 , str , str_len) ;
	str2[str_len] = ' ' ;	/* dummy */
	str2[str_len + 1] = '\0' ;

	if( cairo_scaled_font_text_to_glyphs( xfont , x , y , str2 , str_len + 1 ,
			&glyphs , &num_of_glyphs , NULL , NULL , NULL) == CAIRO_STATUS_SUCCESS)
	{
		adjust_glyphs( font , glyphs , num_of_glyphs) ;
		num_of_glyphs -- ;	/* remove dummy */
		cairo_show_glyphs( cr , glyphs , num_of_glyphs) ;

		if( double_draw_gap)
		{
			int  count ;

			for( count = 0 ; count < num_of_glyphs ; count++)
			{
				glyphs[count].x += double_draw_gap ;
			}

			cairo_show_glyphs( cr , glyphs , num_of_glyphs) ;
		}
	}

	if( orig_glyphs != glyphs)
	{
		cairo_glyph_free( orig_glyphs) ;
	}

	if( num_of_glyphs > 0)
	{
		return  glyphs[num_of_glyphs].x ;
	}
	else
	{
		return  0 ;
	}
#endif
}


static cairo_scaled_font_t *
cairo_font_open(
	Display *  display ,
	cairo_scaled_font_t *  orig_xfont ,
	int  ch
	)
{
	cairo_font_options_t *  options ;
	cairo_t *  cairo ;
	FcPattern *  pattern ;
	FcPattern *  match ;
	FcResult  result ;
	cairo_font_face_t *  font_face ;
	cairo_matrix_t  font_matrix ;
	cairo_matrix_t  ctm ;
	cairo_scaled_font_t *  xfont ;
	double  pixel_size ;
	int  pixel_size2 ;
	FcCharSet *  charset ;

	if( ! ( pattern = FcPatternCreate()))
	{
		return  NULL ;
	}

	FcConfigSubstitute( NULL , pattern , FcMatchPattern) ;

	if( ! ( cairo = cairo_create( cairo_xlib_surface_create( display ,
				DefaultRootWindow( display) ,
				DefaultVisual( display , DefaultScreen( display)) ,
				DisplayWidth( display , DefaultScreen( display)) ,
				DisplayHeight( display , DefaultScreen( display))))))
	{
		FcPatternDestroy( pattern) ;

		return  NULL ;
	}

	options = cairo_font_options_create() ;
	cairo_scaled_font_get_font_options( orig_xfont , options) ;
	/* For performance */
	cairo_font_options_set_hint_style( options , CAIRO_HINT_STYLE_NONE) ;
	cairo_ft_font_options_substitute( options , pattern) ;

	FcDefaultSubstitute( pattern) ;

#if  1
	FcPatternRemove( pattern , FC_FAMILYLANG , 0) ;
	FcPatternRemove( pattern , FC_STYLELANG , 0) ;
	FcPatternRemove( pattern , FC_FULLNAMELANG , 0) ;
	FcPatternRemove( pattern , FC_NAMELANG , 0) ;
	FcPatternRemove( pattern , FC_LANG , 0) ;
#endif

	while( 1)
	{
		FcValue  val ;

		if( FcPatternGet( pattern , FC_FAMILY , 0 , &val) == FcResultMatch)
		{
			FcPatternRemove( pattern , FC_FAMILY , 0) ;
		}
		else
		{
			FcPatternDestroy( pattern) ;
			cairo_destroy( cairo) ;
			cairo_font_options_destroy( options) ;

			return  NULL ;
		}

		match = FcFontMatch( NULL , pattern , &result) ;

		if( ! match)
		{
			FcPatternDestroy( pattern) ;
			cairo_destroy( cairo) ;
			cairo_font_options_destroy( options) ;

			return  NULL ;
		}

	#if  0
		FcPatternPrint( match) ;
	#endif

		font_face = cairo_ft_font_face_create_for_pattern( match) ;

		FcPatternGetDouble( match , FC_PIXEL_SIZE , 0 , &pixel_size) ;
		/*
		 * 10.5 / 2.0 = 5.25 ->(roundup) 6 -> 6 * 2 = 12
		 * 11.5 / 2.0 = 5.75 ->(roundup) 6 -> 6 * 2 = 12
		 *
		 * If half width is 5.25 -> 6 and full width is 5.25 * 2 = 10.5 -> 11,
		 * half width char -> x_bearing = 1 / width 5
		 * full width char -> x_bearing = 1 / width 10.
		 * This results in gap between chars.
		 */
		pixel_size2 = DIVIDE_ROUNDINGUP(pixel_size,2.0) * 2 ;

		cairo_matrix_init_scale( &font_matrix , pixel_size2 , pixel_size2) ;
		cairo_get_matrix( cairo , &ctm) ;

		xfont = cairo_scaled_font_create( font_face , &font_matrix , &ctm , options) ;

		cairo_font_face_destroy( font_face) ;

		if( cairo_scaled_font_status( xfont))
		{
			FcPatternDestroy( match) ;
			cairo_scaled_font_destroy( xfont) ;

			return  NULL ;
		}

		result = FcPatternGetCharSet( match , FC_CHARSET , 0 , &charset) ;

		FcPatternDestroy( match) ;

		if( result == FcResultMatch)
		{
			user_data_t *  user_data ;

			if( ! FcCharSetHasChar( charset , ch))
			{
				cairo_scaled_font_destroy( xfont) ;

				continue ;
			}
			else if( ( user_data = malloc( sizeof(*user_data))))
			{
				user_data->charset = FcCharSetCopy( charset) ;
				user_data->next = NULL ;

				cairo_scaled_font_set_user_data( xfont , 1 , user_data , NULL) ;
			}
		}

		break ;
	}

	FcPatternDestroy( pattern) ;
	cairo_font_options_destroy( options) ;
	cairo_destroy( cairo) ;

	return  xfont ;
}

static cairo_scaled_font_t *
search_next_font(
	Display *  display ,
	cairo_scaled_font_t *  orig_xfont ,
	user_data_t *  user_data ,
	int  ch
	)
{
	while( user_data->next)
	{
		cairo_scaled_font_t *  xfont ;

		if( ! ( user_data = cairo_scaled_font_get_user_data( (xfont = user_data->next) , 1)))
		{
			return  NULL ;
		}

		if( FcCharSetHasChar( user_data->charset , ch))
		{
			return  xfont ;
		}
	}

	return  (user_data->next = cairo_font_open( display , orig_xfont , ch)) ;
}

static int
draw_string32(
	x_window_t *  win ,
	cairo_scaled_font_t *  xfont ,
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

	return  show_text( win->cairo_draw , xfont , font , fg_color ,
			x + win->hmargin ,
			y + win->vmargin , buf , font->double_draw_gap) ;
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

	show_text( win->cairo_draw , font->cairo_font , font , fg_color ,
		x + font->x_off + win->hmargin ,
		y + win->vmargin , buf , font->double_draw_gap) ;

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
	cairo_scaled_font_t *  xfont ;
	user_data_t *  user_data ;

	xfont = font->cairo_font ;

	if( ( user_data = cairo_scaled_font_get_user_data( xfont , 1)))
	{
		u_int  count ;

		for( count = 0 ; count < len ; count++)
		{
			if( ! FcCharSetHasChar( user_data->charset , str[count]))
			{
				cairo_scaled_font_t *  next_xfont ;

				if( ( next_xfont = search_next_font( win->disp->display ,
								xfont , user_data , str[count])))
				{
					FcChar32 *  substr ;
					user_data_t *  next_user_data ;

					if( count > 0)
					{
						x += draw_string32( win , xfont , font , fg_color ,
							x + font->x_off , y ,
							str , count) ;
					}

					substr = str + count ;
					next_user_data = cairo_scaled_font_get_user_data(
								next_xfont , 1) ;

					for( count ++ ; count < len &&
					       ! FcCharSetHasChar( user_data->charset , str[count]) &&
					       FcCharSetHasChar( next_user_data->charset , str[count]) ;
					       count ++) ;

					x += draw_string32( win , next_xfont , font , fg_color ,
							x + font->x_off , y , substr ,
							substr - str + count) ;
					str += count ;
					len -= count ;
					count = 0 ;
				}
			}
		}
	}

	draw_string32( win , xfont , font , fg_color , x + font->x_off , y , str , len) ;

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
