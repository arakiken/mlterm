/*
 *	$Id$
 */

#include  "../x_window.h"

#include  <cairo/cairo.h>
#include  <cairo/cairo-ft.h>	/* FcChar32 */
#include  <cairo/cairo-xlib.h>

#include  <ml_char.h>	/* UTF_MAX_SIZE */


#define  DIVIDE_ROUNDINGUP(a,b) ( ((int)((a)*10 + (b)*10 - 1)) / ((int)((b)*10)) )


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

	if( font->size_attr == DOUBLE_WIDTH)
	{
		x /= 2 ;
		font->width /= 2 ;
		cairo_scale( cr , 2.0 , 1.0) ;
	}

#if  CAIRO_VERSION_ENCODE(1,8,0) > CAIRO_VERSION
	cairo_move_to( cr , x , y) ;
	cairo_show_text( cr , str) ;

	if( double_draw_gap)
	{
		cairo_move_to( cr , x + double_draw_gap , y) ;
		cairo_show_text( cr , str) ;
	}

	if( font->size_attr == DOUBLE_WIDTH)
	{
		font->width *= 2 ;
		cairo_scale( cr , 0.5 , 1.0) ;
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

	if( font->size_attr == DOUBLE_WIDTH)
	{
		font->width *= 2 ;
		cairo_scale( cr , 0.5 , 1.0) ;
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


static int
cairo_font_open(
	x_font_t *  font ,
	int  num_of_compl_fonts ,
	FcPattern *  orig_pattern ,
	int  ch
	)
{
	int  count ;
	cairo_t *  cairo = NULL ;
	cairo_font_options_t *  options ;
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
	int  ret = 0 ;

	if( ! ( pattern = FcPatternDuplicate( orig_pattern)))
	{
		return  0 ;
	}

	for( count = 0 ; ; count++)
	{
		FcValue  val ;

		if( FcPatternGet( pattern , FC_FAMILY , 0 , &val) == FcResultMatch)
		{
			FcPatternRemove( pattern , FC_FAMILY , 0) ;
		}
		else
		{
			break ;
		}

		if( ! ( match = FcFontMatch( NULL , pattern , &result)))
		{
			break ;
		}

		result = FcPatternGetCharSet( match , FC_CHARSET , 0 , &charset) ;

		if( result == FcResultMatch)
		{
			void *  p ;

			if( ! FcCharSetHasChar( charset , ch))
			{
				FcPatternDestroy( match) ;

				continue ;
			}
			else if( ( p = realloc( font->compl_fonts ,
					sizeof(*font->compl_fonts) * (num_of_compl_fonts + 1))))
			{
				font->compl_fonts = p ;
			}
			else
			{
				FcPatternDestroy( match) ;

				break ;
			}
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

		FcPatternDestroy( match) ;

		cairo_matrix_init_scale( &font_matrix , pixel_size2 , pixel_size2) ;

		if( ! cairo)
		{
			if( ! ( cairo = cairo_create( cairo_xlib_surface_create( font->display ,
						DefaultRootWindow( font->display) ,
						DefaultVisual( font->display ,
							DefaultScreen( font->display)) ,
						DisplayWidth( font->display ,
							DefaultScreen( font->display)) ,
						DisplayHeight( font->display ,
							DefaultScreen( font->display))))))
			{
				break ;
			}

			options = cairo_font_options_create() ;
			cairo_get_font_options( cairo , options) ;
			/* For performance */
			cairo_font_options_set_hint_style( options , CAIRO_HINT_STYLE_NONE) ;
		}

		cairo_get_matrix( cairo , &ctm) ;

		if( ! ( xfont = cairo_scaled_font_create( font_face ,
					&font_matrix , &ctm , options)))
		{
			break ;
		}

		cairo_font_face_destroy( font_face) ;

		if( cairo_scaled_font_status( xfont))
		{
			cairo_scaled_font_destroy( xfont) ;

			break ;
		}

		font->compl_fonts[num_of_compl_fonts - 1].next = xfont ;
		font->compl_fonts[num_of_compl_fonts].charset = FcCharSetCopy( charset) ;
		font->compl_fonts[num_of_compl_fonts].next = NULL ;

		FcPatternRemove( orig_pattern , FC_FAMILY , count) ;
		ret = 1 ;

		break ;
	}

	FcPatternDestroy( pattern) ;

	if( cairo)
	{
		cairo_font_options_destroy( options) ;
		cairo_destroy( cairo) ;
	}

	return  ret ;
}

static int
search_next_font(
	x_font_t *  font ,
	int  ch
	)
{
	int  count ;

	if( ! font->compl_fonts)
	{
		return  -1 ;
	}

	for( count = 0 ; font->compl_fonts[count].next ; count++)
	{
		if( FcCharSetHasChar( font->compl_fonts[count + 1].charset , ch))
		{
			return  count ;
		}
	}

	if( cairo_font_open( font , count + 1 , font->pattern , ch))
	{
		return  count ;
	}
	else
	{
		/* To avoid to research it. */
		FcCharSetAddChar( font->compl_fonts[0].charset , ch) ;

		return  -1 ;
	}
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

	xfont = font->cairo_font ;

	if( font->compl_fonts)
	{
		u_int  count ;

		for( count = 0 ; count < len ; count++)
		{
			if( ! FcCharSetHasChar( font->compl_fonts[0].charset , str[count]))
			{
				int  compl_idx ;

				if( ( compl_idx = search_next_font( font , str[count])) >= 0)
				{
					FcChar32 *  substr ;

					if( count > 0)
					{
						x += draw_string32( win , xfont , font , fg_color ,
							x + font->x_off , y , str , count) ;
					}

					substr = str + count ;

					for( count ++ ; count < len &&
					       ! FcCharSetHasChar( font->compl_fonts[0].charset ,
							str[count]) &&
					       FcCharSetHasChar(
							font->compl_fonts[compl_idx+1].charset ,
							str[count]) ;
					       count ++) ;

					x += draw_string32( win ,
						font->compl_fonts[compl_idx].next , font ,
						fg_color , x + font->x_off , y ,
						substr , substr - str + count) ;
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

void
cairo_set_clip(
	x_window_t *  win ,
	int  x ,
	int  y ,
	u_int  width ,
	u_int  height
	)
{
	cairo_rectangle( win->cairo_draw , x , y , width , height) ;
	cairo_clip( win->cairo_draw) ;
}

void
cairo_unset_clip(
	x_window_t *  win
	)
{
	cairo_reset_clip( win->cairo_draw) ;
}
