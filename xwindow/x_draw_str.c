
/*
 *	$Id$
 */

#include  "x_draw_str.h"

#if  0
#define  PERF_DEBUG
#endif


/* --- static functions --- */

/*
 * Not used for now.
 */
#if  0
static int
char_combining_is_supported(
	mkf_charset_t  cs
	)
{
	/*
	 * some thai fonts (e.g. -thai-fixed-medium-r-normal--14-100-100-100-m-70-tis620.2529-1
	 * distributed by ZzzThai(http://zzzthai.fedu.uec.ac.jp/ZzzThai/))
	 * automatically draws combining chars ...
	 */
	if( cs == TIS620_2533)
	{
		return  1 ;
	}
	else
	{
		return  0 ;
	}
}
#else
#define  char_combining_is_supported(cs)  0
#endif


/*
 * drawing string
 */

#ifdef  USE_TYPE_XFT

static int
xft_draw_combining_chars(
	x_window_t *  window ,
	x_font_manager_t *  font_man ,
	x_color_manager_t *  color_man ,
	ml_char_t *  chars ,
	u_int  size ,
	int  x ,
	int  y
	)
{
	int  count ;
	u_char *  ch_bytes ;
	size_t  ch_size ;
	mkf_charset_t  ch_cs ;

	for( count = 0 ; count < size ; count ++)
	{
		ch_bytes = ml_char_bytes( &chars[count]) ;
		ch_size = ml_char_size( &chars[count]) ;
		ch_cs = ml_char_cs( &chars[count]) ;

		if( ch_cs == DEC_SPECIAL)
		{
			x_window_draw_decsp_string( window ,
				x_get_font( font_man , ml_char_font( &chars[count])) ,
				x_get_xcolor( color_man , ml_char_fg_color( &chars[count])) ,
				NULL , x , y , ch_bytes , ch_size) ;
		}
		else if( ch_cs == US_ASCII || ch_cs == ISO8859_1_R)
		{
			x_window_xft_draw_string8( window ,
				x_get_font( font_man , ml_char_font( &chars[count])) ,
				x_get_xcolor( color_man , ml_char_fg_color( &chars[count])) ,
				x , y , ch_bytes , ch_size) ;
		}
		else
		{
			XftChar32  xch ;

			char  ucs4_bytes[4] ;

			if( ! ml_convert_to_xft_ucs4( ucs4_bytes , ch_bytes , ch_size , ch_cs))
			{
				return  0 ;
			}

			xch = ((ucs4_bytes[0] << 24) & 0xff000000) |
				((ucs4_bytes[1] << 16) & 0xff0000) |
				((ucs4_bytes[2] << 8) & 0xff00) |
				(ucs4_bytes[3] & 0xff) ;

			x_window_xft_draw_string32( window ,
				x_get_font( font_man , ml_char_font( &chars[count])) ,
				x_get_xcolor( color_man , ml_char_fg_color( &chars[count])) ,
				x , y , &xch , 1) ;
		}
	}

	return  1 ;
}

static int
xft_draw_str(
	x_window_t *  window ,
	x_font_manager_t *  font_man ,
	x_color_manager_t *  color_man ,
	u_int *  updated_width ,
	ml_char_t *  chars ,
	u_int  num_of_chars ,
	int  x ,
	int  y ,
	u_int  height ,
	u_int  std_height_to_baseline ,
	u_int  top_margin ,
	u_int  bottom_margin
	)
{
	int  count ;
	int  start_draw ;
	int  end_of_str ;
	u_int  height_to_baseline ;
	u_int	current_width ;
	XftChar8 *  str8 ;
	XftChar32 *  str32 ;
	u_int	str_len ;
	int  state ;
	int  next_state ;

	u_char *  ch_bytes ;
	size_t  ch_size ;
	u_int  ch_width ;
	mkf_charset_t  ch_cs ;
	x_font_t *  xfont ;
	ml_color_t  fg_color ;
	ml_color_t  bg_color ;
	int  is_underlined ;
	ml_char_t *  comb_chars ;
	u_int  comb_size ;

	x_font_t *  next_xfont ;
	ml_color_t  next_fg_color ;
	ml_color_t  next_bg_color ;
	int  next_is_underlined ;
	u_int  next_ch_width ;

#ifdef  PERF_DEBUG
	int  draw_count = 0 ;
#endif

	if( x > window->width || y + height > window->height)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " drawing string in overflowed area.(x %d y %d h %d)\n" ,
			x , y , height) ;
	#endif

		return  0 ;
	}

	if( num_of_chars == 0)
	{
	#ifdef	__DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " input chars length is 0(x_window_draw_str).\n") ;
	#endif

		return	1 ;
	}

	start_draw = 0 ;
	end_of_str = 0 ;

	x = x ;
	y = y ;

	count = 0 ;

	ch_bytes = ml_char_bytes( &chars[count]) ;
	ch_size = ml_char_size( &chars[count]) ;
	ch_cs = ml_char_cs( &chars[count]) ;

	if( ch_cs == US_ASCII || ch_cs == ISO8859_1_R)
	{
		state = 0 ;
	}
	else if( ch_cs == DEC_SPECIAL)
	{
		state = 1 ;
	}
	else
	{
		state = 2 ;
	}

	xfont = x_get_font( font_man , ml_char_font( &chars[count])) ;

	ch_width = x_calculate_char_width( xfont , ch_bytes , ch_size , ch_cs) ;
	current_width = x + ch_width ;

	fg_color = ml_char_fg_color( &chars[count]) ;
	bg_color = ml_char_bg_color( &chars[count]) ;

	is_underlined = ml_char_is_underlined( &chars[count]) ;

	if( ( str8 = alloca( sizeof( XftChar8) * num_of_chars)) == NULL)
	{
		return	0 ;
	}

	if( ( str32 = alloca( sizeof( XftChar32) * num_of_chars)) == NULL)
	{
		return  0 ;
	}

	str_len = 0 ;

	while( 1)
	{
		if( state == 0)
		{
			str8[str_len++] = ch_bytes[0] ;
		}
		else if( state == 1)
		{
			str8[str_len++] = ch_bytes[0] ;
		}
		else /* if( state == 2) */
		{
			char  ucs4_bytes[4] ;

			if( ! ml_convert_to_xft_ucs4( ucs4_bytes , ch_bytes , ch_size , ch_cs))
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " strange character , ignored.\n") ;
			#endif

				ucs4_bytes[0] = 0 ;
				ucs4_bytes[1] = 0 ;
				ucs4_bytes[2] = 0 ;
				ucs4_bytes[3] = 0x20 ;
			}

			str32[str_len++] = ((ucs4_bytes[0] << 24) & 0xff000000) |
				((ucs4_bytes[1] << 16) & 0xff0000) | ((ucs4_bytes[2] << 8) & 0xff00) |
				(ucs4_bytes[3] & 0xff) ;
		}

		comb_chars = ml_get_combining_chars( &chars[count] , &comb_size) ;

		/*
		 * next character.
		 */

		count ++ ;

		if( count >= num_of_chars)
		{
			start_draw = 1 ;
			end_of_str = 1 ;
		}
		else
		{
			ch_bytes = ml_char_bytes( &chars[count]) ;
			ch_size = ml_char_size( &chars[count]) ;
			ch_cs = ml_char_cs( &chars[count]) ;
			next_fg_color = ml_char_fg_color( &chars[count]) ;
			next_bg_color = ml_char_bg_color( &chars[count]) ;
			next_is_underlined = ml_char_is_underlined( &chars[count]) ;
			next_xfont = x_get_font( font_man , ml_char_font( &chars[count])) ;

			if( ch_cs == US_ASCII || ch_cs == ISO8859_1_R)
			{
				next_state = 0 ;
			}
			else if( ch_cs == DEC_SPECIAL)
			{
				next_state = 1 ;
			}
			else
			{
				next_state = 2 ;
			}

			next_ch_width = x_calculate_char_width( next_xfont , ch_bytes , ch_size , ch_cs) ;

			if( current_width + next_ch_width > window->width)
			{
				start_draw = 1 ;
				end_of_str = 1 ;
			}
			/*
			 * !! Notice !!
			 * next_xfont != xfont doesn't necessarily detect change of 'state'
			 * (for example, same Unicode font is used for both US_ASCII/ISO8859_1
			 * and other half-width unicode characters), 'next_state' is necessary.
			 */
			else if( next_xfont != xfont
				|| next_fg_color != fg_color
				|| next_bg_color != bg_color
				|| next_is_underlined != is_underlined
				|| (is_underlined && xfont->is_vertical)
				|| (next_is_underlined && xfont->is_vertical)
				|| comb_chars != NULL
				|| state != next_state
				|| (next_xfont->is_proportional && ! next_xfont->is_var_col_width)
				|| (xfont->is_proportional && ! xfont->is_var_col_width))
			{
				start_draw = 1 ;
			}
			else
			{
				start_draw = 0 ;
			}
		}

		if( start_draw)
		{
			/*
			 * status is changed.
			 */

		#ifdef  PERF_DEBUG
			draw_count ++ ;
		#endif

			if( height == xfont->height)
			{
				height_to_baseline = xfont->height_to_baseline ;
			}
			else
			{
				height_to_baseline = std_height_to_baseline ;
			}

			/*
			 * clearing background
			 */
			if( window->wall_picture_is_set && bg_color == ML_BG_COLOR)
			{
				x_window_clear( window ,
					x , y , current_width - x , height) ;
			}
			else
			{
				x_window_fill_with( window ,
					x_get_xcolor( color_man , bg_color)->pixel ,
					x , y , current_width - x , height) ;
			}

			/*
			 * drawing string
			 */
			if( state == 0)
			{
				x_window_xft_draw_string8( window ,
					xfont , x_get_xcolor( color_man , fg_color) ,
					x , y + height_to_baseline , str8 , str_len) ;
			}
			else if( state == 1)
			{
				x_window_draw_decsp_string( window , xfont ,
					x_get_xcolor( color_man , fg_color) , NULL ,
					x , y + height_to_baseline , str8 , str_len) ;
			}
			else /* if( state == 2) */
			{
				x_window_xft_draw_string32( window ,
					xfont , x_get_xcolor( color_man , fg_color) ,
					x , y + height_to_baseline , str32 , str_len) ;
			}

			if( comb_chars)
			{
				xft_draw_combining_chars( window , font_man , color_man , comb_chars , comb_size ,
					current_width - ch_width , y + height_to_baseline) ;
			}

			if( is_underlined)
			{
				if( xfont->is_vertical)
				{
					x_window_fill_with( window ,
						x_get_xcolor( color_man , fg_color)->pixel ,
						x , y , (ch_width>>4) + 1 , height) ;
				}
				else
				{
					x_window_fill_with( window ,
						x_get_xcolor( color_man , fg_color)->pixel ,
						x , y + height_to_baseline ,
						current_width - x , ((height_to_baseline - bottom_margin)>>4) +1 ) ;
				}
			}

			start_draw = 0 ;

			x = current_width ;
			str_len = 0 ;
		}

		if( end_of_str)
		{
			break ;
		}

		is_underlined = next_is_underlined ;
		xfont = next_xfont ;
		fg_color = next_fg_color ;
		bg_color = next_bg_color ;
		state = next_state ;
		current_width += (ch_width = next_ch_width) ;
	}

	if( updated_width != NULL)
	{
		*updated_width = current_width ;
	}

#ifdef  PERF_DEBUG
	kik_debug_printf( " drawing %d times in a line.\n" , draw_count) ;
#endif

	return	1 ;
}

#endif

#ifdef  USE_TYPE_XCORE

static int
xcore_draw_combining_chars(
	x_window_t *  window ,
	x_font_manager_t *  font_man ,
	x_color_manager_t *  color_man ,
	ml_char_t *  chars ,
	u_int  size ,
	int  x ,
	int  y
	)
{
	int  count ;
	u_char *  ch_bytes ;
	size_t  ch_size ;

	for( count = 0 ; count < size ; count ++)
	{
		ch_bytes = ml_char_bytes( &chars[count]) ;
		ch_size = ml_char_size( &chars[count]) ;

		if( ml_char_cs( &chars[count]) == DEC_SPECIAL)
		{
			x_window_draw_decsp_string( window ,
				x_get_font( font_man , ml_char_font( &chars[count])) ,
				x_get_xcolor( color_man , ml_char_fg_color( &chars[count])) ,
				NULL , x , y , ch_bytes , 1) ;
		}
		else if( ch_size == 1)
		{
			x_window_draw_string( window ,
				x_get_font( font_man , ml_char_font( &chars[count])) ,
				x_get_xcolor( color_man , ml_char_fg_color( &chars[count])) ,
				x , y , ch_bytes , 1) ;
		}
		else if( ch_size == 2)
		{
			XChar2b  xch ;

			xch.byte1 = ch_bytes[0] ;
			xch.byte2 = ch_bytes[1] ;

			x_window_draw_string16( window ,
				x_get_font( font_man , ml_char_font( &chars[count])) ,
				x_get_xcolor( color_man , ml_char_fg_color( &chars[count])) ,
				x , y , &xch , 1) ;
		}
		else if( ch_size == 4 && ch_bytes[0] == '\0' && ch_bytes[1] == '\0')
		{
			/* UCS4 */

			XChar2b  xch ;

			xch.byte1 = ch_bytes[2] ;
			xch.byte2 = ch_bytes[3] ;

			x_window_draw_string16( window ,
				x_get_font( font_man , ml_char_font( &chars[count])) ,
				x_get_xcolor( color_man , ml_char_fg_color( &chars[count])) ,
				x , y , &xch , 1) ;
		}
	#ifdef  DEBUG
		else
		{
			kik_warn_printf( KIK_DEBUG_TAG " strange character , ignored.\n") ;
		}
	#endif
	}

	return  1 ;
}

static int
xcore_draw_str(
	x_window_t *  window ,
	x_font_manager_t *  font_man ,
	x_color_manager_t *  color_man ,
	u_int *  updated_width ,
	ml_char_t *  chars ,
	u_int  num_of_chars ,
	int  x ,
	int  y ,
	u_int  height ,
	u_int  std_height_to_baseline ,
	u_int  top_margin ,
	u_int  bottom_margin
	)
{
	int  count ;
	int  start_draw ;
	int  end_of_str ;
	u_int  height_to_baseline ;
	u_int	current_width ;
	u_char *  str ;
	XChar2b *  str2b ;
	u_int	str_len ;
	int  state ;			/* 0(8bit),1(decsp),2(16bit) */
	int  next_state ;
	ml_char_t *  comb_chars ;
	u_int  comb_size ;

	u_char *  ch_bytes ;
	size_t  ch_size ;
	mkf_charset_t  ch_cs ;
	u_int  ch_width ;
	x_font_t *  xfont ;
	ml_color_t  fg_color ;
	ml_color_t  bg_color ;
	int  is_underlined ;

	u_int  next_ch_width ;
	x_font_t *  next_xfont ;
	ml_color_t  next_fg_color ;
	ml_color_t  next_bg_color ;
	int  next_is_underlined ;

#ifdef  PERF_DEBUG
	int  draw_count = 0 ;
#endif

	if( x > window->width || y + height > window->height)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " drawing string in overflowed area.(x %d y %d h %d)\n" ,
			x , y , height) ;
	#endif

		return  0 ;
	}

	if( num_of_chars == 0)
	{
	#ifdef	__DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " input chars length is 0(x_window_draw_str).\n") ;
	#endif

		return	1 ;
	}

	count = 0 ;

	start_draw = 0 ;
	end_of_str = 0 ;

	x = x ;
	y = y ;

	ch_bytes = ml_char_bytes( &chars[count]) ;
	ch_size = ml_char_size( &chars[count]) ;
	ch_cs = ml_char_cs( &chars[count]) ;

	if( ch_cs == DEC_SPECIAL)
	{
		state = 1 ;
	}
	else if( ch_size == 1)
	{
		state = 0 ;
	}
	else /* if( ch_size == 2 || ch_size == 4) */
	{
		state = 2 ;
	}

	xfont = x_get_font( font_man , ml_char_font( &chars[count])) ;

	ch_width = x_calculate_char_width( xfont , ch_bytes , ch_size , ch_cs) ;
	current_width = x + ch_width ;

	fg_color = ml_char_fg_color( &chars[count]) ;
	bg_color = ml_char_bg_color( &chars[count]) ;
	is_underlined = ml_char_is_underlined( &chars[count]) ;

	if( ( str = alloca( sizeof( char) * num_of_chars)) == NULL)
	{
		return	0 ;
	}

	if( ( str2b = alloca( sizeof( XChar2b) * num_of_chars)) == NULL)
	{
		return	0 ;
	}

	str_len = 0 ;

	while( 1)
	{
		if( ch_size == 1)
		{
			str[str_len++] = ch_bytes[0] ;
		}
		else if( ch_size == 2)
		{
			str2b[str_len].byte1 = ch_bytes[0] ;
			str2b[str_len].byte2 = ch_bytes[1] ;
			str_len ++ ;
		}
		else if( ch_size == 4 && ch_bytes[0] == '\0' && ch_bytes[1] == '\0')
		{
			/* UCS4 */

			str2b[str_len].byte1 = ch_bytes[2] ;
			str2b[str_len].byte2 = ch_bytes[3] ;
			str_len ++ ;
		}
		else
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " strange character , ignored.\n") ;
		#endif

			if( state)
			{
				str2b[str_len].byte1 = '\x20' ;
				str2b[str_len].byte2 = '\x20' ;
				str_len ++ ;
			}
			else /* if( ch_size == 1) */
			{
				str[str_len++] = '\x20' ;
			}
		}

		comb_chars = ml_get_combining_chars( &chars[count] , &comb_size) ;

		/*
		 * next character.
		 */

		count ++ ;

		if( count >= num_of_chars)
		{
			start_draw = 1 ;
			end_of_str = 1 ;
		}
		else
		{
			ch_bytes = ml_char_bytes( &chars[count]) ;
			ch_size = ml_char_size( &chars[count]) ;
			ch_cs = ml_char_cs( &chars[count]) ;
			next_fg_color = ml_char_fg_color( &chars[count]) ;
			next_bg_color = ml_char_bg_color( &chars[count]) ;
			next_is_underlined = ml_char_is_underlined( &chars[count]) ;
			next_xfont = x_get_font( font_man ,  ml_char_font( &chars[count])) ;

			if( ch_cs == DEC_SPECIAL)
			{
				next_state = 1 ;
			}
			else if( ch_size == 1)
			{
				next_state = 0 ;
			}
			else /* if( ch_size == 2 || ch_size == 4) */
			{
				next_state = 2 ;
			}

			next_ch_width = x_calculate_char_width( next_xfont , ch_bytes , ch_size , ch_cs) ;

			if( current_width + next_ch_width > window->width)
			{
				start_draw = 1 ;
				end_of_str = 1 ;
			}
			/*
			 * !! Notice !!
			 * next_xfont != xfont doesn't necessarily detect change of 'state'
			 * (for example, same Unicode font is used for both US_ASCII/ISO8859_1
			 * and other half-width unicode characters), 'next_state' is necessary.
			 */
			else if( next_xfont != xfont
				|| next_fg_color != fg_color
				|| next_bg_color != bg_color
				|| next_is_underlined != is_underlined
				|| (is_underlined && xfont->is_vertical)
				|| (next_is_underlined && xfont->is_vertical)
				|| next_state != state
				|| comb_chars != NULL
				|| (next_xfont->is_proportional && ! next_xfont->is_var_col_width)
				|| (xfont->is_proportional && ! xfont->is_var_col_width))
			{
				start_draw = 1 ;
			}
			else
			{
				start_draw = 0 ;
			}
		}

		if( start_draw)
		{
			/*
			 * status is changed.
			 */

		#ifdef  PERF_DEBUG
			draw_count ++ ;
		#endif

			if( xfont->height == height)
			{
				height_to_baseline = xfont->height_to_baseline ;
			}
			else
			{
				height_to_baseline = std_height_to_baseline ;
			}

			if( ( window->wall_picture_is_set && bg_color == ML_BG_COLOR) ||
				bottom_margin + top_margin > 0 /* == line space XXX */|| 
				( xfont->is_proportional && ! xfont->is_var_col_width))
			{
				if( window->wall_picture_is_set && bg_color == ML_BG_COLOR)
				{
					x_window_clear( window ,
						x , y , current_width - x , height) ;
				}
				else
				{
				#ifdef  __DEBUG
					kik_debug_printf( KIK_DEBUG_TAG "prop font is used.\n") ;
				#endif

					x_window_fill_with( window ,
						x_get_xcolor( color_man , bg_color)->pixel ,
						x , y , current_width - x , height) ;
				}

				if( state == 2)
				{
					x_window_draw_string16( window , xfont ,
						x_get_xcolor( color_man , fg_color) ,
						x , y + height_to_baseline , str2b , str_len) ;
				}
				else if( state == 1)
				{
					x_window_draw_decsp_string( window , xfont ,
						x_get_xcolor( color_man , fg_color) , NULL ,
						x , y + height_to_baseline , str , str_len) ;
				}
				else /* if( state == 0) */
				{
					x_window_draw_string( window , xfont ,
						x_get_xcolor( color_man , fg_color) ,
						x , y + height_to_baseline , str , str_len) ;
				}
			}
			else
			{
				if( state == 2)
				{
					x_window_draw_image_string16( window , xfont ,
						x_get_xcolor( color_man , fg_color) ,
						x_get_xcolor( color_man , bg_color) ,
						x , y + height_to_baseline , str2b , str_len) ;
				}
				else if( state == 1)
				{
					x_window_draw_decsp_string( window , xfont ,
						x_get_xcolor( color_man , fg_color) ,
						x_get_xcolor( color_man , bg_color) ,
						x , y + height_to_baseline , str , str_len) ;
				}
				else /* if( state == 0) */
				{
					x_window_draw_image_string( window , xfont ,
						x_get_xcolor( color_man , fg_color) ,
						x_get_xcolor( color_man , bg_color) ,
						x , y + height_to_baseline , str , str_len) ;
				}
			}

			if( comb_chars)
			{
				xcore_draw_combining_chars( window , font_man , color_man , comb_chars , comb_size ,
					current_width - ch_width , y + height_to_baseline) ;
			}

			if( is_underlined)
			{
				if( xfont->is_vertical)
				{
					x_window_fill_with( window ,
						x_get_xcolor( color_man , fg_color)->pixel ,
						x , y , (ch_width>>4) + 1 , height) ;
				}
				else
				{
					x_window_fill_with( window ,
						x_get_xcolor( color_man , fg_color)->pixel ,
						x , y + height_to_baseline,
						current_width - x , ((height_to_baseline - bottom_margin)>>4) +1 ) ;
				}
			}

			start_draw = 0 ;

			x = current_width ;
			str_len = 0 ;
		}

		if( end_of_str)
		{
			break ;
		}

		xfont = next_xfont ;
		fg_color = next_fg_color ;
		bg_color = next_bg_color ;
		is_underlined = next_is_underlined ;
		state = next_state ;
		current_width += (ch_width = next_ch_width) ;
	}

	if( updated_width != NULL)
	{
		*updated_width = current_width ;
	}

#ifdef  PERF_DEBUG
	kik_debug_printf( " drawing %d times in a line.\n" , draw_count) ;
#endif

	return	1 ;
}

#endif

/* --- global functions --- */

int
x_draw_str(
	x_window_t *  window ,
	x_font_manager_t *  font_man ,
	x_color_manager_t *  color_man ,
	ml_char_t *  chars ,
	u_int	num_of_chars ,
	int  x ,
	int  y ,
	u_int  height ,
	u_int  height_to_baseline ,
	u_int  top_margin ,
	u_int  bottom_margin
	)
{
	u_int  updated_width ;

	switch( x_get_type_engine( font_man))
	{
	default:
		return  0 ;

#ifdef  USE_TYPE_XFT
	case  TYPE_XFT:
		if( ! xft_draw_str( window , font_man , color_man , &updated_width , chars , num_of_chars ,
			x , y , height , height_to_baseline , top_margin , bottom_margin))
		{
			return  0 ;
		}

		break ;
#endif

#ifdef  USE_TYPE_XCORE
	case  TYPE_XCORE:
		if( ! xcore_draw_str( window , font_man , color_man , &updated_width , chars , num_of_chars ,
			x , y , height , height_to_baseline , top_margin , bottom_margin))
		{
			return  0 ;
		}

		break ;
#endif
	}
	
	return  1 ;
}

int
x_draw_str_to_eol(
	x_window_t *  window ,
	x_font_manager_t *  font_man ,
	x_color_manager_t *  color_man ,
	ml_char_t *  chars ,
	u_int  num_of_chars ,
	int  x ,
	int  y ,
	u_int  height ,
	u_int  height_to_baseline ,
	u_int  top_margin ,
	u_int  bottom_margin
	)
{
	u_int  updated_width ;

	switch( x_get_type_engine( font_man))
	{
	default:
		return  0 ;

#ifdef  USE_TYPE_XFT
	case  TYPE_XFT:
		if( ! xft_draw_str( window , font_man , color_man , &updated_width , chars , num_of_chars ,
			x , y , height , height_to_baseline , top_margin , bottom_margin))
		{
			return  0 ;
		}

		break ;
#endif

#ifdef  USE_TYPE_XCORE
	case  TYPE_XCORE:
		if( ! xcore_draw_str( window , font_man , color_man , &updated_width , chars , num_of_chars ,
			x , y , height , height_to_baseline , top_margin , bottom_margin))
		{
			return	0 ;
		}

		break ;
#endif
	}

	if( updated_width < window->width)
	{
		x_window_clear( window , updated_width , y ,
			window->width - updated_width , height) ;
	}

	return	1 ;
}

