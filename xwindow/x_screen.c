/*
 *	$Id$
 */

#include  "x_screen.h"

#include  <signal.h>
#include  <stdio.h>		/* sprintf */
#include  <unistd.h>            /* getcwd */
#include  <limits.h>            /* PATH_MAX */
#include  <X11/keysym.h>	/* XK_xxx */
#include  <kiklib/kik_mem.h>	/* alloca */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_util.h>	/* K_MIN */
#include  <kiklib/kik_locale.h>	/* kik_get_locale */
#include  <mkf/mkf_xct_parser.h>
#include  <mkf/mkf_xct_conv.h>
#include  <mkf/mkf_utf8_conv.h>
#include  <mkf/mkf_utf8_parser.h>

#include  "ml_str_parser.h"
#include  "x_xic.h"
#include  "x_picture.h"


/*
 * XXX
 * char length is max 8 bytes.
 * I think this is enough , but I'm not sure.
 */
#define  UTF8_MAX_CHAR_SIZE  (8 * (MAX_COMB_SIZE + 1))

/*
 * XXX
 *
 * char prefixes are max 4 bytes.
 * additional 3 bytes + cs name len ("viscii1.1-1" is max 11 bytes) = 14 bytes for iso2022
 * extension.
 * char length is max 2 bytes.
 * (total 20 bytes)
 *
 * combining chars is max 3 per char.
 *
 * I think this is enough , but I'm not sure.
 */
#define  XCT_MAX_CHAR_SIZE  (20 * 4)

/* the same as rxvt. if this size is too few , we may drop sequences from kinput2. */
#define  KEY_BUF_SIZE  512

#define  HAS_SYSTEM_LISTENER(screen,function) \
	((screen)->system_listener && (screen)->system_listener->function)

#define  HAS_SCROLL_LISTENER(screen,function) \
	((screen)->screen_scroll_listener && (screen)->screen_scroll_listener->function)


#if  0
#define  __DEBUG
#endif

#if  0
#define  PERF_DEBUG
#endif

#if  1
#define  NL_TO_CR_IN_PAST_TEXT
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
 
#ifdef  ANTI_ALIAS

static int
xft_draw_combining_chars(
	x_screen_t *  screen ,
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
			x_window_draw_decsp_string( &screen->window ,
				x_get_font( screen->font_man , ml_char_font( &chars[count])) ,
				x_get_color( screen->color_man , ml_char_fg_color( &chars[count])) ,
				NULL , x , y , ch_bytes , ch_size) ;
		}
		else if( ch_cs == US_ASCII || ch_cs == ISO8859_1_R)
		{
			x_window_xft_draw_string8( &screen->window ,
				x_get_font( screen->font_man , ml_char_font( &chars[count])) ,
				x_get_color( screen->color_man , ml_char_fg_color( &chars[count])) ,
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

			x_window_xft_draw_string32( &screen->window ,
				x_get_font( screen->font_man , ml_char_font( &chars[count])) ,
				x_get_color( screen->color_man , ml_char_fg_color( &chars[count])) ,
				x , y , &xch , 1) ;
		}
	}

	return  1 ;
}

static int
xft_draw_str(
	x_screen_t *  screen ,
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
	
#ifdef  PERF_DEBUG
	int  draw_count = 0 ;
#endif

	if( x > screen->window.width || y + height > screen->window.height)
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

	xfont = x_get_font( screen->font_man , ml_char_font( &chars[count])) ;

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
			next_xfont = x_get_font( screen->font_man , ml_char_font( &chars[count])) ;
			
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
			
			ch_width = x_calculate_char_width( next_xfont , ch_bytes , ch_size , ch_cs) ;
			
			if( current_width + ch_width > screen->window.width)
			{
				start_draw = 1 ;
				end_of_str = 1 ;
			}
			/*
			 * !! Notice !!
			 * next_xfont != xfont doen't necessarily detect change of 'state'
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
			if( screen->window.wall_picture_is_set && bg_color == ML_BG_COLOR)
			{
				x_window_clear( &screen->window ,
					x , y , current_width - x , height) ;
			}
			else
			{
				x_window_set_fg_color( &screen->window ,
					x_get_color( screen->color_man , bg_color)->pixel) ;
				x_window_fill( &screen->window , x , y , current_width - x , height) ;
			}
			
			/*
			 * drawing string
			 */
			if( state == 0)
			{
				x_window_xft_draw_string8( &screen->window ,
					xfont , x_get_color( screen->color_man , fg_color) ,
					x , y + height_to_baseline , str8 , str_len) ;
			}
			else if( state == 1)
			{
				x_window_draw_decsp_string( &screen->window , xfont ,
					x_get_color( screen->color_man , fg_color) , NULL ,
					x , y + height_to_baseline , str8 , str_len) ;
			}
			else /* if( state == 2) */
			{
				x_window_xft_draw_string32( &screen->window ,
					xfont , x_get_color( screen->color_man , fg_color) ,
					x , y + height_to_baseline , str32 , str_len) ;
			}

			if( comb_chars)
			{
				xft_draw_combining_chars( screen , comb_chars , comb_size ,
					current_width - ch_width , y + height_to_baseline) ;
			}
			
			if( is_underlined)
			{
				if( xfont->is_vertical)
				{
					x_window_set_fg_color( &screen->window ,
						x_get_color( screen->color_man , fg_color)->pixel) ;
					x_window_fill( &screen->window , x , y , (ch_width>>4) + 1 , height) ;
				}
				else
				{
					x_window_set_fg_color( &screen->window ,
						x_get_color( screen->color_man , fg_color)->pixel) ;
					x_window_fill( &screen->window ,
						x , y + height_to_baseline - bottom_margin + 1, current_width - x , (height_to_baseline>>4) +1 ) ;
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
		current_width += ch_width ;
	}

	x_window_set_fg_color( &screen->window , x_get_color( screen->color_man , ML_FG_COLOR)->pixel) ;

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

static int
x_draw_combining_chars(
	x_screen_t *  screen ,
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
			x_window_draw_decsp_string( &screen->window ,
				x_get_font( screen->font_man , ml_char_font( &chars[count])) ,
				x_get_color( screen->color_man , ml_char_fg_color( &chars[count])) ,
				NULL , x , y , ch_bytes , 1) ;
		}
		else if( ch_size == 1)
		{
			x_window_draw_string( &screen->window ,
				x_get_font( screen->font_man , ml_char_font( &chars[count])) ,
				x_get_color( screen->color_man , ml_char_fg_color( &chars[count])) ,
				x , y , ch_bytes , 1) ;
		}
		else if( ch_size == 2)
		{
			XChar2b  xch ;

			xch.byte1 = ch_bytes[0] ;
			xch.byte2 = ch_bytes[1] ;

			x_window_draw_string16( &screen->window ,
				x_get_font( screen->font_man , ml_char_font( &chars[count])) ,
				x_get_color( screen->color_man , ml_char_fg_color( &chars[count])) ,
				x , y , &xch , 1) ;
		}
		else if( ch_size == 4 && ch_bytes[0] == '\0' && ch_bytes[1] == '\0')
		{
			/* UCS4 */
			
			XChar2b  xch ;

			xch.byte1 = ch_bytes[2] ;
			xch.byte2 = ch_bytes[3] ;
			
			x_window_draw_string16( &screen->window ,
				x_get_font( screen->font_man , ml_char_font( &chars[count])) ,
				x_get_color( screen->color_man , ml_char_fg_color( &chars[count])) ,
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
x_draw_str(
	x_screen_t *  screen ,
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

	x_font_t *  next_xfont ;
	ml_color_t  next_fg_color ;
	ml_color_t  next_bg_color ;
	int  next_is_underlined ;

#ifdef  PERF_DEBUG
	int  draw_count = 0 ;
#endif

	if( x > screen->window.width || y + height > screen->window.height)
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
	
	xfont = x_get_font( screen->font_man , ml_char_font( &chars[count])) ;
	
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
			next_xfont = x_get_font( screen->font_man ,  ml_char_font( &chars[count])) ;

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
			
			ch_width = x_calculate_char_width( next_xfont , ch_bytes , ch_size , ch_cs) ;
			
			if( current_width + ch_width > screen->window.width)
			{
				start_draw = 1 ;
				end_of_str = 1 ;
			}
			/*
			 * !! Notice !!
			 * next_xfont != xfont doen't necessarily detect change of 'state'
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

			if( ( screen->window.wall_picture_is_set && bg_color == ML_BG_COLOR) ||
				( xfont->is_proportional && ! xfont->is_var_col_width))
			{
				if( screen->window.wall_picture_is_set && bg_color == ML_BG_COLOR)
				{
					x_window_clear( &screen->window ,
						x , y , current_width - x , height) ;
				}
				else
				{
				#ifdef  __DEBUG
					kik_debug_printf( KIK_DEBUG_TAG "prop font is used.\n") ;
				#endif

					x_window_set_fg_color( &screen->window ,
						x_get_color( screen->color_man , bg_color)->pixel) ;
					x_window_fill( &screen->window ,
						x , y , current_width - x , height) ;
				}

				if( state == 2)
				{
					x_window_draw_string16( &screen->window , xfont ,
						x_get_color( screen->color_man , fg_color) ,
						x , y + height_to_baseline , str2b , str_len) ;
				}
				else if( state == 1)
				{
					x_window_draw_decsp_string( &screen->window , xfont ,
						x_get_color( screen->color_man , fg_color) , NULL ,
						x , y + height_to_baseline , str , str_len) ;
				}
				else /* if( state == 0) */
				{
					x_window_draw_string( &screen->window , xfont ,
						x_get_color( screen->color_man , fg_color) ,
						x , y + height_to_baseline , str , str_len) ;
				}
			}
			else
			{
				if( state == 2)
				{
					x_window_draw_image_string16( &screen->window , xfont ,
						x_get_color( screen->color_man , fg_color) ,
						x_get_color( screen->color_man , bg_color) ,
						x , y + height_to_baseline , str2b , str_len) ;
				}
				else if( state == 1)
				{
					x_window_draw_decsp_string( &screen->window , xfont ,
						x_get_color( screen->color_man , fg_color) ,
						x_get_color( screen->color_man , bg_color) ,
						x , y + height_to_baseline , str , str_len) ;
				}
				else /* if( state == 0) */
				{
					x_window_draw_image_string( &screen->window , xfont ,
						x_get_color( screen->color_man , fg_color) ,
						x_get_color( screen->color_man , bg_color) ,
						x , y + height_to_baseline , str , str_len) ;
				}
			}
			
			if( comb_chars)
			{
				x_draw_combining_chars( screen , comb_chars , comb_size ,
					current_width - ch_width , y + height_to_baseline) ;
			}

			if( is_underlined)
			{
				if( xfont->is_vertical)
				{
					x_window_set_fg_color( &screen->window ,
						x_get_color( screen->color_man , fg_color)->pixel) ;
					x_window_fill( &screen->window , x , y , (ch_width>>4) + 1 , height) ;
				}
				else
				{
					x_window_set_fg_color( &screen->window ,
						x_get_color( screen->color_man , fg_color)->pixel) ;
					x_window_fill( &screen->window ,
						x , y + height_to_baseline - bottom_margin + 1, current_width - x , (height_to_baseline>>4) + 1) ;
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
		current_width += ch_width ;
	}

	x_window_set_fg_color( &screen->window , x_get_color( screen->color_man , ML_FG_COLOR)->pixel) ;
	
	if( updated_width != NULL)
	{
		*updated_width = current_width ;
	}

#ifdef  PERF_DEBUG
	kik_debug_printf( " drawing %d times in a line.\n" , draw_count) ;
#endif

	return	1 ;
}

static int
draw_str(
	x_screen_t *  screen ,
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

#ifdef  ANTI_ALIAS
	if( x_font_manager_get_font_present( screen->font_man) & FONT_AA)
	{
		if( ! xft_draw_str( screen , &updated_width , chars , num_of_chars ,
			x , y , height , height_to_baseline , top_margin , bottom_margin))
		{
			return  0 ;
		}
	}
	else
#endif
	{
		if( ! x_draw_str( screen , &updated_width , chars , num_of_chars ,
			x , y , height , height_to_baseline , top_margin , bottom_margin))
		{
			return  0 ;
		}
	}

	return  1 ;
}

static int
draw_str_to_eol(
	x_screen_t *  screen ,
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

#ifdef  ANTI_ALIAS
	if( x_font_manager_get_font_present( screen->font_man) & FONT_AA)
	{
		if( ! xft_draw_str( screen , &updated_width , chars , num_of_chars ,
			x , y , height , height_to_baseline , top_margin , bottom_margin))
		{
			return  0 ;
		}
	}
	else
#endif
	{
		if( ! x_draw_str( screen , &updated_width , chars , num_of_chars ,
			x , y , height , height_to_baseline , top_margin , bottom_margin))
		{
			return	0 ;
		}
	}

	if( updated_width < screen->window.width)
	{
		x_window_clear( &screen->window , updated_width , y ,
			screen->window.width - updated_width , height) ;
	}

	return	1 ;
}

static int
convert_row_to_y(
	x_screen_t *  screen ,
	int  row
	)
{
	/*
	 * !! Notice !!
	 * assumption: line hight is always the same!
	 */

	return  x_line_height( screen) * row ;
}

static int
convert_y_to_row(
	x_screen_t *  screen ,
	u_int *  y_rest ,
	int  y
	)
{
	int  row ;

	/*
	 * !! Notice !!
	 * assumption: line hight is always the same!
	 */
	
	row = y / x_line_height( screen) ;

	if( y_rest)
	{
		*y_rest = y - row * x_line_height( screen) ;
	}

	return  row ;
}

static int
convert_char_index_to_x(
	x_screen_t *  screen ,
	ml_line_t *  line ,
	int  char_index
	)
{
	int  count ;
	int  x ;
	
	if( char_index > ml_line_end_char_index(line))
	{
		char_index = ml_line_end_char_index(line) ;
	}

	if( ml_line_is_rtl( line))
	{
		x = screen->window.width ;

		for( count = ml_line_end_char_index(line) ; count >= char_index ; count --)
		{
			x -= x_calculate_char_width(
				x_get_font( screen->font_man , ml_char_font( &line->chars[count])) ,
				ml_char_bytes( &line->chars[count]) ,
				ml_char_size( &line->chars[count]) ,
				ml_char_cs( &line->chars[count])) ;
		}
	}
	else
	{
		/*
		 * excluding the last char width.
		 */
		x = 0 ;
		for( count = 0 ; count < char_index ; count ++)
		{
			x += x_calculate_char_width(
				x_get_font( screen->font_man , ml_char_font( &line->chars[count])) ,
				ml_char_bytes( &line->chars[count]) ,
				ml_char_size( &line->chars[count]) ,
				ml_char_cs( &line->chars[count])) ;
		}
	}

	return  x ;
}

static int
convert_char_index_to_x_with_shape(
	x_screen_t *  screen ,
	ml_line_t *  line ,
	int  char_index
	)
{
	ml_line_t *  orig ;
	int  x ;

	if( screen->shape)
	{
		if( ( orig = ml_line_shape( line , screen->shape)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ml_line_shape() failed.\n") ;
		#endif
		}
	}
	else
	{
		orig = NULL ;
	}

	x = convert_char_index_to_x( screen , line , char_index) ;

	if( orig)
	{
		ml_line_unshape( line , orig) ;
	}

	return  x ;
}

static int
convert_x_to_char_index(
	x_screen_t *  screen ,
	ml_line_t *  line ,
	u_int *  x_rest ,
	int  x
	)
{
	int  count ;
	u_int  width ;

	if( ml_line_is_rtl( line))
	{
		x = screen->window.width - x ;

		for( count = ml_line_end_char_index(line) ; count > 0 ; count --)
		{
			width = x_calculate_char_width(
				x_get_font( screen->font_man , ml_char_font( &line->chars[count])) ,
				ml_char_bytes( &line->chars[count]) ,
				ml_char_size( &line->chars[count]) ,
				ml_char_cs( &line->chars[count])) ;

			if( x <= width)
			{
				break ;
			}

			x -= width ;
		}
	}
	else
	{
		for( count = 0 ; count < ml_line_end_char_index(line) ; count ++)
		{
			width = x_calculate_char_width(
				x_get_font( screen->font_man , ml_char_font( &line->chars[count])) ,
				ml_char_bytes( &line->chars[count]) ,
				ml_char_size( &line->chars[count]) ,
				ml_char_cs( &line->chars[count])) ;

			if( x < width)
			{
				break ;
			}

			x -= width ;
		}
	}

	if( x_rest != NULL)
	{
		*x_rest = x ;
	}
	
	return  count ;
}

static int
convert_x_to_char_index_with_shape(
	x_screen_t *  screen ,
	ml_line_t *  line ,
	u_int *  x_rest ,
	int  x
	)
{
	ml_line_t *  orig ;
	int  char_index ;
		
	if( screen->shape)
	{
		if( ( orig = ml_line_shape( line , screen->shape)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ml_line_shape() failed.\n") ;
		#endif
		}
	}
	else
	{
		orig = NULL ;
	}

	char_index = convert_x_to_char_index( screen , line , x_rest , x) ;
	
	if( orig)
	{
		ml_line_unshape( line , orig) ;
	}

	return  char_index ;
}

static u_int
screen_width(
	x_screen_t *  screen
	)
{
	u_int  width ;
	
	/*
	 * logical cols/rows => visual width/height.
	 */
	 
	if( screen->vertical_mode)
	{
		width = ml_term_get_logical_rows( screen->term) * x_col_width( screen) ;
	}
	else
	{
		width = ml_term_get_logical_cols( screen->term) * x_col_width( screen) ;
	}
	
	return  (width * screen->screen_width_ratio) / 100 ;
}

static u_int
screen_height(
	x_screen_t *  screen
	)
{
	u_int  height ;
	
	/*
	 * logical cols/rows => visual width/height.
	 */
	 
	if( screen->vertical_mode)
	{
		height = ml_term_get_logical_cols( screen->term) * x_line_height( screen) ;
	}
	else
	{
		height = ml_term_get_logical_rows( screen->term) * x_line_height( screen) ;
	}

	return  (height * screen->screen_height_ratio) / 100 ;
}


/*
 * drawing screen functions.
 */
 
static int
draw_line(
	x_screen_t *  screen ,
	ml_line_t *  line ,
	int  y
	)
{
	if( ml_line_is_empty( line))
	{
		x_window_clear( &screen->window , 0 , y , screen->window.width , x_line_height(screen)) ;
	}
	else
	{
		int  beg_char_index ;
		int  beg_x ;
		u_int  num_of_redrawn ;
		ml_line_t *  orig ;

		if( screen->shape)
		{
			if( ( orig = ml_line_shape( line , screen->shape)) == NULL)
			{
				return  0 ;
			}
		}
		else
		{
			orig = NULL ;
		}
		
		beg_char_index = ml_line_get_beg_of_modified( line) ;
		
		/* don't use _with_shape function since line is already shaped */
		beg_x = convert_char_index_to_x( screen , line , beg_char_index) ;

		if( x_font_manager_get_font_present( screen->font_man) & FONT_VAR_WIDTH)
		{
			num_of_redrawn = line->num_of_filled_chars - beg_char_index ;
		}
		else
		{
			num_of_redrawn = ml_line_get_num_of_redrawn_chars( line) ;
		}

		if( ml_line_is_cleared_to_end( line) ||
			( x_font_manager_get_font_present( screen->font_man) & FONT_VAR_WIDTH))
		{
			if( ml_line_is_rtl( line))
			{
				x_window_clear( &screen->window , 0 , y ,
					screen->window.width , x_line_height( screen)) ;
			}
			
			if( ! draw_str_to_eol( screen , &line->chars[beg_char_index] ,
				num_of_redrawn , beg_x , y ,
				x_line_height( screen) ,
				x_line_height_to_baseline( screen) ,
				x_line_top_margin( screen) ,
				x_line_bottom_margin( screen)))
			{
				return  0 ;
			}
		}
		else
		{
			if( ! draw_str( screen , &line->chars[beg_char_index] ,
				num_of_redrawn , beg_x , y ,
				x_line_height( screen) ,
				x_line_height_to_baseline( screen) ,
				x_line_top_margin( screen) ,
				x_line_bottom_margin( screen)))
			{
				return  0 ;
			}
		}

		if( orig)
		{
			ml_line_unshape( line , orig) ;
		}
	}

	return  1 ;
}

static int
draw_cursor(
	x_screen_t *  screen ,
	int  restore
	)
{
	int  row ;
	int  x ;
	int  y ;
	ml_line_t *  line ;
	ml_line_t *  orig ;
	ml_char_t  ch ;

	if( ( row = ml_term_cursor_row_in_screen( screen->term)) == -1)
	{
		return  0 ;
	}
	
	y = convert_row_to_y( screen , row) ;
	
	if( ( line = ml_term_get_cursor_line( screen->term)) == NULL || ml_line_is_empty( line))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " cursor line doesn't exist.\n") ;
	#endif

		return  0 ;
	}

	if( screen->shape)
	{
		if( ( orig = ml_line_shape( line , screen->shape)) == NULL)
		{
			return  0 ;
		}
	}
	else
	{
		orig = NULL ;
	}
	
	/* don't use _with_shape function since line is already shaped */
	x = convert_char_index_to_x( screen , line , ml_term_cursor_char_index( screen->term)) ;

	ml_char_init( &ch) ;
	ml_char_copy( &ch , &line->chars[ ml_term_cursor_char_index( screen->term)]) ;

	if( restore)
	{
		draw_str( screen , &ch , 1 , x , y ,
			x_line_height( screen) ,
			x_line_height_to_baseline( screen) ,
			x_line_top_margin( screen) ,
			x_line_bottom_margin( screen)) ;
	}
	else
	{
		if( screen->is_focused)
		{
			ml_char_set_fg_color( &ch , ML_FG_COLOR) ;
			ml_char_set_bg_color( &ch , ML_BG_COLOR) ;
			
			x_color_manager_begin_cursor_color( screen->color_man) ;
			
			if( screen->window.wall_picture_is_set)
			{
				screen->window.wall_picture_is_set = 0 ;
				draw_str( screen , &ch , 1 , x , y ,
					x_line_height( screen) ,
					x_line_height_to_baseline( screen) , 0 , 0) ;
				screen->window.wall_picture_is_set = 1 ;
			}
			else
			{
				draw_str( screen , &ch , 1 , x , y ,
					x_line_height( screen) ,
					x_line_height_to_baseline( screen) , 0 , 0) ;
			}

			x_color_manager_end_cursor_color( screen->color_man) ;
		}
		else
		{
			x_font_t *  xfont ;

			xfont = x_get_font( screen->font_man , ml_char_font( &ch)) ;
			
			x_window_draw_rect_frame( &screen->window ,
				x + 2 ,
				y + x_line_top_margin( screen) + 2 ,
				x + x_calculate_char_width( xfont ,
					ml_char_bytes(&ch) , ml_char_size(&ch) , ml_char_cs(&ch)) + 1 ,
				y + x_line_top_margin( screen) + xfont->height + 1) ;
		}
	}

	ml_char_final( &ch) ;
	
	if( orig)
	{
		ml_line_unshape( line , orig) ;
	}

	return  1 ;
}

static int
flush_scroll_cache(
	x_screen_t *  screen ,
	int  scroll_actual_screen
	)
{
	if( ! screen->scroll_cache_rows)
	{
		return  0 ;
	}
	
	if( scroll_actual_screen && x_window_is_scrollable( &screen->window))
	{
		int  beg_y ;
		int  end_y ;
		u_int  scroll_height ;

		scroll_height = x_line_height( screen) * abs( screen->scroll_cache_rows) ;

		if( scroll_height < screen->window.height)
		{
			beg_y = convert_row_to_y( screen , screen->scroll_cache_boundary_start) ;
			end_y = beg_y + x_line_height( screen) *
					(screen->scroll_cache_boundary_end -
					screen->scroll_cache_boundary_start + 1) ;

			if( screen->scroll_cache_rows > 0)
			{
				x_window_scroll_upward_region( &screen->window ,
					beg_y , end_y , scroll_height) ;
			}
			else
			{
				x_window_scroll_downward_region( &screen->window ,
					beg_y , end_y , scroll_height) ;
			}
		}
	#if  0
		else
		{
			x_window_clear_all( &screen->window) ;
		}
	#endif
	}
	else
	{
		/*
		 * setting modified mark to the lines within scroll region.
		 */

		int  row ;
		ml_line_t *  line ;

		if( screen->scroll_cache_rows > 0)
		{
			/*
			 * scrolling upward.
			 */
			for( row = screen->scroll_cache_boundary_start ;
				row <= screen->scroll_cache_boundary_end - screen->scroll_cache_rows ;
				row ++)
			{
				if( ( line = ml_term_get_line( screen->term , row)))
				{
					ml_line_set_modified_all( line) ;
				}
			}
		}
		else
		{
			/*
			 * scrolling downward.
			 */
			for( row = screen->scroll_cache_boundary_end ;
				row >= screen->scroll_cache_boundary_start - screen->scroll_cache_rows ;
				row --)
			{
				if( ( line = ml_term_get_line( screen->term , row)))
				{
					ml_line_set_modified_all( line) ;
				}
			}
		}
	}

	screen->scroll_cache_rows = 0 ;

	return  1 ;
}

static void
set_scroll_boundary(
	x_screen_t *  screen ,
	int  boundary_start ,
	int  boundary_end
	)
{
	if( screen->scroll_cache_rows &&
		(screen->scroll_cache_boundary_start != boundary_start ||
		screen->scroll_cache_boundary_end != boundary_end))
	{
		flush_scroll_cache( screen , 0) ;
	}

	screen->scroll_cache_boundary_start = boundary_start ;
	screen->scroll_cache_boundary_end = boundary_end ;
}

static int
redraw_screen(
	x_screen_t *  screen
	)
{
	int  count ;
	ml_line_t *  line ;
	int  y ;
	int  end_y ;
	int  beg_y ;

	flush_scroll_cache( screen , 1) ;

	count = 0 ;
	while(1)
	{
		if( ( line = ml_term_get_line_in_screen( screen->term , count)) == NULL)
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " nothing is redrawn.\n") ;
		#endif
		
			return  1 ;
		}
		
		if( ml_line_is_modified( line))
		{
			break ;
		}

		count ++ ;
	}

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " redrawing -> line %d\n" , count) ;
#endif

	beg_y = end_y = y = convert_row_to_y( screen , count) ;

	draw_line( screen , line , y) ;
	ml_line_updated( line) ;

	count ++ ;
	y += x_line_height( screen) ;
	end_y = y ;

	while( ( line = ml_term_get_line_in_screen( screen->term , count)) != NULL)
	{
		if( ml_line_is_modified( line))
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " redrawing -> line %d\n" , count) ;
		#endif

			draw_line( screen , line , y) ;
			ml_line_updated( line) ;

			y += x_line_height( screen) ;
			end_y = y ;
		}
		else
		{
			y += x_line_height( screen) ;
		}
	
		count ++ ;
	}

	x_window_clear_margin_area( &screen->window) ;

	return  1 ;
}

static int
highlight_cursor(
	x_screen_t *  screen
	)
{
	flush_scroll_cache( screen , 1) ;

	draw_cursor( screen , 0) ;

	x_xic_set_spot( &screen->window) ;

	return  1 ;
}

static int
unhighlight_cursor(
	x_screen_t *  screen
	)
{
	flush_scroll_cache( screen , 1) ;

	draw_cursor( screen , 1) ;
	
	return  1 ;
}


/*
 * {enter|exit}_backscroll_mode() and bs_XXX() functions provides backscroll operations.
 *
 * Similar processing to bs_XXX() is done in x_screen_scroll_{upward|downward|to}().
 */
 
static void
enter_backscroll_mode(
	x_screen_t *  screen
	)
{
	if( ml_term_is_backscrolling( screen->term))
	{
		return ;
	}
	
	ml_term_enter_backscroll_mode( screen->term) ;

	if( HAS_SCROLL_LISTENER(screen,bs_mode_entered))
	{
		(*screen->screen_scroll_listener->bs_mode_entered)(
			screen->screen_scroll_listener->self) ;
	}
}

static void
exit_backscroll_mode(
	x_screen_t *  screen
	)
{
	if( ! ml_term_is_backscrolling( screen->term))
	{
		return ;
	}
	
	ml_term_exit_backscroll_mode( screen->term) ;
	
	ml_term_set_modified_all( screen->term) ;
	
	if( HAS_SCROLL_LISTENER(screen,bs_mode_exited))
	{
		(*screen->screen_scroll_listener->bs_mode_exited)(
			screen->screen_scroll_listener->self) ;
	}
}

static void
bs_scroll_upward(
	x_screen_t *  screen
	)
{
	unhighlight_cursor( screen) ;
	
	if( ml_term_backscroll_upward( screen->term , 1))
	{
		redraw_screen( screen) ;
		
		if( HAS_SCROLL_LISTENER(screen,scrolled_upward))
		{
			(*screen->screen_scroll_listener->scrolled_upward)(
				screen->screen_scroll_listener->self , 1) ;
		}
	}
	
	highlight_cursor( screen) ;
}

static void
bs_scroll_downward(
	x_screen_t *  screen
	)
{
	unhighlight_cursor( screen) ;
	
	if( ml_term_backscroll_downward( screen->term , 1))
	{
		redraw_screen( screen) ;
		
		if( HAS_SCROLL_LISTENER(screen,scrolled_downward))
		{
			(*screen->screen_scroll_listener->scrolled_downward)(
				screen->screen_scroll_listener->self , 1) ;
		}
	}
	
	highlight_cursor( screen) ;
}

static void
bs_half_page_upward(
	x_screen_t *  screen
	)
{
	unhighlight_cursor( screen) ;
	
	if( ml_term_backscroll_upward( screen->term , ml_term_get_rows( screen->term) / 2))
	{
		redraw_screen( screen) ;
		
		if( HAS_SCROLL_LISTENER(screen,scrolled_upward))
		{
			(*screen->screen_scroll_listener->scrolled_upward)(
				screen->screen_scroll_listener->self ,
				ml_term_get_rows( screen->term) / 2) ;
		}
	}
	
	highlight_cursor( screen) ;
}

static void
bs_half_page_downward(
	x_screen_t *  screen
	)
{
	unhighlight_cursor( screen) ;
	
	if( ml_term_backscroll_downward( screen->term , ml_term_get_rows( screen->term) / 2))
	{
		redraw_screen( screen) ;
		
		if( HAS_SCROLL_LISTENER(screen,scrolled_downward))
		{
			(*screen->screen_scroll_listener->scrolled_downward)(
				screen->screen_scroll_listener->self ,
				ml_term_get_rows( screen->term) / 2) ;
		}
	}
	
	highlight_cursor( screen) ;
}

static void
bs_page_upward(
	x_screen_t *  screen
	)
{
	unhighlight_cursor( screen) ;
	
	if( ml_term_backscroll_upward( screen->term , ml_term_get_rows( screen->term)))
	{
		redraw_screen( screen) ;
		
		if( HAS_SCROLL_LISTENER(screen,scrolled_upward))
		{
			(*screen->screen_scroll_listener->scrolled_upward)(
				screen->screen_scroll_listener->self ,
				ml_term_get_rows( screen->term)) ;
		}
	}
	
	highlight_cursor( screen) ;
}

static void
bs_page_downward(
	x_screen_t *  screen
	)
{
	unhighlight_cursor( screen) ;
	
	if( ml_term_backscroll_downward( screen->term , ml_term_get_rows( screen->term)))
	{
		redraw_screen( screen) ;
		
		if( HAS_SCROLL_LISTENER(screen,scrolled_downward))
		{
			(*screen->screen_scroll_listener->scrolled_downward)(
				screen->screen_scroll_listener->self ,
				ml_term_get_rows( screen->term)) ;
		}
	}
	
	highlight_cursor( screen) ;
}

 
static x_picture_modifier_t *
get_picture_modifier(
	x_screen_t *  screen
	)
{
	if( x_picture_modifier_is_normal( &screen->pic_mod))
	{
		return  NULL ;
	}
	else
	{
		return  &screen->pic_mod ;
	}
}


/*
 * !! Notice !!
 * don't call x_restore_selected_region_color() directly.
 */
static void
restore_selected_region_color(
	x_screen_t *  screen
	)
{
	if( x_restore_selected_region_color( &screen->sel))
	{
		redraw_screen( screen) ;
		highlight_cursor( screen) ;
	}
}


static void
write_to_pty(
	x_screen_t *  screen ,
	u_char *  str ,			/* str may be NULL */
	size_t  len ,
	mkf_parser_t *  parser		/* parser may be NULL */
	)
{
	if( parser && str)
	{
		(*parser->init)( parser) ;
		(*parser->set_str)( parser , str , len) ;
	}

	ml_term_init_encoding_conv( screen->term) ;

	if( parser)
	{
		u_char  conv_buf[512] ;
		u_char *  p ;
		size_t  filled_len ;

		p = conv_buf ;
		
	#ifdef  __DEBUG
		{
			int  i ;

			kik_debug_printf( KIK_DEBUG_TAG " written str:\n") ;
			for( i = 0 ; i < len ; i ++)
			{
				kik_msg_printf( "[%.2x]" , str[i]) ;
			}
			kik_msg_printf( "=>\n") ;
		}
	#endif

		while( ! parser->is_eos)
		{
			if( ( filled_len = ml_term_convert_to( screen->term ,
						conv_buf , sizeof( conv_buf) , parser)) == 0)
			{
				break ;
			}

		#ifdef  __DEBUG
			{
				int  i ;

				for( i = 0 ; i < filled_len ; i ++)
				{
					kik_msg_printf( "[%.2x]" , conv_buf[i]) ;
				}
			}
		#endif

			ml_term_write( screen->term , conv_buf , filled_len , 0) ;
		}
	}
	else if( str)
	{
	#ifdef  __DEBUG
		{
			int  i ;

			kik_debug_printf( KIK_DEBUG_TAG " written str: ") ;
			for( i = 0 ; i < len ; i ++)
			{
				kik_msg_printf( "%.2x" , str[i]) ;
			}
			kik_msg_printf( "\n") ;
		}
	#endif

		ml_term_write( screen->term , str , len , 0) ;
	}
	else
	{
		return ;
	}
}


static int
set_wall_picture(
	x_screen_t *  screen
	)
{
	x_picture_t  pic ;
	
	if( ! screen->pic_file_path)
	{
		return  0 ;
	}
	
	if( ! x_picture_init( &pic , &screen->window , get_picture_modifier( screen)))
	{
		goto  error ;
	}

	if( ! x_picture_load_file( &pic , screen->pic_file_path))
	{
		kik_msg_printf( " wall picture file %s is not found.\n" ,
			screen->pic_file_path) ;

		x_picture_final( &pic) ;
		
		goto  error ;
	}
	
	if( ! x_window_set_wall_picture( &screen->window , pic.pixmap))
	{
		x_picture_final( &pic) ;
		
		goto  error ;
	}
	else
	{
		x_picture_final( &pic) ;

		return  1 ;
	}

error:
	free( screen->pic_file_path) ;
	screen->pic_file_path = NULL ;

	x_window_unset_wall_picture( &screen->window) ;

	return  0 ;
}

static u_int
get_mod_meta_mask(
	Display *  display ,
	char *  mod_key
	)
{
	int  mask_count ;
	int  kc_count ;
	XModifierKeymap *  mod_map ;
	KeyCode *  key_codes ;
	KeySym  sym ;
	char *  mod_keys[] = { "mod1" , "mod2" , "mod3" , "mod4" , "mod5" } ;
	u_int  mod_masks[] = { Mod1Mask , Mod2Mask , Mod3Mask , Mod4Mask , Mod5Mask } ;

	mod_map = XGetModifierMapping( display) ;
	key_codes = mod_map->modifiermap ;

	if( mod_key)
	{
		int  count ;
		
		for( count = 0 ; count < sizeof( mod_keys) / sizeof( mod_keys[0]) ; count ++)
		{
			if( strcmp( mod_key , mod_keys[count]) == 0)
			{
				return  mod_masks[count] ;
			}
		}
	}
	
	for( mask_count = 0 ; mask_count < 6 ; mask_count++)
	{
		int  count ;

		/*
		 * KeyCodes order is like this.
		 * Shift[max_keypermod] Lock[max_keypermod] Control[max_keypermod]
		 * Mod1[max_keypermod] Mod2[max_keypermod] Mod3[max_keypermod]
		 * Mod4[max_keypermod] Mod5[max_keypermod]
		 */

		/* skip shift/lock/control */
		kc_count = (mask_count + 3) * mod_map->max_keypermod ;
		
		for( count = 0 ; count < mod_map->max_keypermod ; count++)
		{
			if( key_codes[kc_count] == 0)
			{
				break ;
			}

			sym = XKeycodeToKeysym( display , key_codes[kc_count] , 0) ;

			if( ( ( mod_key == NULL || strcmp( mod_key , "meta") == 0) &&
					( sym == XK_Meta_L || sym == XK_Meta_R)) ||
				( ( mod_key == NULL || strcmp( mod_key , "alt") == 0) &&
					( sym == XK_Alt_L || sym == XK_Alt_R)) ||
				( ( mod_key == NULL || strcmp( mod_key , "super") == 0) &&
					( sym == XK_Super_L || sym == XK_Super_R)) ||
				( ( mod_key == NULL || strcmp( mod_key , "hyper") == 0) &&
					( sym == XK_Hyper_L || sym == XK_Hyper_R)) )
			{
				XFreeModifiermap( mod_map) ;
				
				return  mod_masks[mask_count] ;
			}

			kc_count ++ ;
		}
	}
	
	XFreeModifiermap( mod_map) ;

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " No meta key is found.\n") ;
#endif

	return  0 ;
}

static int
update_special_visual(
	x_screen_t *  screen
	)
{
	if( screen->shape)
	{
		(*screen->shape->delete)( screen->shape) ;
		screen->shape = NULL ;
	}

	if( ml_term_get_encoding( screen->term) == ML_ISCII)
	{
		/*
		 * It is impossible to process ISCII with other encoding proper auxes.
		 */

		ml_iscii_lang_t  iscii_lang ;
		u_int  font_size ;
		char *  font_name ;
		
		if( ( iscii_lang = ml_iscii_lang_new( screen->iscii_lang_type)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ml_iscii_new() failed.\n") ;
		#endif

			return  0 ;
		}
		
		for( font_size = screen->font_man->font_custom->min_font_size ;
			font_size <= screen->font_man->font_custom->max_font_size ;
			font_size ++)
		{
			if( ( font_name = ml_iscii_get_font_name( iscii_lang , font_size)) == NULL)
			{
				continue ;
			}

			x_font_manager_set_local_font_name( screen->font_man ,
				DEFAULT_FONT(ISCII) , font_name , font_size) ;
		}

		/*
		 * XXX
		 * anti alias ISCII font is not supported.
		 */
		if( ! ( x_font_manager_get_font_present( screen->font_man) & FONT_AA))
		{
			x_font_manager_reload( screen->font_man) ;
		}

		if( ( screen->shape = ml_iscii_shape_new( iscii_lang)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ml_iscii_shape_new() failed.\n") ;
		#endif

			ml_iscii_lang_delete( iscii_lang) ;

			goto  error ;
		}

		if( ! ml_term_enable_special_visual( screen->term , VIS_ISCII , 0 , iscii_lang , 0))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ml_term_enable_special_visual() failed.\n") ;
		#endif
		
			ml_iscii_lang_delete( iscii_lang) ;

			goto  error ;
		}
	}
	else
	{
		ml_special_visual_t  visual ;

		visual = 0 ;

		if( screen->use_dynamic_comb)
		{
			visual |= VIS_DYNAMIC_COMB ;
		}
		
		if( screen->vertical_mode)
		{
			visual |= VIS_VERT ;
		}
		else if( screen->use_bidi && ml_term_get_encoding( screen->term) == ML_UTF8)
		{
			if( ( screen->shape = ml_arabic_shape_new()) == NULL)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " x_arabic_shape_new() failed.\n") ;
			#endif

				goto  error ;
			}
			
			visual |= VIS_BIDI ;
		}
		
		if( ! ml_term_enable_special_visual( screen->term , visual ,
			0 , NULL , screen->vertical_mode))
		{
			goto  error ;
		}
	}
	
	return  1 ;

error:
	if( screen->shape)
	{
		(*screen->shape->delete)( screen->shape) ;
	}

	return  0 ;
}


/*
 * callbacks of x_window events
 */

static void
window_realized(
	x_window_t *  win
	)
{
	x_screen_t *  screen ;

	screen = (x_screen_t*) win ;

	screen->mod_meta_mask = get_mod_meta_mask( screen->window.display , screen->mod_meta_key) ;

	if( screen->xim_open_in_startup)
	{
		x_xic_activate( &screen->window , "" , "") ;
	}

	x_window_set_fg_color( win , x_get_color( screen->color_man , ML_FG_COLOR)->pixel) ;
	x_window_set_bg_color( win , x_get_color( screen->color_man , ML_BG_COLOR)->pixel) ;

	set_wall_picture( screen) ;
}

static void
window_exposed(
	x_window_t *  win ,
	int  x ,
	int  y ,
	u_int  width ,
	u_int  height
	)
{
	int  count ;
	int  beg_row ;
	int  end_row ;
	x_screen_t *  screen ;
	ml_line_t *  line ;
	
	screen = (x_screen_t *) win ;

	beg_row = convert_y_to_row( screen , NULL , y) ;
	end_row = convert_y_to_row( screen , NULL , y + height) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " exposed [row] from %d to %d [y] from %d to %d\n" ,
		beg_row , end_row , y , y + height) ;
#endif
	
	for( count = beg_row ; count <= end_row ; count ++)
	{
		if( ( line = ml_term_get_line_in_screen( screen->term , count)) == NULL)
		{
			break ;
		}

		ml_line_set_modified_all( line) ;
	}
	
	redraw_screen( screen) ;
	highlight_cursor( screen) ;
}

static void
window_resized(
	x_window_t *  win
	)
{
	x_screen_t *  screen ;
	u_int  rows ;
	u_int  cols ;
	u_int  width ;
	u_int  height ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " term screen resized => width %d height %d.\n" ,
		win->width , win->height) ;
#endif
	
	screen = (x_screen_t *) win ;

	/* This is necessary since ml_term_t size is changed. */
	exit_backscroll_mode( screen) ;

	unhighlight_cursor( screen) ;

	/*
	 * visual width/height => logical cols/rows
	 */

	width = (screen->window.width * 100) / screen->screen_width_ratio ;
	height = (screen->window.height * 100) / screen->screen_height_ratio ;
	
	if( screen->vertical_mode)
	{
		rows = width / x_col_width( screen) ;
		cols = height / x_line_height( screen) ;
	}
	else
	{
		cols = width / x_col_width( screen) ;
		rows = height / x_line_height( screen) ;
	}

	ml_term_resize( screen->term , cols , rows) ;
	
	set_wall_picture( screen) ;

	redraw_screen( screen) ;
	highlight_cursor( screen) ;
}

static void
window_focused(
	x_window_t *  win
	)
{
	x_screen_t *  screen ;

	screen = (x_screen_t *) win ;

	if( screen->is_focused)
	{
		return ;
	}
	
	unhighlight_cursor( screen) ;

	screen->is_focused = 1 ;

	if( screen->fade_ratio != 100)
	{
		x_color_manager_unfade( screen->color_man) ;

		x_window_set_fg_color( &screen->window ,
			x_get_color( screen->color_man , ML_FG_COLOR)->pixel) ;
		x_window_set_bg_color( &screen->window ,
			x_get_color( screen->color_man , ML_BG_COLOR)->pixel) ;

		ml_term_set_modified_all( screen->term) ;

		redraw_screen( screen) ;
	}

	highlight_cursor( screen) ;
}

static void
window_unfocused(
	x_window_t *  win
	)
{
	x_screen_t *  screen ;
	
	screen = (x_screen_t *) win ;

	if( ! screen->is_focused)
	{
		return ;
	}
	
	unhighlight_cursor( screen) ;

	screen->is_focused = 0 ;

	if( screen->fade_ratio != 100)
	{
		x_color_manager_fade( screen->color_man , screen->fade_ratio) ;

		x_window_set_fg_color( &screen->window ,
			x_get_color( screen->color_man , ML_FG_COLOR)->pixel) ;
		x_window_set_bg_color( &screen->window ,
			x_get_color( screen->color_man , ML_BG_COLOR)->pixel) ;

		ml_term_set_modified_all( screen->term) ;

		redraw_screen( screen) ;
	}

	highlight_cursor( screen) ;
}

/*
 * the finalizer of x_screen_t.
 * 
 * x_window_final(term_screen) -> window_finalized(term_screen)
 */
static void
window_finalized(
	x_window_t *  win
	)
{
	x_screen_delete( (x_screen_t*)win) ;
}

static void
window_deleted(
	x_window_t *  win
	)
{
	x_screen_t *  screen ;

	screen = (x_screen_t*) win ;

	if( HAS_SYSTEM_LISTENER(screen,close_screen))
	{
		(*screen->system_listener->close_screen)( screen->system_listener->self , screen) ;
	}
}

static void
config_menu(
	x_screen_t *  screen ,
	int  x ,
	int  y
	)
{
	int  global_x ;
	int  global_y ;
	Window  child ;

	XTranslateCoordinates( screen->window.display , screen->window.my_window ,
		DefaultRootWindow( screen->window.display) , x , y ,
		&global_x , &global_y , &child) ;

	ml_term_start_config_menu( screen->term , NULL , global_x , global_y ,
		DisplayString(screen->window.display)) ;
}

static int
use_utf8_selection(
	x_screen_t *  screen
	)
{
	ml_char_encoding_t  encoding ;

	encoding = ml_term_get_encoding( screen->term) ;

	if( encoding == UTF8)
	{
		return  1 ;
	}
	else if( IS_UCS_SUBSET_ENCODING(encoding) && screen->receive_string_via_ucs)
	{
		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

static int
yank_event_received(
	x_screen_t *  screen ,
	Time  time
	)
{
	if( screen->sel.is_owner)
	{
		if( screen->sel.sel_str == NULL || screen->sel.sel_len == 0)
		{
			return  0 ;
		}

	#ifdef  NL_TO_CR_IN_PAST_TEXT
		{
			int  count ;
			
			/*
			 * Convert normal newline chars to carriage return chars which are
			 * common return key sequences.
			 */
			for( count = 0 ; count < screen->sel.sel_len ; count ++)
			{
				if( ml_char_bytes_is( &screen->sel.sel_str[count] , "\n" , 1 , US_ASCII))
				{
					ml_char_set_bytes( &screen->sel.sel_str[count] , "\r") ;
				}
			}
		}
	#endif
		
		(*screen->ml_str_parser->init)( screen->ml_str_parser) ;
		ml_str_parser_set_str( screen->ml_str_parser ,
			screen->sel.sel_str , screen->sel.sel_len) ;
		
		write_to_pty( screen , NULL , 0 , screen->ml_str_parser) ;

		return  1 ;
	}
	else
	{
		if( use_utf8_selection(screen))
		{
			return  x_window_utf8_selection_request( &screen->window , time) ;
		}
		else
		{
			return  x_window_xct_selection_request( &screen->window , time) ;
		}
	}
}

static int
receive_string_via_ucs(
	x_screen_t *  screen
	)
{
	ml_char_encoding_t  encoding ;

	encoding = ml_term_get_encoding( screen->term) ;

	if( IS_UCS_SUBSET_ENCODING(encoding) && screen->receive_string_via_ucs)
	{
		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

static void
key_pressed(
	x_window_t *  win ,
	XKeyEvent *  event
	)
{
	x_screen_t *  screen ;
	size_t  size ;
	u_char  seq[KEY_BUF_SIZE] ;
	KeySym  ksym ;
	mkf_parser_t *  parser ;

	screen = (x_screen_t *) win ;

	if( screen->kbd)
	{
		size = (*screen->kbd->get_str)( screen->kbd , seq , sizeof(seq) , &parser , &ksym , event) ;
	}
	else
	{
		size = x_window_get_str( win , seq , sizeof(seq) , &parser , &ksym , event) ;
	}

#ifdef  __DEBUG
	{
		int  i ;

		kik_debug_printf( KIK_DEBUG_TAG " received sequence =>") ;
		for( i = 0 ; i < size ; i ++)
		{
			kik_msg_printf( "%.2x" , seq[i]) ;
		}
		kik_msg_printf( "\n") ;
	}
#endif

	if( x_shortcut_match( screen->shortcut , XIM_OPEN , ksym , event->state))
	{
		x_xic_activate( &screen->window , "" , "") ;

		return ;
	}
	else if( x_shortcut_match( screen->shortcut , XIM_CLOSE , ksym , event->state))
	{
		x_xic_deactivate( &screen->window) ;

		return ;
	}
	else if( x_shortcut_match( screen->shortcut , EXT_KBD , ksym , event->state))
	{
		if( screen->kbd)
		{
			if( screen->kbd->type == KBD_ISCII_INSCRIPT)
			{
				(*screen->kbd->delete)( screen->kbd) ;
				screen->kbd = x_iscii_phonetic_kbd_new( &screen->window) ;
			}
			else
			{
				(*screen->kbd->delete)( screen->kbd) ;
				screen->kbd = NULL ;
			}
		}
		else
		{
			if( ml_term_get_encoding( screen->term) == ML_ISCII)
			{
				screen->kbd = x_iscii_inscript_kbd_new( &screen->window) ;
			}
			else
			{
				screen->kbd = x_arabic_kbd_new( &screen->window) ;
			}
		}
		
		return ;
	}
	else if( x_shortcut_match( screen->shortcut , OPEN_SCREEN , ksym , event->state))
	{
		if( HAS_SYSTEM_LISTENER(screen,open_screen))
		{
			(*screen->system_listener->open_screen)( screen->system_listener->self) ;
		}

		return ;
	}
	else if( x_shortcut_match( screen->shortcut , OPEN_PTY , ksym , event->state))
	{
		if( HAS_SYSTEM_LISTENER(screen,open_pty))
		{
			(*screen->system_listener->open_pty)(
				screen->system_listener->self , screen , NULL) ;
		}

		return ;
	}
	else if( x_shortcut_match( screen->shortcut , NEXT_PTY , ksym , event->state))
	{
		if( HAS_SYSTEM_LISTENER(screen,next_pty))
		{
			(*screen->system_listener->next_pty)( screen->system_listener->self , screen) ;
		}

		return ;
	}
#ifdef  DEBUG
	else if( x_shortcut_match( screen->shortcut , EXIT_PROGRAM , ksym , event->state))
	{
		if( HAS_SYSTEM_LISTENER(screen,exit))
		{
			screen->system_listener->exit( screen->system_listener->self , 1) ;
		}

		return ;
	}
#endif

	if( ml_term_is_backscrolling( screen->term))
	{
		if( screen->use_extended_scroll_shortcut)
		{
			if( x_shortcut_match( screen->shortcut , SCROLL_UP , ksym , event->state))
			{
				bs_scroll_downward( screen) ;

				return ;
			}
			else if( x_shortcut_match( screen->shortcut , SCROLL_DOWN , ksym , event->state))
			{
				bs_scroll_upward( screen) ;

				return ;
			}
		#if  1
			else if( ksym == XK_u || ksym == XK_Prior)
			{
				bs_half_page_downward( screen) ;

				return ;
			}
			else if( ksym == XK_d || ksym == XK_Next)
			{
				bs_half_page_upward( screen) ;

				return ;
			}
			else if( ksym == XK_k || ksym == XK_Up)
			{
				bs_scroll_downward( screen) ;

				return ;
			}
			else if( ksym == XK_j || ksym == XK_Down)
			{
				bs_scroll_upward( screen) ;

				return ;
			}
		#endif
		}
		
		if( x_shortcut_match( screen->shortcut , PAGE_UP , ksym , event->state))
		{
			bs_half_page_downward( screen) ;

			return ;
		}
		else if( x_shortcut_match( screen->shortcut , PAGE_DOWN , ksym , event->state))
		{
			bs_half_page_upward( screen) ;

			return ;
		}
		else if( ksym == XK_Shift_L || ksym == XK_Shift_R || ksym == XK_Control_L ||
			ksym == XK_Control_R || ksym == XK_Caps_Lock || ksym == XK_Shift_Lock ||
			ksym == XK_Meta_L || ksym == XK_Meta_R || ksym == XK_Alt_L ||
			ksym == XK_Alt_R || ksym == XK_Super_L || ksym == XK_Super_R ||
			ksym == XK_Hyper_L || ksym == XK_Hyper_R || ksym == XK_Escape)
		{
			/* any modifier keys(X11/keysymdefs.h) */

			return ;
		}
		else
		{
			exit_backscroll_mode( screen) ;
		}
	}

	if( screen->use_extended_scroll_shortcut &&
		x_shortcut_match( screen->shortcut , SCROLL_UP , ksym , event->state))
	{
		enter_backscroll_mode( screen) ;
		bs_scroll_downward( screen) ;
	}
	else if( x_shortcut_match( screen->shortcut , PAGE_UP , ksym , event->state))
	{
		enter_backscroll_mode( screen) ;
		bs_half_page_downward( screen) ;
	}
	else if( x_shortcut_match( screen->shortcut , INSERT_SELECTION , ksym , event->state))
	{
		yank_event_received( screen , CurrentTime) ;
	}
#ifdef  __DEBUG
	else if( ksym == XK_F12)
	{
		/* this is for tests of x_image_xxx functions */

		/* x_image_xxx( screen->image) ; */

		redraw_screen( screen) ;
	}
#endif
	else
	{
		char *  buf ;
		int  is_app_keypad ;
		int  is_app_cursor_keys ;

		is_app_keypad = ml_term_is_app_keypad( screen->term) ;
		is_app_cursor_keys = ml_term_is_app_cursor_keys( screen->term) ;

		if( screen->use_vertical_cursor)
		{
			if( screen->vertical_mode & VERT_RTL)
			{
				if( ksym == XK_Up)
				{
					ksym = XK_Left ;
				}
				else if( ksym == XK_Down)
				{
					ksym = XK_Right ;
				}
				else if( ksym == XK_Left)
				{
					ksym = XK_Down ;
				}
				else if( ksym == XK_Right)
				{
					ksym = XK_Up ;
				}
			}
			else if( screen->vertical_mode & VERT_LTR)
			{
				if( ksym == XK_Up)
				{
					ksym = XK_Left ;
				}
				else if( ksym == XK_Down)
				{
					ksym = XK_Right ;
				}
				else if( ksym == XK_Left)
				{
					ksym = XK_Up ;
				}
				else if( ksym == XK_Right)
				{
					ksym = XK_Down ;
				}
			}
		}

		if( ksym == XK_Delete && size == 1)
		{
			buf = x_termcap_get_str_field( screen->termcap , ML_DELETE) ;
		}
		else if( ksym == XK_BackSpace && size == 1)
		{
			buf = x_termcap_get_str_field( screen->termcap , ML_BACKSPACE) ;
		}
		else if( x_shortcut_str( screen->shortcut , ksym , event->state))
		{
			buf = x_shortcut_str( screen->shortcut , ksym , event->state) ;
		}
		else if( size > 0)
		{
			buf = NULL ;
		}
		/*
		 * following ksym is processed only if no sequence string is received(size == 0)
		 */
		else if( ksym == XK_Up)
		{
			if( is_app_cursor_keys)
			{
				buf = "\x1bOA" ;
			}
			else
			{
				buf = "\x1b[A" ;
			}
		}
		else if( ksym == XK_Down)
		{
			if( is_app_cursor_keys)
			{
				buf = "\x1bOB" ;
			}
			else
			{
				buf = "\x1b[B" ;
			}
		}
		else if( ksym == XK_Right)
		{
			if( is_app_cursor_keys)
			{
				buf = "\x1bOC" ;
			}
			else
			{
				buf = "\x1b[C" ;
			}
		}
		else if( ksym == XK_Left)
		{
			if( is_app_cursor_keys)
			{
				buf = "\x1bOD" ;
			}
			else
			{
				buf = "\x1b[D" ;
			}
		}
		else if( ksym == XK_Prior)
		{
			buf = "\x1b[5~" ;
		}
		else if( ksym == XK_Next)
		{
			buf = "\x1b[6~" ;
		}
		else if( ksym == XK_Insert)
		{
			buf = "\x1b[2~" ;
		}
		else if( ksym == XK_F1)
		{
			buf = "\x1b[11~" ;
		}
		else if( ksym == XK_F2)
		{
			buf = "\x1b[12~" ;
		}
		else if( ksym == XK_F3)
		{
			buf = "\x1b[13~" ;
		}
		else if( ksym == XK_F4)
		{
			buf = "\x1b[14~" ;
		}
		else if( ksym == XK_F5)
		{
			buf = "\x1b[15~" ;
		}
		else if( ksym == XK_F6)
		{
			buf = "\x1b[17~" ;
		}
		else if( ksym == XK_F7)
		{
			buf = "\x1b[18~" ;
		}
		else if( ksym == XK_F8)
		{
			buf = "\x1b[19~" ;
		}
		else if( ksym == XK_F9)
		{
			buf = "\x1b[20~" ;
		}
		else if( ksym == XK_F10)
		{
			buf = "\x1b[21~" ;
		}
		else if( ksym == XK_F11)
		{
			buf = "\x1b[23~" ;
		}
		else if( ksym == XK_F12)
		{
			buf = "\x1b[24~" ;
		}
		else if( ksym == XK_F13)
		{
			buf = "\x1b[25~" ;
		}
		else if( ksym == XK_F14)
		{
			buf = "\x1b[26~" ;
		}
		else if( ksym == XK_F15)
		{
			buf = "\x1b[28~" ;
		}
		else if( ksym == XK_F16)
		{
			buf = "\x1b[29~" ;
		}
		else if( ksym == XK_Help)
		{
			buf = "\x1b[28~" ;
		}
		else if( ksym == XK_Menu)
		{
			buf = "\x1b[29~" ;
		}
		else if( ksym == XK_Home)
		{
			buf = x_termcap_get_str_field( screen->termcap , ML_HOME) ;
		}
		else if( ksym == XK_End)
		{
			buf = x_termcap_get_str_field( screen->termcap , ML_END) ;
		}
	#if  0
		else if( is_app_keypad && ksym == XK_KP_Home)
		{
			buf = "\x1bOw" ;
		}
		else if( is_app_keypad && ksym == XK_KP_Up)
		{
			buf = "\x1bOx" ;
		}
		else if( is_app_keypad && ksym == XK_KP_Down)
		{
			buf = "\x1bOw" ;
		}
		else if( is_app_keypad && ksym == XK_KP_Right)
		{
			buf = "\x1bOv" ;
		}
		else if( is_app_keypad && ksym == XK_KP_Left)
		{
			buf = "\x1bOt" ;
		}
		else if( is_app_keypad && ksym == XK_KP_Prior)
		{
			buf = "\x1bOy" ;
		}
		else if( is_app_keypad && ksym == XK_KP_Next)
		{
			buf = "\x1bOs" ;
		}
		else if( is_app_keypad && ksym == XK_KP_End)
		{
			buf = "\x1bOq" ;
		}
		else if( is_app_keypad && ksym == XK_KP_Enter)
		{
			buf = "\x1bOM" ;
		}
		else if( is_app_keypad && ksym == XK_KP_Begin)
		{
			buf = "\x1bOu" ;
		}
		else if( is_app_keypad && ksym == XK_KP_Insert)
		{
			buf = "\x1bOp" ;
		}
		else if( is_app_keypad && ksym == XK_KP_Begin)
		{
			buf = "\x1bOu" ;
		}
		else if( is_app_keypad && ksym == XK_KP_Delete)
		{
			buf = "\x1bOn" ;
		}
	#endif
		else if( is_app_keypad && ksym == XK_KP_F1)
		{
			buf = "\x1bOP" ;
		}
		else if( is_app_keypad && ksym == XK_KP_F2)
		{
			buf = "\x1bOQ" ;
		}		
		else if( is_app_keypad && ksym == XK_KP_F3)
		{
			buf = "\x1bOR" ;
		}		
		else if( is_app_keypad && ksym == XK_KP_F4)
		{
			buf = "\x1bOS" ;
		}
		else if( is_app_keypad && ksym == XK_KP_Multiply)
		{
			buf = "\x1bOj" ;
		}
		else if( is_app_keypad && ksym == XK_KP_Add)
		{
			buf = "\x1bOk" ;
		}
		else if( is_app_keypad && ksym == XK_KP_Separator)
		{
			buf = "\x1bOl" ;
		}
		else if( is_app_keypad && ksym == XK_KP_Subtract)
		{
			buf = "\x1bOm" ;
		}
		else if( is_app_keypad && ksym == XK_KP_Decimal)
		{
			buf = "\x1bOn" ;
		}
		else if( is_app_keypad && ksym == XK_KP_Divide)
		{
			buf = "\x1bOo" ;
		}
		else if( is_app_keypad && ksym == XK_KP_0)
		{
			buf = "\x1bOp" ;
		}
		else if( is_app_keypad && ksym == XK_KP_1)
		{
			buf = "\x1bOq" ;
		}
		else if( is_app_keypad && ksym == XK_KP_2)
		{
			buf = "\x1bOr" ;
		}
		else if( is_app_keypad && ksym == XK_KP_3)
		{
			buf = "\x1bOs" ;
		}
		else if( is_app_keypad && ksym == XK_KP_4)
		{
			buf = "\x1bOt" ;
		}
		else if( is_app_keypad && ksym == XK_KP_5)
		{
			buf = "\x1bOu" ;
		}
		else if( is_app_keypad && ksym == XK_KP_6)
		{
			buf = "\x1bOv" ;
		}
		else if( is_app_keypad && ksym == XK_KP_7)
		{
			buf = "\x1bOw" ;
		}
		else if( is_app_keypad && ksym == XK_KP_8)
		{
			buf = "\x1bOx" ;
		}
		else if( is_app_keypad && ksym == XK_KP_9)
		{
			buf = "\x1bOy" ;
		}
		else
		{
			return ;
		}

		if( screen->mod_meta_mask & event->state)
		{
			if( screen->mod_meta_mode == MOD_META_OUTPUT_ESC)
			{
				write_to_pty( screen , "\x1b" , 1 , NULL) ;
			}
			else if( screen->mod_meta_mode == MOD_META_SET_MSB)
			{
				int  count ;

				for( count = 0 ; count < size ; count ++)
				{
					if( 0x20 <= seq[count] && seq[count] <= 0x7e)
					{
						seq[count] |= 0x80 ;
					}
				}
			}
		}

		if( buf)
		{
			write_to_pty( screen , buf , strlen(buf) , NULL) ;
		}
		else
		{
			if( parser && receive_string_via_ucs(screen))
			{
				/* XIM Text -> UCS -> PTY ENCODING */

				u_char  conv_buf[512] ;
				size_t  filled_len ;

				(*parser->init)( parser) ;
				(*parser->set_str)( parser , seq , size) ;

				(*screen->utf8_conv->init)( screen->utf8_conv) ;

				while( ! parser->is_eos)
				{
					if( ( filled_len = (*screen->utf8_conv->convert)(
						screen->utf8_conv , conv_buf , sizeof( conv_buf) ,
						parser)) == 0)
					{
						break ;
					}

					write_to_pty( screen , conv_buf , filled_len , screen->utf8_parser) ;
				}
			}
			else
			{
				write_to_pty( screen , seq , size , parser) ;
			}
		}
	}
}

static void
selection_cleared(
	x_window_t *  win ,
	XSelectionClearEvent *  event
	)
{
	x_sel_clear( &((x_screen_t*)win)->sel) ;
}

static size_t
convert_selection_to_xct(
	x_screen_t *  screen ,
	u_char *  str ,
	size_t  len
	)
{
	size_t  filled_len ;
	
#ifdef  __DEBUG
	{
		int  i ;

		kik_debug_printf( KIK_DEBUG_TAG " sending internal str: ") ;
		for( i = 0 ; i < screen->sel.sel_len ; i ++)
		{
			ml_char_dump( &screen->sel.sel_str[i]) ;
		}
		kik_msg_printf( "\n -> converting to ->\n") ;
	}
#endif

	(*screen->ml_str_parser->init)( screen->ml_str_parser) ;
	ml_str_parser_set_str( screen->ml_str_parser , screen->sel.sel_str , screen->sel.sel_len) ;
	
	(*screen->xct_conv->init)( screen->xct_conv) ;
	filled_len = (*screen->xct_conv->convert)( screen->xct_conv ,
		str , len , screen->ml_str_parser) ;

#ifdef  __DEBUG
	{
		int  i ;

		kik_debug_printf( KIK_DEBUG_TAG " sending xct str: ") ;
		for( i = 0 ; i < filled_len ; i ++)
		{
			kik_msg_printf( "%.2x" , str[i]) ;
		}
		kik_msg_printf( "\n") ;
	}
#endif

	return  filled_len ;
}

static size_t
convert_selection_to_utf8(
	x_screen_t *  screen ,
	u_char *  str ,
	size_t  len
	)
{
	size_t  filled_len ;

#ifdef  __DEBUG
	{
		int  i ;

		kik_debug_printf( KIK_DEBUG_TAG " sending internal str: ") ;
		for( i = 0 ; i < screen->sel.sel_len ; i ++)
		{
			ml_char_dump( &screen->sel.sel_str[i]) ;
		}
		kik_msg_printf( "\n -> converting to ->\n") ;
	}
#endif

	(*screen->ml_str_parser->init)( screen->ml_str_parser) ;
	ml_str_parser_set_str( screen->ml_str_parser , screen->sel.sel_str , screen->sel.sel_len) ;
	
	(*screen->utf8_conv->init)( screen->utf8_conv) ;
	filled_len = (*screen->utf8_conv->convert)( screen->utf8_conv ,
		str , len , screen->ml_str_parser) ;
		
#ifdef  __DEBUG
	{
		int  i ;

		kik_debug_printf( KIK_DEBUG_TAG " sending utf8 str: ") ;
		for( i = 0 ; i < filled_len ; i ++)
		{
			kik_msg_printf( "%.2x" , str[i]) ;
		}
		kik_msg_printf( "\n") ;
	}
#endif

	return  filled_len ;
}

static void
xct_selection_requested(
	x_window_t * win ,
	XSelectionRequestEvent *  event ,
	Atom  type
	)
{
	x_screen_t *  screen ;

	screen = (x_screen_t*) win ;

	if( screen->sel.sel_str == NULL || screen->sel.sel_len == 0)
	{
		x_window_send_selection( win , event , NULL , 0 , 0) ;
	}
	else
	{
		u_char *  xct_str ;
		size_t  xct_len ;
		size_t  filled_len ;

		xct_len = screen->sel.sel_len * XCT_MAX_CHAR_SIZE ;

		if( ( xct_str = alloca( xct_len)) == NULL)
		{
			return ;
		}

		filled_len = convert_selection_to_xct( screen , xct_str , xct_len) ;

		x_window_send_selection( win , event , xct_str , filled_len , type) ;
	}
}

static void
utf8_selection_requested(
	x_window_t * win ,
	XSelectionRequestEvent *  event ,
	Atom  type
	)
{
	x_screen_t *  screen ;

	screen = (x_screen_t*) win ;

	if( screen->sel.sel_str == NULL || screen->sel.sel_len == 0)
	{		
		x_window_send_selection( win , event , NULL , 0 , 0) ;
	}
	else
	{
		u_char *  utf8_str ;
		size_t  utf8_len ;
		size_t  filled_len ;
		
		utf8_len = screen->sel.sel_len * UTF8_MAX_CHAR_SIZE ;

		if( ( utf8_str = alloca( utf8_len)) == NULL)
		{
			return ;
		}

		filled_len = convert_selection_to_utf8( screen , utf8_str , utf8_len) ;

		x_window_send_selection( win , event , utf8_str , filled_len , type) ;
	}
}

static void
xct_selection_notified(
	x_window_t *  win ,
	u_char *  str ,
	size_t  len
	)
{
	x_screen_t *  screen ;

#ifdef  NL_TO_CR_IN_PAST_TEXT
	int  count ;

	/*
	 * Convert normal newline chars to carriage return chars which are
	 * common return key sequences.
	 */
	for( count = 0 ; count < len ; count ++)
	{
		if( str[count] == '\n')
		{
			str[count] = '\r' ;
		}
	}
#endif

	screen = (x_screen_t*) win ;

#if  1
	/*
	 * XXX
	 * parsing UTF-8 sequence designated by ESC % G.
	 */
	if( len > 3 && strncmp( str , "\x1b%G" , 3) == 0)
	{
	#if  0
		int  i;
		for( i = 0 ; i < len ; i ++)
		{
			kik_msg_printf( "%.2x " , str[i]) ;
		}
	#endif
	
		write_to_pty( screen , str + 3 , len - 3 , screen->utf8_parser) ;
	}
	else
#endif
	if( receive_string_via_ucs(screen))
	{
		/* XCOMPOUND TEXT -> UCS -> PTY ENCODING */
		
		u_char  conv_buf[512] ;
		size_t  filled_len ;

		(*screen->xct_parser->init)( screen->xct_parser) ;
		(*screen->xct_parser->set_str)( screen->xct_parser , str , len) ;
		
		(*screen->utf8_conv->init)( screen->utf8_conv) ;

		while( ! screen->xct_parser->is_eos)
		{
			if( ( filled_len = (*screen->utf8_conv->convert)(
				screen->utf8_conv , conv_buf , sizeof( conv_buf) ,
				screen->xct_parser)) == 0)
			{
				break ;
			}

			write_to_pty( screen , conv_buf , filled_len , screen->utf8_parser) ;
		}
	}
	else
	{
		/* XCOMPOUND TEXT -> PTY ENCODING */
		
		write_to_pty( screen , str , len , screen->xct_parser) ;
	}
}

static void
utf8_selection_notified(
	x_window_t *  win ,
	u_char *  str ,
	size_t  len
	)
{
#ifdef  NL_TO_CR_IN_PAST_TEXT
	int  count ;

	/*
	 * Convert normal newline chars to carriage return chars which are
	 * common return key sequences.
	 */
	for( count = 0 ; count < len ; count ++)
	{
		if( str[count] == '\n')
		{
			str[count] = '\r' ;
		}
	}
#endif

	write_to_pty( (x_screen_t*) win , str , len , ( (x_screen_t*) win)->utf8_parser) ;
}


static void
start_selection(
	x_screen_t *  screen ,
	int  col_r ,
	int  row_r
	)
{
	int  col_l ;
	int  row_l ;
	ml_line_t *  line ;

	if( ( line = ml_term_get_line( screen->term , row_r)) == NULL || ml_line_is_empty( line))
	{
		return ;
	}
	
	if( ( ! ml_line_is_rtl( line) && col_r == 0) ||
		( ml_line_is_rtl( line) && abs( col_r) == line->num_of_filled_chars - 1))
	{
		if( ( line = ml_term_get_line( screen->term , row_r - 1)) == NULL ||
			ml_line_is_empty( line))
		{
			/* XXX col_l can be underflowed, but anyway it works. */
			col_l = col_r - 1 ;
			row_l = row_r ;
		}
		else
		{
			if( ml_line_is_rtl( line))
			{
				col_l = 0 ;
			}
			else
			{
				col_l = line->num_of_filled_chars - 1 ;
			}
			row_l = row_r - 1 ;
		}
	}
	else
	{
		col_l = col_r - 1 ;
		row_l = row_r ;
	}

	if( x_start_selection( &screen->sel , col_l , row_l , col_r , row_r))
	{
		redraw_screen( screen) ;
	}
}

static void
selecting(
	x_screen_t *  screen ,
	int  char_index ,
	int  row
	)
{
	if( x_selecting( &screen->sel , char_index , row))
	{
		redraw_screen( screen) ;
	}
}

static void
selecting_with_motion(
	x_screen_t *  screen ,
	int  x ,
	int  y ,
	Time  time
	)
{
	int  char_index ;
	int  row ;
	int  x_is_outside ;
	u_int  x_rest ;
	ml_line_t *  line ;

	x_is_outside = 0 ;
	if( x < 0)
	{
		x = 0 ;
		x_is_outside = 1 ;
	}
	else if( x > screen->window.width)
	{
		x = screen->window.width ;
		x_is_outside = 1 ;
	}
	
	if( y < 0)
	{
		if( ml_term_get_num_of_logged_lines( screen->term) > 0)
		{
			if( ! ml_term_is_backscrolling( screen->term))
			{
				enter_backscroll_mode( screen) ;
			}
			
			bs_scroll_downward( screen) ;
		}
		
		y = 0 ;
	}
	else if( y > screen->window.height)
	{
		if( ml_term_is_backscrolling( screen->term))
		{
			bs_scroll_upward( screen) ;
		}
		
		y = screen->window.height - x_line_height( screen) ;
	}

	row = ml_term_convert_scr_row_to_abs( screen->term , convert_y_to_row( screen , NULL , y)) ;

	if( ( line = ml_term_get_line( screen->term , row)) == NULL || ml_line_is_empty( line))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " line(%d) not found.\n" , row) ;
	#endif

		return ;
	}
	
	char_index = convert_x_to_char_index_with_shape( screen , line , &x_rest , x) ;

	if( ml_line_is_rtl( line))
	{
		char_index = -char_index ;
	}

	if( ! screen->sel.is_selecting)
	{
		restore_selected_region_color( screen) ;
		
		if( ! screen->sel.is_owner)
		{
			if( x_window_set_selection_owner( &screen->window , time) == 0)
			{
				return ;
			}
		}

		start_selection( screen , char_index , row) ;
	}
	else
	{
		if( x_is_after_sel_right_base_pos( &screen->sel , char_index , row))
		{
			if( ! x_is_outside && abs( char_index) > 0)
			{
				char_index -- ;
			}
		}
		else if( x_is_before_sel_left_base_pos( &screen->sel , char_index , row))
		{
			if( ! x_is_outside && abs( char_index) < ml_line_end_char_index( line))
			{
				char_index ++ ;
			}
		}
		
		if( x_selected_region_is_changed( &screen->sel , char_index , row , 1))
		{
			selecting( screen , char_index , row) ;
		}
	}
}

static void
button_motion(
	x_window_t *  win ,
	XMotionEvent *  event
	)
{
	x_screen_t *  screen ;
	
	screen = (x_screen_t*) win ;

	if( ml_term_is_mouse_pos_sending( screen->term) && ! (event->state & ShiftMask))
	{
		return ;
	}

	selecting_with_motion( screen , event->x , event->y , event->time) ;
}

static void
button_press_continued(
	x_window_t *  win ,
	XButtonEvent *  event
	)
{
	x_screen_t *  screen ;
	
	screen = (x_screen_t*) win ;
	
	if( screen->sel.is_selecting &&
		(event->y < 0 || win->height < event->y))
	{
		selecting_with_motion( screen , event->x , event->y , event->time) ;
	}
}

static void
selecting_word(
	x_screen_t *  screen ,
	int  x ,
	int  y ,
	Time  time
	)
{
	int  char_index ;
	int  row ;
	u_int  x_rest ;
	int  beg_row ;
	int  beg_char_index ;
	int  end_row ;
	int  end_char_index ;
	ml_line_t *  line ;

	row = ml_term_convert_scr_row_to_abs( screen->term , convert_y_to_row( screen , NULL , y)) ;

	if( ( line = ml_term_get_line( screen->term , row)) == NULL || ml_line_is_empty( line))
	{
		return ;
	}

	char_index = convert_x_to_char_index_with_shape( screen , line , &x_rest , x) ;

	if( line->num_of_filled_chars - 1 == char_index && x_rest)
	{
		/* over end of line */

		return ;
	}

	if( ml_term_get_word_region( screen->term , &beg_char_index , &beg_row , &end_char_index ,
		&end_row , char_index , row) == 0)
	{
		return ;
	}

	if( ml_line_is_rtl( ml_term_get_line( screen->term , beg_row)))
	{
		beg_char_index = -beg_char_index + 1 ;
	}
	
	if( ml_line_is_rtl( ml_term_get_line( screen->term , end_row)))
	{
		end_char_index = -end_char_index ;
	}
	
	if( ! screen->sel.is_selecting)
	{
		restore_selected_region_color( screen) ;
		
		if( ! screen->sel.is_owner)
		{
			if( x_window_set_selection_owner( &screen->window , time) == 0)
			{
				return ;
			}
		}
		
		start_selection( screen , beg_char_index , beg_row) ;
	}

	selecting( screen , end_char_index , end_row) ;
}

static void
selecting_line(
	x_screen_t *  screen ,
	int  y ,
	Time  time
	)
{
	int  row ;
	int  beg_char_index ;
	int  beg_row ;
	int  end_char_index ;
	int  end_row ;

	row = ml_term_convert_scr_row_to_abs( screen->term , convert_y_to_row( screen , NULL , y)) ;

	if( ml_term_get_line_region( screen->term , &beg_row , &end_char_index , &end_row , row) == 0)
	{
		return ;
	}

	if( ml_line_is_rtl( ml_term_get_line( screen->term , beg_row)))
	{
		beg_char_index = -ml_line_end_char_index( ml_term_get_line( screen->term , beg_row)) ;
	}
	else
	{
		beg_char_index = 0 ;
	}
	
	if( ml_line_is_rtl( ml_term_get_line( screen->term , end_row)))
	{
		end_char_index = end_char_index -
			ml_line_end_char_index( ml_term_get_line( screen->term , end_row)) ;
	}
	
	if( ! screen->sel.is_selecting)
	{
		restore_selected_region_color( screen) ;
		
		if( ! screen->sel.is_owner)
		{
			if( x_window_set_selection_owner( &screen->window , time) == 0)
			{
				return ;
			}
		}
		
		start_selection( screen , beg_char_index , beg_row) ;
	}

	selecting( screen , end_char_index , end_row) ;
}

static int
report_mouse_tracking(
	x_screen_t *  screen ,
	XButtonEvent *  event ,
	int  is_released
	)
{
	ml_line_t *  line ;
	int  button ;
	int  key_state ;
	int  col ;
	int  row ;
	u_char  buf[7] ;

	if( is_released)
	{
		button = 3 ;
	}
	else
	{
		button = event->button - Button1 ;
	}

	/*
	 * Shift = 4
	 * Meta = 8
	 * Control = 16
	 */
	key_state = ((event->state & ShiftMask) ? 4 : 0) +
		((event->state & ControlMask) ? 16 : 0) ;

	if( screen->vertical_mode)
	{
		u_int  x_rest ;
		
		col = convert_y_to_row( screen , NULL , event->y) ;

	#if  0
		if( screen->font_man->use_multi_col_char)
		{
			/*
			 * XXX
			 * col can be inaccurate if full width characters are used.
			 */
		}
	#endif
	
		if( ( line = ml_term_get_line_in_screen( screen->term , col)) == NULL)
		{
			return  0 ;
		}
		
		row = ml_convert_char_index_to_col( line ,
			convert_x_to_char_index_with_shape( screen , line , &x_rest , event->x) , 0) ;
			
		if( screen->vertical_mode & VERT_RTL)
		{
			row = ml_term_get_cols( screen->term) - row - 1 ;
		}
		
	#if  0
		if( screen->font_man->use_multi_col_char)
		{
			/*
			 * XXX
			 * row can be inaccurate if full width characters are used.
			 */
		}
	#endif
	}
	else
	{
		row = convert_y_to_row( screen , NULL , event->y) ;
		
		if( ( line = ml_term_get_line_in_screen( screen->term , row)) == NULL)
		{
			return  0 ;
		}
		
		col = ml_convert_char_index_to_col( line ,
			convert_x_to_char_index_with_shape( screen , line , NULL , event->x) , 0) ;
	}

	strcpy( buf , "\x1b[M") ;

	buf[3] = 0x20 + button + key_state ;
	buf[4] = 0x20 + col + 1 ;
	buf[5] = 0x20 + row + 1 ;

	write_to_pty( screen , buf , 6 , NULL) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [reported cursor pos] %d %d\n" , col , row) ;
#endif
	
	return  1 ;
}

static void
button_pressed(
	x_window_t *  win ,
	XButtonEvent *  event ,
	int  click_num
	)
{
	x_screen_t *  screen ;

	screen = (x_screen_t*)win ;

	if( ml_term_is_mouse_pos_sending( screen->term) && ! (event->state & (ShiftMask | ControlMask)))
	{
		restore_selected_region_color( screen) ;
		
		report_mouse_tracking( screen , event , 0) ;

		return ;
	}

	if( event->button == 2)
	{
		restore_selected_region_color( screen) ;
		
		yank_event_received( screen , event->time) ;
	}
	else if( click_num == 2 && event->button == 1)
	{
		/* double clicked */
		
		selecting_word( screen , event->x , event->y , event->time) ;
	}
	else if( click_num == 3 && event->button == 1)
	{
		/* triple click */

		selecting_line( screen , event->y , event->time) ;
	}
	else if( event->button == 3 && (event->state & ControlMask))
	{
		config_menu( screen , event->x , event->y) ;
	}
	else if ( event->button == 4) 
	{
		/* wheel mouse */
		
		enter_backscroll_mode(screen) ;
		if( event->state & ShiftMask)
		{
			bs_scroll_downward(screen) ;
		}
		else if( event->state & ControlMask)
		{
			bs_page_downward(screen) ;
		} 
		else 
		{
			bs_half_page_downward(screen) ;
		}
	}
	else if ( event->button == 5) 
	{
		/* wheel mouse */
		
		enter_backscroll_mode(screen) ;
		if( event->state & ShiftMask)
		{
			bs_scroll_upward(screen) ;
		}
		else if( event->state & ControlMask)
		{
			bs_page_upward(screen) ;
		} 
		else 
		{
			bs_half_page_upward(screen) ;
		}
	}
	else
	{
		restore_selected_region_color( screen) ;
	}
}

static void
button_released(
	x_window_t *  win ,
	XButtonEvent *  event
	)
{
	x_screen_t *  screen ;

	screen = (x_screen_t*) win ;

	if( ml_term_is_mouse_pos_sending( screen->term) && ! (event->state & ShiftMask))
	{
		report_mouse_tracking( screen , event , 1) ;
	}
	else
	{
		x_stop_selecting( &screen->sel) ;
		highlight_cursor( screen) ;
	}
}


static void
font_size_changed(
	x_screen_t *  screen
	)
{
	if( HAS_SCROLL_LISTENER(screen,line_height_changed))
	{
		(*screen->screen_scroll_listener->line_height_changed)(
			screen->screen_scroll_listener->self , x_line_height( screen)) ;
	}

	/* screen will redrawn in window_resized() */
	x_window_resize( &screen->window , screen_width( screen) , screen_height( screen) ,
		NOTIFY_TO_PARENT) ;

	x_window_set_normal_hints( &screen->window , 0 , 0 ,
		x_col_width( screen) , x_line_height( screen)) ;
		
	/*
	 * !! Notice !!
	 * x_window_resize() will invoke ConfigureNotify event but window_resized() won't be
	 * called , since xconfigure.width , xconfigure.height are the same as the already
	 * resized window.
	 */
	if( screen->window.window_resized)
	{
		(*screen->window.window_resized)( &screen->window) ;
	}
}

static void
change_font_size(
	x_screen_t *  screen ,
	u_int  font_size
	)
{
	if( font_size == screen->font_man->font_size)
	{
		/* not changed */
		
		return ;
	}
	
	if( ! x_change_font_size( screen->font_man , font_size))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_change_font_size(%d) failed.\n" , font_size) ;
	#endif
	
		return ;
	}
	
	/* redrawing all lines with new fonts. */
	ml_term_set_modified_all( screen->term) ;

	font_size_changed( screen) ;

	/* this is because font_man->font_set may have changed in x_change_font_size() */
	x_xic_font_set_changed( &screen->window) ;
}

static void
change_line_space(
	x_screen_t *  screen ,
	u_int  line_space
	)
{
	screen->line_space = line_space ;

	font_size_changed( screen) ;
}

static void
change_screen_width_ratio(
	x_screen_t *  screen ,
	u_int  ratio
	)
{
	if( screen->screen_width_ratio == ratio)
	{
		return ;
	}
	
	screen->screen_width_ratio = ratio ;

	x_window_resize( &screen->window , screen_width( screen) , screen_height( screen) ,
		NOTIFY_TO_PARENT) ;

	/*
	 * !! Notice !!
	 * x_window_resize() will invoke ConfigureNotify event but window_resized() won't be
	 * called , since xconfigure.width , xconfigure.height are the same as the already
	 * resized window.
	 */
	if( screen->window.window_resized)
	{
		(*screen->window.window_resized)( &screen->window) ;
	}
}

static void
change_screen_height_ratio(
	x_screen_t *  screen ,
	u_int  ratio
	)
{
	if( screen->screen_height_ratio == ratio)
	{
		return ;
	}
	
	screen->screen_height_ratio = ratio ;

	x_window_resize( &screen->window , screen_width( screen) , screen_height( screen) ,
		NOTIFY_TO_PARENT) ;

	/*
	 * !! Notice !!
	 * x_window_resize() will invoke ConfigureNotify event but window_resized() won't be
	 * called , since xconfigure.width , xconfigure.height are the same as the already
	 * resized window.
	 */
	if( screen->window.window_resized)
	{
		(*screen->window.window_resized)( &screen->window) ;
	}
}

static void
change_font_present(
	x_screen_t *  screen ,
	x_font_present_t  font_present
	)
{
	if( screen->vertical_mode)
	{
		font_present &= ~FONT_VAR_WIDTH ;
	}

	if( x_font_manager_get_font_present( screen->font_man) == font_present)
	{
		/* not changed */
		
		return ;
	}

	if( ! x_font_manager_change_font_present( screen->font_man , font_present))
	{
		return ;
	}

	font_size_changed( screen) ;
	
	/* redrawing all lines with new fonts. */
	ml_term_set_modified_all( screen->term) ;
}

static int
usascii_font_cs_changed(
	x_screen_t *  screen ,
	ml_char_encoding_t  encoding
	)
{
	x_font_manager_usascii_font_cs_changed( screen->font_man , x_get_usascii_font_cs( encoding)) ;
	
	font_size_changed( screen) ;

	/*
	 * this is because font_man->font_set may have changed in
	 * x_font_manager_usascii_font_cs_changed()
	 */
	x_xic_font_set_changed( &screen->window) ;

	return  1 ;
}

static void
change_char_encoding(
	x_screen_t *  screen ,
	ml_char_encoding_t  encoding
	)
{
	if( ml_term_get_encoding( screen->term) == encoding)
	{
		/* not changed */
		
		return ;
	}

	if( encoding == ML_ISCII)
	{
		/*
		 * ISCII needs variable column width and character combining.
		 */

		if( ! ( x_font_manager_get_font_present( screen->font_man) & FONT_VAR_WIDTH))
		{
			change_font_present( screen ,
				x_font_manager_get_font_present( screen->font_man) | FONT_VAR_WIDTH) ;
		}

		ml_term_set_char_combining_flag( screen->term , 1) ;
	}

	usascii_font_cs_changed( screen , encoding) ;

	if( ! ml_term_change_encoding( screen->term , encoding))
	{
		kik_error_printf( "VT100 encoding and Terminal screen encoding are discrepant.\n") ;
	}
	
	update_special_visual( screen) ;
	
	ml_term_set_modified_all( screen->term) ;
}

static void
change_iscii_lang(
	x_screen_t *  screen ,
	ml_iscii_lang_type_t  type
	)
{
	if( screen->iscii_lang_type == type)
	{
		/* not changed */
		
		return ;
	}

	screen->iscii_lang_type = type ;
	
	if( update_special_visual( screen))
	{
		ml_term_set_modified_all( screen->term) ;
	}
}

static void
change_tab_size(
	x_screen_t *  screen ,
	u_int  tab_size
	)
{
	ml_term_set_tab_size( screen->term , tab_size) ;
}

static void
change_log_size(
	x_screen_t *  screen ,
	u_int  logsize
	)
{
	if( ml_term_get_log_size( screen->term) == logsize)
	{
		/* not changed */
		
		return ;
	}

	/*
	 * this is necesary since ml_logs_t size is changed.
	 */
	restore_selected_region_color( screen) ;
	exit_backscroll_mode( screen) ;
	
	ml_term_change_log_size( screen->term , logsize) ;

	if( HAS_SCROLL_LISTENER(screen,log_size_changed))
	{
		(*screen->screen_scroll_listener->log_size_changed)(
			screen->screen_scroll_listener->self , logsize) ;
	}
}

static void
change_sb_view(
	x_screen_t *  screen ,
	char *  name
	)
{
	if( HAS_SCROLL_LISTENER(screen,change_view))
	{
		(*screen->screen_scroll_listener->change_view)(
			screen->screen_scroll_listener->self , name) ;
	}
}

static void
change_mod_meta_key(
	x_screen_t *  screen ,
	char *  key
	)
{
	free( screen->mod_meta_key) ;

	if( strcmp( key , "none") == 0)
	{
		screen->mod_meta_key = NULL ;
	}
	else
	{
		screen->mod_meta_key = strdup( key) ;
	}
	
	screen->mod_meta_mask = get_mod_meta_mask( screen->window.display , screen->mod_meta_key) ;
}

static void
change_mod_meta_mode(
	x_screen_t *  screen ,
	x_mod_meta_mode_t  mod_meta_mode
	)
{
	screen->mod_meta_mode = mod_meta_mode ;
}

static void
change_bel_mode(
	x_screen_t *  screen ,
	x_bel_mode_t  bel_mode
	)
{	
	screen->bel_mode = bel_mode ;
}

static void
change_vertical_mode(
	x_screen_t *  screen ,
	ml_vertical_mode_t  vertical_mode
	)
{
	if( screen->vertical_mode == vertical_mode)
	{
		/* not changed */
		
		return ;
	}

	/*
	 * vertical font is automatically used under vertical mode.
	 * similler processing is done in x_term_manager.c:config_init.
	 */
	if( vertical_mode)
	{
		if( ! ( x_font_manager_get_font_present( screen->font_man) & FONT_VERTICAL))
		{
			change_font_present( screen ,
				( x_font_manager_get_font_present( screen->font_man) | FONT_VERTICAL) &
					~FONT_VAR_WIDTH) ;
		}
	}
	else
	{
		if( x_font_manager_get_font_present( screen->font_man) & FONT_VERTICAL)
		{
			change_font_present( screen ,
				x_font_manager_get_font_present( screen->font_man) & ~FONT_VERTICAL) ;
		}
	}

	screen->vertical_mode = vertical_mode ;
	
	if( update_special_visual( screen))
	{
		/* redrawing under new vertical mode. */
		ml_term_set_modified_all( screen->term) ;
	}
	
	x_window_resize( &screen->window , screen_width(screen) , screen_height(screen) ,
		NOTIFY_TO_PARENT) ;

	/*
	 * !! Notice !!
	 * x_window_resize() will invoke ConfigureNotify event but window_resized() won't be
	 * called , since xconfigure.width , xconfigure.height are the same as the already
	 * resized window.
	 */
	if( screen->window.window_resized)
	{
		(*screen->window.window_resized)( &screen->window) ;
	}
}

static void
change_sb_mode(
	x_screen_t *  screen ,
	x_sb_mode_t  sb_mode
	)
{
	if( HAS_SCROLL_LISTENER(screen,change_sb_mode))
	{
		(*screen->screen_scroll_listener->change_sb_mode)(
			screen->screen_scroll_listener->self , sb_mode) ;
	}
}

static void
change_char_combining_flag(
	x_screen_t *  screen ,
	int  flag
	)
{
	ml_term_set_char_combining_flag( screen->term , flag) ;
}

static void
change_dynamic_comb_flag(
	x_screen_t *  screen ,
	int  use_dynamic_comb
	)
{
	if( screen->use_dynamic_comb == use_dynamic_comb)
	{
		/* not changed */
		
		return ;
	}

	screen->use_dynamic_comb = use_dynamic_comb ;

	if( update_special_visual( screen))
	{
		ml_term_set_modified_all( screen->term) ;
	}
}

static void
change_receive_string_via_ucs_flag(
	x_screen_t *  screen ,
	int  flag
	)
{
	screen->receive_string_via_ucs = flag ;
}

static void
change_fg_color(
	x_screen_t *  screen ,
	char *  name
	)
{
	if( strcmp( name , x_color_manager_get_fg_color( screen->color_man)) == 0)
	{
		return ;
	}

	x_color_manager_set_fg_color( screen->color_man , name) ;
	
	x_window_set_fg_color( &screen->window ,
		x_get_color( screen->color_man , ML_FG_COLOR)->pixel) ;
	
	x_xic_fg_color_changed( &screen->window) ;

	ml_term_set_modified_all( screen->term) ;
}

static void
change_bg_color(
	x_screen_t *  screen ,
	char *  name
	)
{
	if( strcmp( name , x_color_manager_get_bg_color( screen->color_man)) == 0)
	{
		return ;
	}

	x_color_manager_set_bg_color( screen->color_man , name) ;
	
	x_window_set_bg_color( &screen->window ,
		x_get_color( screen->color_man , ML_BG_COLOR)->pixel) ;
	
	x_xic_bg_color_changed( &screen->window) ;
	
	ml_term_set_modified_all( screen->term) ;
}

static void
change_cursor_fg_color(
	x_screen_t *  screen ,
	char *  name
	)
{
	char *  old ;
	
	if( ( old = x_color_manager_get_cursor_fg_color( screen->color_man)) == NULL)
	{
		old = "" ;
	}
	
	if( strcmp( name , old) == 0)
	{
		return ;
	}

	if( *name == '\0')
	{
		name = NULL ;
	}

	x_color_manager_set_cursor_fg_color( screen->color_man , name) ;
}

static void
change_cursor_bg_color(
	x_screen_t *  screen ,
	char *  name
	)
{
	char *  old ;
	
	if( ( old = x_color_manager_get_cursor_bg_color( screen->color_man)) == NULL)
	{
		old = "" ;
	}
	
	if( strcmp( name , old) == 0)
	{
		return ;
	}

	if( *name == '\0')
	{
		name = NULL ;
	}

	x_color_manager_set_cursor_bg_color( screen->color_man , name) ;
}

static void
change_sb_fg_color(
	x_screen_t *  screen ,
	char *  name
	)
{
	if( HAS_SCROLL_LISTENER(screen,change_fg_color))
	{
		(*screen->screen_scroll_listener->change_fg_color)(
			screen->screen_scroll_listener->self , name) ;
	}
}

static void
change_sb_bg_color(
	x_screen_t *  screen ,
	char *  name
	)
{
	if( HAS_SCROLL_LISTENER(screen,change_bg_color))
	{
		(*screen->screen_scroll_listener->change_bg_color)(
			screen->screen_scroll_listener->self , name) ;
	}
}

static void
larger_font_size(
	x_screen_t *  screen
	)
{
	x_larger_font( screen->font_man) ;

	font_size_changed( screen) ;

	/* this is because font_man->font_set may have changed in x_larger_font() */
	x_xic_font_set_changed( &screen->window) ;

	/* redrawing all lines with new fonts. */
	ml_term_set_modified_all( screen->term) ;
}

static void
smaller_font_size(
	x_screen_t *  screen
	)
{
	x_smaller_font( screen->font_man) ;

	font_size_changed( screen) ;
	
	/* this is because font_man->font_set may have changed in x_smaller_font() */
	x_xic_font_set_changed( &screen->window) ;

	/* redrawing all lines with new fonts. */
	ml_term_set_modified_all( screen->term) ;
	
	return ;
}

static void
change_transparent_flag(
	x_screen_t *  screen ,
	int  is_transparent
	)
{
	if( screen->window.is_transparent == is_transparent)
	{
		/* not changed */
		
		return ;
	}

	if( is_transparent)
	{
		x_window_set_transparent( &screen->window , get_picture_modifier( screen)) ;
	}
	else
	{
		x_window_unset_transparent( &screen->window) ;
		set_wall_picture( screen) ;
	}
	
	if( HAS_SCROLL_LISTENER(screen,transparent_state_changed))
	{
		(*screen->screen_scroll_listener->transparent_state_changed)(
			screen->screen_scroll_listener->self , is_transparent ,
			get_picture_modifier( screen)) ;
	}
}

static void
change_multi_col_char_flag(
	x_screen_t *  screen ,
	int  flag
	)
{
	if( flag)
	{
		x_use_multi_col_char( screen->font_man) ;
	}
	else
	{
		x_unuse_multi_col_char( screen->font_man) ;
	}

	ml_term_set_multi_col_char_flag( screen->term , flag) ;
}

static void
change_bidi_flag(
	x_screen_t *  screen ,
	int  use_bidi
	)
{
	if( screen->use_bidi == use_bidi)
	{
		/* not changed */
		
		return ;
	}

	screen->use_bidi = use_bidi ;

	if( update_special_visual( screen))
	{
		ml_term_set_modified_all( screen->term) ;
	}
}

static void
change_wall_picture(
	x_screen_t *  screen ,
	char *  file_path
	)
{
	if( screen->pic_file_path)
	{
		if( strcmp( screen->pic_file_path , file_path) == 0)
		{
			/* not changed */
			
			return ;
		}
		
		free( screen->pic_file_path) ;
	}

	screen->pic_file_path = strdup( file_path) ;

	set_wall_picture( screen) ;
}

static void
picture_modifier_changed(
	x_screen_t *  screen
	)
{
	if( screen->window.is_transparent)
	{
		x_window_set_transparent( &screen->window , get_picture_modifier( screen)) ;
		
		if( HAS_SCROLL_LISTENER(screen,transparent_state_changed))
		{
			(*screen->screen_scroll_listener->transparent_state_changed)(
				screen->screen_scroll_listener->self , 1 , get_picture_modifier( screen)) ;
		}
	}
	else
	{
		set_wall_picture( screen) ;
	}
}

static void
change_brightness(
	x_screen_t *  screen ,
	u_int  brightness
	)
{
	if( screen->pic_mod.brightness == brightness)
	{
		/* not changed */
		
		return ;
	}

	screen->pic_mod.brightness = brightness ;

	picture_modifier_changed( screen);
}
	
static void
change_contrast(
	x_screen_t *  screen ,
	u_int  contrast
	)
{
	if( screen->pic_mod.contrast == contrast)
	{
		/* not changed */
		
		return ;
	}

	screen->pic_mod.contrast = contrast ;
	
	picture_modifier_changed( screen);
}
	
static void
change_gamma(
	x_screen_t *  screen ,
	u_int  gamma
	)
{
	if( screen->pic_mod.gamma == gamma)
	{
		/* not changed */
		
		return ;
	}

	screen->pic_mod.gamma = gamma ;
	
	picture_modifier_changed( screen);
}
	
static void
change_fade_ratio(
	x_screen_t *  screen ,
	u_int  fade_ratio
	)
{
	if( screen->fade_ratio == fade_ratio)
	{
		/* not changed */
		
		return ;
	}

	screen->fade_ratio = fade_ratio ;

	x_color_manager_unfade( screen->color_man) ;

	if( ! screen->is_focused)
	{
		if( screen->fade_ratio < 100)
		{
			x_color_manager_fade( screen->color_man , screen->fade_ratio) ;
		}
	}

	x_window_set_fg_color( &screen->window ,
		x_get_color( screen->color_man , ML_FG_COLOR)->pixel) ;
	x_window_set_bg_color( &screen->window ,
		x_get_color( screen->color_man , ML_BG_COLOR)->pixel) ;
	
	x_xic_fg_color_changed( &screen->window) ;
	x_xic_bg_color_changed( &screen->window) ;

	ml_term_set_modified_all( screen->term) ;
}

static void
change_xim(
	x_screen_t *  screen ,
	char *  xim ,
	char *  locale
	)
{
	x_xic_deactivate( &screen->window) ;

	x_xic_activate( &screen->window , xim , locale) ;
}

static void
full_reset(
	x_screen_t *  screen
	)
{
	ml_term_init_encoding_parser( screen->term) ;
}

static void
set_config(
	void *  p ,
	char *  key ,
	char *  value
	)
{
	x_screen_t *  screen ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s=%s\n" , key , value) ;
#endif

	screen = p ;

	/*
	 * x_screen_{start|stop}_term_screen are necessary since
	 * window is redrawn in chagne_wall_picture().
	 */

	if( strcmp( key , "encoding") == 0)
	{
		ml_char_encoding_t  encoding ;

		if( ( encoding = ml_get_char_encoding( value)) == ML_UNKNOWN_ENCODING)
		{
			return ;
		}

		change_char_encoding( screen , encoding) ;
	}
	else if( strcmp( key , "iscii_lang") == 0)
	{
		ml_iscii_lang_type_t  type ;

		if( ( type = ml_iscii_get_lang( value)) == ISCIILANG_UNKNOWN)
		{
			return ;
		}
		
		change_iscii_lang( screen , type) ;
	}
	else if( strcmp( key , "fg_color") == 0)
	{
		change_fg_color( screen , value) ;
	}
	else if( strcmp( key , "bg_color") == 0)
	{
		change_bg_color( screen , value) ;
	}
	else if( strcmp( key , "cursor_fg_color") == 0)
	{
		change_cursor_fg_color( screen , value) ;
	}
	else if( strcmp( key , "cursor_bg_color") == 0)
	{
		change_cursor_bg_color( screen , value) ;
	}
	else if( strcmp( key , "sb_fg_color") == 0)
	{
		change_sb_fg_color( screen , value) ;
	}
	else if( strcmp( key , "sb_bg_color") == 0)
	{
		change_sb_bg_color( screen , value) ;
	}
	else if( strcmp( key , "tabsize") == 0)
	{
		u_int  tab_size ;

		if( ! kik_str_to_uint( &tab_size , value))
		{
			return ;
		}

		change_tab_size( screen , tab_size) ;
	}
	else if( strcmp( key , "logsize") == 0)
	{
		u_int  log_size ;

		if( ! kik_str_to_uint( &log_size , value))
		{
			return ;
		}

		change_log_size( screen , log_size) ;
	}
	else if( strcmp( key , "fontsize") == 0)
	{
		u_int  font_size ;

		if( strcmp( value , "larger") == 0)
		{
			larger_font_size( screen) ;
		}
		else if( strcmp( value , "smaller") == 0)
		{
			smaller_font_size( screen) ;
		}
		else
		{
			if( ! kik_str_to_uint( &font_size , value))
			{
				return ;
			}

			change_font_size( screen , font_size) ;
		}
	}
	else if( strcmp( key , "line_space") == 0)
	{
		u_int  line_space ;

		if( ! kik_str_to_uint( &line_space , value))
		{
			return ;
		}

		change_line_space( screen , line_space) ;
	}
	else if( strcmp( key , "screen_width_ratio") == 0)
	{
		u_int  ratio ;

		if( ! kik_str_to_uint( &ratio , value))
		{
			return ;
		}

		change_screen_width_ratio( screen , ratio) ;
	}
	else if( strcmp( key , "screen_height_ratio") == 0)
	{
		u_int  ratio ;

		if( ! kik_str_to_uint( &ratio , value))
		{
			return ;
		}

		change_screen_height_ratio( screen , ratio) ;
	}
	else if( strcmp( key , "scrollbar_view_name") == 0)
	{
		change_sb_view( screen , value) ;
	}
	else if( strcmp( key , "mod_meta_key") == 0)
	{
		change_mod_meta_key( screen , value) ;
	}
	else if( strcmp( key , "mod_meta_mode") == 0)
	{
		change_mod_meta_mode( screen , x_get_mod_meta_mode( value)) ;
	}
	else if( strcmp( key , "bel_mode") == 0)
	{
		change_bel_mode( screen , x_get_bel_mode( value)) ;
	}
	else if( strcmp( key , "vertical_mode") == 0)
	{
		change_vertical_mode( screen , ml_get_vertical_mode( value)) ;
	}
	else if( strcmp( key , "scrollbar_mode") == 0)
	{
		change_sb_mode( screen , x_get_sb_mode( value)) ;
	}
	else if( strcmp( key , "use_combining") == 0)
	{
		int  flag ;
		
		if( strcmp( value , "true") == 0)
		{
			flag = 1 ;
		}
		else if( strcmp( value , "false") == 0)
		{
			flag = 0 ;
		}
		else
		{
			return ;
		}
		
		change_char_combining_flag( screen , flag) ;
	}
	else if( strcmp( key , "use_dynamic_comb") == 0)
	{
		int  flag ;
		
		if( strcmp( value , "true") == 0)
		{
			flag = 1 ;
		}
		else if( strcmp( value , "false") == 0)
		{
			flag = 0 ;
		}
		else
		{
			return ;
		}

		change_dynamic_comb_flag( screen , flag) ;
	}
	else if( strcmp( key , "receive_string_via_ucs") == 0 ||
		/* backward compatibility with 2.6.1 or before */
		strcmp( key , "copy_paste_via_ucs") == 0)
	{
		int  flag ;
		
		if( strcmp( value , "true") == 0)
		{
			flag = 1 ;
		}
		else if( strcmp( value , "false") == 0)
		{
			flag = 0 ;
		}
		else
		{
			return ;
		}
		
		change_receive_string_via_ucs_flag( screen , flag) ;
	}
	else if( strcmp( key , "use_transbg") == 0)
	{
		int  flag ;
		
		if( strcmp( value , "true") == 0)
		{
			flag = 1 ;
		}
		else if( strcmp( value , "false") == 0)
		{
			flag = 0 ;
		}
		else
		{
			return ;
		}
		
		change_transparent_flag( screen , flag) ;
	}
	else if( strcmp( key , "brightness") == 0)
	{
		u_int  brightness ;

		if( ! kik_str_to_uint( &brightness , value))
		{
			return ;
		}

		change_brightness( screen , brightness) ;
	}
	else if( strcmp( key , "contrast") == 0)
	{
		u_int  contrast ;

		if( ! kik_str_to_uint( &contrast , value))
		{
			return ;
		}

		change_contrast( screen , contrast) ;
	}
	else if( strcmp( key , "gamma") == 0)
	{
		u_int  gamma ;

		if( ! kik_str_to_uint( &gamma , value))
		{
			return ;
		}

		change_gamma( screen , gamma) ;
	}
	else if( strcmp( key , "fade_ratio") == 0)
	{
		u_int  fade_ratio ;

		if( ! kik_str_to_uint( &fade_ratio , value))
		{
			return ;
		}

		change_fade_ratio( screen , fade_ratio) ;
	}
	else if( strcmp( key , "use_anti_alias") == 0)
	{
		x_font_present_t  font_present ;

		font_present = x_font_manager_get_font_present( screen->font_man) ;
		
		if( strcmp( value , "true") == 0)
		{
			font_present |= FONT_AA ;
		}
		else if( strcmp( value , "false") == 0)
		{
			font_present &= ~FONT_AA ;
		}
		else
		{
			return ;
		}
		
		change_font_present( screen , font_present) ;
	}
	else if( strcmp( key , "use_variable_column_width") == 0)
	{
		x_font_present_t  font_present ;

		font_present = x_font_manager_get_font_present( screen->font_man) ;
		
		if( strcmp( value , "true") == 0)
		{
			font_present |= FONT_VAR_WIDTH ;
		}
		else if( strcmp( value , "false") == 0)
		{
			font_present &= ~FONT_VAR_WIDTH ;
		}
		else
		{
			return ;
		}
		
		change_font_present( screen , font_present) ;
	}
	else if( strcmp( key , "use_multi_column_char") == 0)
	{
		int  flag ;
		
		if( strcmp( value , "true") == 0)
		{
			flag = 1 ;
		}
		else if( strcmp( value , "false") == 0)
		{
			flag = 0 ;
		}
		else
		{
			return ;
		}
		
		change_multi_col_char_flag( screen , flag) ;
	}
	else if( strcmp( key , "use_bidi") == 0)
	{
		int  flag ;
		
		if( strcmp( value , "true") == 0)
		{
			flag = 1 ;
		}
		else if( strcmp( value , "false") == 0)
		{
			flag = 0 ;
		}
		else
		{
			return ;
		}
		
		change_bidi_flag( screen , flag) ;
	}
	else if( strcmp( key , "xim") == 0)
	{
		char *  xim ;
		char *  locale ;
		char *  p ;

		xim = value ;

		if( ( p = strchr( value , ':')) == NULL)
		{
			locale = "" ;
		}
		else
		{
			*p = '\0' ;
			locale = p + 1 ;
		}

		change_xim( screen , xim , locale) ;
	}
	else if( strcmp( key , "wall_picture") == 0)
	{
		change_wall_picture( screen , value) ;
	}
	else if( strcmp( key , "full_reset") == 0)
	{
		full_reset( screen) ;
	}
	else if( strcmp( key , "select_pty") == 0)
	{
		if( HAS_SYSTEM_LISTENER(screen,open_pty))
		{
			(*screen->system_listener->open_pty)( screen->system_listener->self , screen ,
				value) ;
		}
	}

	redraw_screen( screen) ;
	highlight_cursor( screen) ;
}

static void
get_config(
	void *  p ,
	char *  key ,
	int  to_cfg
	)
{
	x_screen_t *  screen ;
	char *  value ;
	char  digit[DIGIT_STR_LEN(u_int) + 1] ;
	char *  true = "true" ;
	char *  false = "false" ;
	char  cwd[PATH_MAX] ;

	screen = p ;
	
	if( strcmp( key , "encoding") == 0)
	{
		value = ml_get_char_encoding_name( ml_term_get_encoding( screen->term)) ;
	}
	else if( strcmp( key , "iscii_lang") == 0)
	{
		value = ml_iscii_get_lang_name( screen->iscii_lang_type) ;
	}
	else if( strcmp( key , "fg_color") == 0)
	{
		value = x_color_manager_get_fg_color( screen->color_man) ;
	}
	else if( strcmp( key , "bg_color") == 0)
	{
		value = x_color_manager_get_bg_color( screen->color_man) ;
	}
	else if( strcmp( key , "cursor_fg_color") == 0)
	{
		if( ( value = x_color_manager_get_cursor_fg_color( screen->color_man)) == NULL)
		{
			value = "" ;
		}
	}
	else if( strcmp( key , "cursor_bg_color") == 0)
	{
		if( ( value = x_color_manager_get_cursor_bg_color( screen->color_man)) == NULL)
		{
			value = "" ;
		}
	}
	else if( strcmp( key , "sb_fg_color") == 0)
	{
		if( screen->screen_scroll_listener && screen->screen_scroll_listener->fg_color)
		{
			value = (*screen->screen_scroll_listener->fg_color)(
					screen->screen_scroll_listener->self) ;
		}
		else
		{
			value = NULL ;
		}
	}
	else if( strcmp( key , "sb_bg_color") == 0)
	{
		if( screen->screen_scroll_listener && screen->screen_scroll_listener->bg_color)
		{
			value = (*screen->screen_scroll_listener->bg_color)(
					screen->screen_scroll_listener->self) ;
		}
		else
		{
			value = NULL ;
		}
	}
	else if( strcmp( key , "tabsize") == 0)
	{
		sprintf( digit , "%d" , ml_term_get_tab_size( screen->term)) ;
		value = digit ;
	}
	else if( strcmp( key , "logsize") == 0)
	{
		sprintf( digit , "%d" , ml_term_get_log_size( screen->term)) ;
		value = digit ;
	}
	else if( strcmp( key , "fontsize") == 0)
	{
		sprintf( digit , "%d" , screen->font_man->font_size) ;
		value = digit ;
	}
	else if( strcmp( key , "line_space") == 0)
	{
		sprintf( digit , "%d" , screen->line_space) ;
		value = digit ;
	}
	else if( strcmp( key , "screen_width_ratio") == 0)
	{
		sprintf( digit , "%d" , screen->screen_width_ratio) ;
		value = digit ;
	}
	else if( strcmp( key , "screen_height_ratio") == 0)
	{
		sprintf( digit , "%d" , screen->screen_height_ratio) ;
		value = digit ;
	}
	else if( strcmp( key , "scrollbar_view_name") == 0)
	{
		if( screen->screen_scroll_listener && screen->screen_scroll_listener->view_name)
		{
			value = (*screen->screen_scroll_listener->view_name)(
					screen->screen_scroll_listener->self) ;
		}
		else
		{
			value = NULL ;
		}
	}
	else if( strcmp( key , "mod_meta_key") == 0)
	{
		if( screen->mod_meta_key == NULL)
		{
			value = "none" ;
		}
		else
		{
			value = screen->mod_meta_key ;
		}
	}
	else if( strcmp( key , "mod_meta_mode") == 0)
	{
		value = x_get_mod_meta_mode_name( screen->mod_meta_mode) ;
	}
	else if( strcmp( key , "bel_mode") == 0)
	{
		value = x_get_bel_mode_name( screen->bel_mode) ;
	}
	else if( strcmp( key , "vertical_mode") == 0)
	{
		value = ml_get_vertical_mode_name( screen->vertical_mode) ;
	}
	else if( strcmp( key , "scrollbar_mode") == 0)
	{
		if( screen->screen_scroll_listener &&
			screen->screen_scroll_listener->sb_mode)
		{
			value = x_get_sb_mode_name( (*screen->screen_scroll_listener->sb_mode)(
				screen->screen_scroll_listener->self)) ;
		}
		else
		{
			value = x_get_sb_mode_name( SB_NONE) ;
		}
	}
	else if( strcmp( key , "use_combining") == 0)
	{
		if( ml_term_is_using_char_combining( screen->term))
		{
			value = true ;
		}
		else
		{
			value = false ;
		}
	}
	else if( strcmp( key , "use_dynamic_comb") == 0)
	{
		if( screen->use_dynamic_comb)
		{
			value = true ;
		}
		else
		{
			value = false ;
		}
	}
	else if( strcmp( key , "receive_string_via_ucs") == 0 ||
		/* backward compatibility with 2.6.1 or before */
		strcmp( key , "copy_paste_via_ucs") == 0)
	{
		if( screen->receive_string_via_ucs)
		{
			value = true ;
		}
		else
		{
			value = false ;
		}
	}
	else if( strcmp( key , "use_transbg") == 0)
	{
		if( screen->window.is_transparent)
		{
			value = true ;
		}
		else
		{
			value = false ;
		}
	}
	else if( strcmp( key , "brightness") == 0)
	{
		sprintf( digit , "%d" , screen->pic_mod.brightness) ;
		value = digit ;
	}
	else if( strcmp( key , "contrast") == 0)
	{
		sprintf( digit , "%d" , screen->pic_mod.contrast) ;
		value = digit ;
	}
	else if( strcmp( key , "gamma") == 0)
	{
		sprintf( digit , "%d" , screen->pic_mod.gamma) ;
		value = digit ;
	}
	else if( strcmp( key , "fade_ratio") == 0)
	{
		sprintf( digit , "%d" , screen->fade_ratio) ;
		value = digit ;
	}
	else if( strcmp( key , "use_anti_alias") == 0)
	{
		if( x_font_manager_get_font_present( screen->font_man) & FONT_AA)
		{
			value = true ;
		}
		else
		{
			value = false ;
		}
	}
	else if( strcmp( key , "use_variable_column_width") == 0)
	{
		if( x_font_manager_get_font_present( screen->font_man) & FONT_VAR_WIDTH)
		{
			value = true ;
		}
		else
		{
			value = false ;
		}
	}
	else if( strcmp( key , "use_multi_column_char") == 0)
	{
		if( screen->font_man->use_multi_col_char)
		{
			value = true ;
		}
		else
		{
			value = false ;
		}
	}
	else if( strcmp( key , "use_bidi") == 0)
	{
		if( screen->use_bidi)
		{
			value = true ;
		}
		else
		{
			value = false ;
		}
	}
	else if( strcmp( key , "xim") == 0)
	{
		value = x_xic_get_xim_name( &screen->window) ;
	}
	else if( strcmp( key , "locale") == 0)
	{
		value = kik_get_locale() ;
	}
	else if( strcmp( key , "wall_picture") == 0)
	{
		if( screen->pic_file_path)
		{
			value = screen->pic_file_path ;
		}
		else
		{
			value = "" ;
		}
	}
	else if( strcmp( key , "pwd") == 0)
	{
		value = getcwd( cwd , PATH_MAX) ;
	}
	else if( strcmp( key , "rows") == 0)
	{
		sprintf( digit , "%d" , ml_term_get_rows( screen->term)) ;
		value = digit ;
	}
	else if( strcmp( key , "cols") == 0)
	{
		sprintf( digit , "%d" , ml_term_get_cols( screen->term)) ;
		value = digit ;
	}
	else if( strcmp( key , "pty_list") == 0)
	{
		if( HAS_SYSTEM_LISTENER(screen,pty_list))
		{
			value = (*screen->system_listener->pty_list)( screen->system_listener->self) ;
		}
	}
	else if( strcmp( key , "pty_name") == 0)
	{
		value = ml_term_get_slave_name( screen->term) ;
	}
	else
	{
		goto  error ;
	}

	if( value == NULL)
	{
		goto  error ;
	}

	ml_term_write( screen->term , "#" , 1 , to_cfg) ;
	ml_term_write( screen->term , key , strlen( key) , to_cfg) ;
	ml_term_write( screen->term , "=" , 1 , to_cfg) ;
	ml_term_write( screen->term , value , strlen( value) , to_cfg) ;
	ml_term_write( screen->term , "\n" , 1 , to_cfg) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " #%s=%s\n" , key , value) ;
#endif

	return ;

error:
	ml_term_write( screen->term , "#error\n" , 7 , to_cfg) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " #error\n") ;
#endif

	return ;
}


/*
 * callbacks of x_sel_event_listener_t events.
 */
 
static void
reverse_color(
	void *  p ,
	int  beg_char_index ,
	int  beg_row ,
	int  end_char_index ,
	int  end_row
	)
{
	x_screen_t *  screen ;

	screen = (x_screen_t*)p ;
	
	beg_char_index = abs( beg_char_index) ;
	end_char_index = abs( end_char_index) ;

	if( ml_line_is_rtl( ml_term_get_line( screen->term , beg_row)) && beg_row == end_row)
	{
		int  buf ;

		buf = beg_char_index ;
		beg_char_index = end_char_index ;
		end_char_index = buf ;
	}
	
	ml_term_reverse_color( screen->term , beg_char_index , beg_row ,
		end_char_index , end_row) ;
}

static void
restore_color(
	void *  p ,
	int  beg_char_index ,
	int  beg_row ,
	int  end_char_index ,
	int  end_row
	)
{
	x_screen_t *  screen ;

	screen = (x_screen_t*)p ;
	
	beg_char_index = abs( beg_char_index) ;
	end_char_index = abs( end_char_index) ;

	if( ml_line_is_rtl( ml_term_get_line( screen->term , beg_row)) && beg_row == end_row)
	{
		int  buf ;

		buf = beg_char_index ;
		beg_char_index = end_char_index ;
		end_char_index = buf ;
	}

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " restoring region %d %d %d %d.\n" ,
		beg_char_index , beg_row , end_char_index , end_row) ;
#endif

	ml_term_restore_color( screen->term , beg_char_index , beg_row ,
		end_char_index , end_row) ;
}

static int
select_in_window(
	void *  p ,
	ml_char_t **  chars ,
	u_int *  len ,
	int  beg_char_index ,
	int  beg_row ,
	int  end_char_index ,
	int  end_row
	)
{
	x_screen_t *  screen ;
	u_int  size ;
	ml_char_t *  buf ;

	screen = p ;

	beg_char_index = abs( beg_char_index) ;
	end_char_index = abs( end_char_index) ;

	if( ml_line_is_rtl( ml_term_get_line( screen->term , beg_row)) && beg_row == end_row)
	{
		int  buf ;

		buf = beg_char_index ;
		beg_char_index = end_char_index ;
		end_char_index = buf ;
	}

	if( ( size = ml_term_get_region_size( screen->term , beg_char_index , beg_row ,
			end_char_index , end_row)) == 0)
	{
		return  0 ;
	}

	if( ( buf = ml_str_alloca( size)) == NULL)
	{
		return  0 ;
	}

	*len = ml_term_copy_region( screen->term , buf , size , beg_char_index ,
		beg_row , end_char_index , end_row) ;

#ifdef  DEBUG
	if( size != *len)
	{
		kik_warn_printf( KIK_DEBUG_TAG
			" ml_term_get_region_size() == %d and ml_term_copy_region() == %d"
			" are not the same size !\n" ,
			size , *len) ;
	}
#endif

	if( (*chars = ml_str_new( size)) == NULL)
	{
		return  0 ;
	}

	ml_str_copy( *chars , buf , size) ;

	ml_str_final( buf , size) ;
	
	return  1 ;
}


/*
 * callbacks of ml_screen_event_listener_t events.
 */
 
static int
window_scroll_upward_region(
	void *  p ,
	int  beg_row ,
	int  end_row ,
	u_int  size
	)
{
	x_screen_t *  screen ;

	screen = p ;

	if( screen->vertical_mode || ! x_window_is_scrollable( &screen->window))
	{
		return  0 ;
	}

	set_scroll_boundary( screen , beg_row , end_row) ;
	
	screen->scroll_cache_rows += size ;

	return  1 ;
}

static int
window_scroll_downward_region(
	void *  p ,
	int  beg_row ,
	int  end_row ,
	u_int  size
	)
{
	x_screen_t *  screen ;

	screen = p ;

	if( screen->vertical_mode || ! x_window_is_scrollable( &screen->window))
	{
		return  0 ;
	}

	set_scroll_boundary( screen , beg_row , end_row) ;
	
	screen->scroll_cache_rows -= size ;

	return  1 ;
}

static void
line_scrolled_out(
	void *  p
	)
{
	x_screen_t *  screen ;
	
	screen = p ;

	x_sel_line_scrolled_out( &screen->sel , -((int)ml_term_get_log_size( screen->term))) ;
}

/*
 * callbacks of x_xim events.
 */
 
/*
 * this doesn't consider backscroll mode.
 */
static int
get_spot(
	void *  p ,
	int *  x ,
	int *  y
	)
{
	x_screen_t *  screen ;
	ml_line_t *  line ;

	screen = p ;
	
	if( ( line = ml_term_get_cursor_line( screen->term)) == NULL ||
		ml_line_is_empty( line))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " cursor line doesn't exist ?.\n") ;
	#endif
	
		return  0 ;
	}
	
	*y = convert_row_to_y( screen , ml_term_cursor_row( screen->term)) +
		x_line_height( screen) ;
	
	*x = convert_char_index_to_x_with_shape( screen , line ,
		ml_term_cursor_char_index( screen->term)) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " xim spot => x %d y %d\n" , *x , *y) ;
#endif

	return  1 ;
}

static XFontSet
get_fontset(
	void *  p
	)
{
	x_screen_t *  screen ;
	
	screen = p ;

	return  x_get_fontset( screen->font_man) ;
}

static u_long
get_fg_color(
	void *  p
	)
{
	x_screen_t *  screen ;
	
	screen = p ;

	return  x_get_color( screen->color_man , ML_FG_COLOR)->pixel ;
}

static u_long
get_bg_color(
	void *  p
	)
{
	x_screen_t *  screen ;
	
	screen = p ;

	return  x_get_color( screen->color_man , ML_BG_COLOR)->pixel ;
}

/*
 * callbacks of ml_xterm_event_listener_t
 */
 
static void
start_vt100_cmd(
	void *  p
	)
{
	x_screen_t *  screen ;

	screen = p ;

#if  0
	if( ! ml_term_is_backscrolling( screen->term) ||
		ml_term_is_backscrolling( screen->term) == BSM_VOLATILE)
	{
		x_stop_selecting( &screen->sel) ;
	}
#endif

	if( screen->sel.is_selecting)
	{
		x_restore_selected_region_color_except_logs( &screen->sel) ;
	}
	else
	{
		restore_selected_region_color( screen) ;
	}
	
	unhighlight_cursor( screen) ;
}

static void
stop_vt100_cmd(
	void *  p
	)
{
	x_screen_t *  screen ;

	screen = p ;

	if( screen->sel.is_selecting)
	{
		x_reverse_selected_region_color_except_logs( &screen->sel) ;
	}
	
	redraw_screen( screen) ;
	highlight_cursor( screen) ;
}

static void
xterm_set_app_keypad(
	void *  p ,
	int  flag
	)
{
	x_screen_t *  screen ;

	screen = p ;

	ml_term_set_app_keypad( screen->term , flag) ;
}

static void
xterm_set_app_cursor_keys(
	void *  p ,
	int  flag
	)
{
	x_screen_t *  screen ;

	screen = p ;

	ml_term_set_app_cursor_keys( screen->term , flag) ;
}

static void
xterm_resize_columns(
	void *  p ,
	u_int  cols
	)
{
	x_screen_t *  screen ;

	screen = p ;

	if( cols == ml_term_get_logical_cols( screen->term))
	{
		return ;
	}

	/*
	 * x_screen_{start|stop}_term_screen are necessary since
	 * window is redrawn in window_resized().
	 */
	
	x_window_resize( &screen->window , x_col_width(screen) * cols ,
		x_line_height(screen) * ml_term_get_rows(screen->term) ,
		NOTIFY_TO_PARENT) ;

	/*
	 * !! Notice !!
	 * x_window_resize() will invoke ConfigureNotify event but window_resized() won't be
	 * called , since xconfigure.width , xconfigure.height are the same as the already
	 * resized window.
	 */
	if( screen->window.window_resized)
	{
		(*screen->window.window_resized)( &screen->window) ;
	}
}

static void
xterm_reverse_video(
	void *  p ,
	int  do_reverse
	)
{
	x_screen_t *  screen ;

	screen = p ;
	
	if( do_reverse)
	{
		if( ! x_color_manager_reverse_video( screen->color_man))
		{
			return ;
		}
	}
	else
	{
		if( ! x_color_manager_restore_video( screen->color_man))
		{
			return ;
		}
	}
	
	x_window_set_fg_color( &screen->window ,
		x_get_color( screen->color_man , ML_FG_COLOR)->pixel) ;
	x_window_set_bg_color( &screen->window ,
		x_get_color( screen->color_man , ML_BG_COLOR)->pixel) ;
	
	ml_term_set_modified_all( screen->term) ;

	redraw_screen( screen) ;
}

static void
xterm_set_mouse_report(
	void *  p ,
	int  flag
	)
{
	x_screen_t *  screen ;

	screen = p ;
	
	if( flag)
	{
		x_stop_selecting( &screen->sel) ;
		restore_selected_region_color( screen) ;
		
		exit_backscroll_mode( screen) ;
	}

	ml_term_set_mouse_report( screen->term , flag) ;
}

static void
xterm_set_window_name(
	void *  p ,
	u_char *  name
	)
{
	x_screen_t *  screen ;

	screen = p ;
	
	x_set_window_name( &screen->window , name) ;
}

static void
xterm_set_icon_name(
	void *  p ,
	u_char *  name
	)
{
	x_screen_t *  screen ;

	screen = p ;
	
	x_set_icon_name( &screen->window , name) ;
}

static void
xterm_bel(
	void *  p
	)
{	
	x_screen_t *  screen ;

	screen = p ;
	
	if( screen->bel_mode == BEL_SOUND)
	{
		XBell( screen->window.display , 0) ;
	}
	else if( screen->bel_mode == BEL_VISUAL)
	{
		x_window_fill_all( &screen->window) ;

		XFlush( screen->window.display) ;

		x_window_clear_all( &screen->window) ;
		ml_term_set_modified_all( screen->term) ;
		redraw_screen( screen) ;
	}
}


/*
 * callbacks of ml_pty_event_listener_t
 */

static void
pty_closed(
	void *  p
	)
{
	x_screen_t *  screen ;

	screen = p ;

	screen->term = NULL ;
	(*screen->system_listener->pty_closed)( screen->system_listener->self , screen) ;
}


/* --- global functions --- */

x_screen_t *
x_screen_new(
	ml_term_t *  term ,
	x_font_manager_t *  font_man ,
	x_color_manager_t *  color_man ,
	x_termcap_entry_t *  termcap ,
	u_int  brightness ,
	u_int  contrast ,
	u_int  gamma ,
	u_int  fade_ratio ,
	x_shortcut_t *  shortcut ,
	u_int  screen_width_ratio ,
	u_int  screen_height_ratio ,
	int  xim_open_in_startup ,
	char *  mod_meta_key ,
	x_mod_meta_mode_t  mod_meta_mode ,
	x_bel_mode_t  bel_mode ,
	int  receive_string_via_ucs ,
	char *  pic_file_path ,
	int  use_transbg ,
	int  use_bidi ,
	ml_vertical_mode_t  vertical_mode ,
	int  use_vertical_cursor ,
	int  big5_buggy ,
	char *  conf_menu_path ,
	ml_iscii_lang_type_t  iscii_lang_type ,
	int  use_extended_scroll_shortcut ,
	int  use_dynamic_comb ,
	u_int  line_space
	)
{
	x_screen_t *  screen ;
	
	if( ( screen = malloc( sizeof( x_screen_t))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
	#endif
	
		return  NULL ;
	}

	screen->line_space = line_space ;

	screen->screen_listener.self = screen ;
	screen->screen_listener.window_scroll_upward_region = window_scroll_upward_region ;
	screen->screen_listener.window_scroll_downward_region = window_scroll_downward_region ;
	screen->screen_listener.line_scrolled_out = line_scrolled_out ;

	screen->xterm_listener.self = screen ;
	screen->xterm_listener.start = start_vt100_cmd ;
	screen->xterm_listener.stop = stop_vt100_cmd ;
	screen->xterm_listener.set_app_keypad = xterm_set_app_keypad ;
	screen->xterm_listener.set_app_cursor_keys = xterm_set_app_cursor_keys ;
	screen->xterm_listener.resize_columns = xterm_resize_columns ;
	screen->xterm_listener.reverse_video = xterm_reverse_video ;
	screen->xterm_listener.set_mouse_report = xterm_set_mouse_report ;
	screen->xterm_listener.set_window_name = xterm_set_window_name ;
	screen->xterm_listener.set_icon_name = xterm_set_icon_name ;
	screen->xterm_listener.bel = xterm_bel ;

	screen->config_listener.self = screen ;
	screen->config_listener.set = set_config ;
	screen->config_listener.get = get_config ;

	screen->pty_listener.self = screen ;
	screen->pty_listener.closed = pty_closed ;

	ml_term_set_listener( term , &screen->xterm_listener , &screen->config_listener ,
		&screen->screen_listener , &screen->pty_listener) ;
	
	screen->term = term ;
	
	/* allocated dynamically */
	screen->utf8_parser = NULL ;
	screen->xct_parser = NULL ;
	screen->ml_str_parser = NULL ;
	screen->utf8_conv = NULL ;
	screen->xct_conv = NULL ;
	
	screen->shape = NULL ;
	screen->kbd = NULL ;

	screen->iscii_lang_type = iscii_lang_type ;

	screen->vertical_mode = vertical_mode ;
	screen->use_vertical_cursor = use_vertical_cursor ;
	
	screen->font_man = font_man ;
	
	if( ml_term_get_encoding( screen->term) == ML_ISCII)
	{
		/*
		 * ISCII needs variable column width and character combining.
		 * (similar processing is done in change_char_encoding)
		 */

		if( ! ( x_font_manager_get_font_present( screen->font_man) & FONT_VAR_WIDTH))
		{
			x_font_manager_change_font_present( screen->font_man ,
				x_font_manager_get_font_present( screen->font_man) | FONT_VAR_WIDTH) ;
		}

		ml_term_set_char_combining_flag( screen->term , 1) ;
	}
	
	screen->use_bidi = use_bidi ;

	screen->color_man = color_man ;
	
	screen->sel_listener.self = screen ;
	screen->sel_listener.select_in_window = select_in_window ;
	screen->sel_listener.reverse_color = reverse_color ;
	screen->sel_listener.restore_color = restore_color ;

	if( ! x_sel_init( &screen->sel , &screen->sel_listener))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_sel_init failed.\n") ;
	#endif
	
		goto  error ;
	}

	screen->pic_mod.brightness = brightness ;
	screen->pic_mod.contrast = contrast ;
	screen->pic_mod.gamma = gamma ;

	screen->fade_ratio = fade_ratio ;
	screen->is_focused = 0 ;

	screen->screen_width_ratio = screen_width_ratio ;
	screen->screen_height_ratio = screen_height_ratio ;
	
	if( x_window_init( &screen->window ,
		screen_width( screen) , screen_height( screen) ,
		0 , 0 , x_col_width( screen) , x_line_height( screen) , 2) == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_window_init failed.\n") ;
	#endif
	
		goto  error ;
	}

	screen->xim_listener.self = screen ;
	screen->xim_listener.get_spot = get_spot ;
	screen->xim_listener.get_fontset = get_fontset ;
	screen->xim_listener.get_fg_color = get_fg_color ;
	screen->xim_listener.get_bg_color = get_bg_color ;
	screen->window.xim_listener = &screen->xim_listener ;

	screen->xim_open_in_startup = xim_open_in_startup ;

	x_window_set_cursor( &screen->window , XC_xterm) ;

	/*
	 * event call backs.
	 */

	x_window_init_event_mask( &screen->window ,
		ButtonPressMask | ButtonMotionMask | ButtonReleaseMask | KeyPressMask) ;

	screen->window.window_realized = window_realized ;
	screen->window.window_finalized = window_finalized ;
	screen->window.window_exposed = window_exposed ;
	screen->window.window_focused = window_focused ;
	screen->window.window_unfocused = window_unfocused ;
	screen->window.key_pressed = key_pressed ;
	screen->window.window_resized = window_resized ;
	screen->window.button_motion = button_motion ;
	screen->window.button_released = button_released ;
	screen->window.button_pressed = button_pressed ;
	screen->window.button_press_continued = button_press_continued ;
	screen->window.selection_cleared = selection_cleared ;
	screen->window.xct_selection_requested = xct_selection_requested ;
	screen->window.utf8_selection_requested = utf8_selection_requested ;
	screen->window.xct_selection_notified = xct_selection_notified ;
	screen->window.utf8_selection_notified = utf8_selection_notified ;
	screen->window.window_deleted = window_deleted ;

	if( use_transbg)
	{
		x_window_set_transparent( &screen->window , get_picture_modifier( screen)) ;
	}
	
	if( pic_file_path)
	{
		screen->pic_file_path = strdup( pic_file_path) ;
	}
	else
	{
		screen->pic_file_path = NULL ;
	}

	screen->shortcut = shortcut ;
	screen->termcap = termcap ;

	if( mod_meta_key && strcmp( mod_meta_key , "none") != 0)
	{
		screen->mod_meta_key = strdup( mod_meta_key) ;
	}
	else
	{
		screen->mod_meta_key = NULL ;
	}
	screen->mod_meta_mode = mod_meta_mode ;
	screen->mod_meta_mask = 0 ;		/* set later in get_mod_meta_mask() */

	screen->bel_mode = bel_mode ;
	
	screen->use_extended_scroll_shortcut = use_extended_scroll_shortcut ;
	screen->use_dynamic_comb = use_dynamic_comb ;

	/*
	 * for receiving selection.
	 */

	if( ( screen->utf8_parser = mkf_utf8_parser_new()) == NULL)
	{
		goto  error ;
	}
	
	if( ( screen->xct_parser = mkf_xct_parser_new()) == NULL)
	{
		goto  error ;
	}

	/*
	 * for sending selection
	 */
	 
	if( ( screen->ml_str_parser = ml_str_parser_new()) == NULL)
	{
		goto  error ;
	}

	if( ( screen->utf8_conv = mkf_utf8_conv_new()) == NULL)
	{
		goto  error ;
	}

	if( big5_buggy)
	{
		if( ( screen->xct_conv = mkf_xct_big5_buggy_conv_new()) == NULL)
		{
			goto  error ;
		}
	}
	else if( ( screen->xct_conv = mkf_xct_conv_new()) == NULL)
	{
		goto  error ;
	}

	screen->receive_string_via_ucs = receive_string_via_ucs ;

	screen->system_listener = NULL ;
	screen->screen_scroll_listener = NULL ;

	screen->scroll_cache_rows = 0 ;
	screen->scroll_cache_boundary_start = 0 ;
	screen->scroll_cache_boundary_end = 0 ;

	update_special_visual( screen) ;

	return  screen ;

error:
	if( screen->utf8_parser)
	{
		(*screen->utf8_parser->delete)( screen->utf8_parser) ;
	}
	
	if( screen->xct_parser)
	{
		(*screen->xct_parser->delete)( screen->xct_parser) ;
	}
	
	if( screen->ml_str_parser)
	{
		(*screen->ml_str_parser->delete)( screen->ml_str_parser) ;
	}
	
	if( screen->utf8_conv)
	{
		(*screen->utf8_conv->delete)( screen->utf8_conv) ;
	}
	
	if( screen->xct_conv)
	{
		(*screen->xct_conv->delete)( screen->xct_conv) ;
	}
	
	if( screen)
	{
		free( screen) ;
	}

	return  NULL ;
}

int
x_screen_delete(
	x_screen_t *  screen
	)
{
	if( screen->term)
	{
		ml_term_set_listener( screen->term , NULL , NULL , NULL , NULL) ;
	}
	
	x_sel_final( &screen->sel) ;

	if( screen->shape)
	{
		(*screen->shape->delete)( screen->shape) ;
	}

	if( screen->kbd)
	{
		(*screen->kbd->delete)( screen->kbd) ;
	}

	if( screen->mod_meta_key)
	{
		free( screen->mod_meta_key) ;
	}

	if( screen->pic_file_path)
	{
		free( screen->pic_file_path) ;
	}
	
	if( screen->utf8_parser)
	{
		(*screen->utf8_parser->delete)( screen->utf8_parser) ;
	}
	
	if( screen->xct_parser)
	{
		(*screen->xct_parser->delete)( screen->xct_parser) ;
	}
	
	if( screen->ml_str_parser)
	{
		(*screen->ml_str_parser->delete)( screen->ml_str_parser) ;
	}
	
	if( screen->utf8_conv)
	{
		(*screen->utf8_conv->delete)( screen->utf8_conv) ;
	}
	
	if( screen->xct_conv)
	{
		(*screen->xct_conv->delete)( screen->xct_conv) ;
	}
	
	free( screen) ;

	return  1 ;
}

int
x_screen_attach(
	x_screen_t *  screen ,
	ml_term_t *  term
	)
{
	if( screen->term)
	{
		return  0 ;
	}
	
	screen->term = term ;

	ml_term_set_listener( term , &screen->xterm_listener , &screen->config_listener ,
		&screen->screen_listener , &screen->pty_listener) ;

	if( ml_term_get_encoding( screen->term) == ML_ISCII)
	{
		/*
		 * ISCII needs variable column width and character combining.
		 */

		if( ! ( x_font_manager_get_font_present( screen->font_man) & FONT_VAR_WIDTH))
		{
			change_font_present( screen ,
				x_font_manager_get_font_present( screen->font_man) | FONT_VAR_WIDTH) ;
		}

		ml_term_set_char_combining_flag( screen->term , 1) ;
	}

	usascii_font_cs_changed( screen , ml_term_get_encoding( screen->term)) ;

	if( update_special_visual( screen))
	{
		ml_term_set_modified_all( screen->term) ;
	}

	if( HAS_SCROLL_LISTENER(screen,term_changed))
	{
		(*screen->screen_scroll_listener->term_changed)(
			screen->screen_scroll_listener->self ,
			ml_term_get_log_size( screen->term) ,
			ml_term_get_num_of_logged_lines( screen->term)) ;
	}

	redraw_screen( screen) ;
	highlight_cursor( screen) ;
	
	return  1 ;
}

ml_term_t *
x_screen_detach(
	x_screen_t *  screen
	)
{
	ml_term_t *  term ;
	
	if( screen->term == NULL)
	{
		return  NULL ;
	}

#if  1
	exit_backscroll_mode( screen) ;
#endif

	ml_term_set_listener( screen->term , NULL , NULL , NULL , NULL) ;

	term = screen->term ;
	screen->term = NULL ;

	x_window_clear_all( &screen->window) ;

	return  term ;
}

int
x_set_system_listener(
	x_screen_t *  screen ,
	x_system_event_listener_t *  system_listener
	)
{
	if( screen->system_listener)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " system listener is already set.\n") ;
	#endif
	
		return  0 ;
	}

	screen->system_listener = system_listener ;

	return  1 ;
}

int
x_set_screen_scroll_listener(
	x_screen_t *  screen ,
	x_screen_scroll_event_listener_t *  screen_scroll_listener
	)
{
	if( screen->screen_scroll_listener)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " screen scroll listener is already set.\n") ;
	#endif
	
		return  0 ;
	}

	screen->screen_scroll_listener = screen_scroll_listener ;

	return  1 ;
}


/*
 * for scrollbar scroll.
 *
 * Similar processing is done in bs_xxx().
 */
 
int
x_screen_scroll_upward(
	x_screen_t *  screen ,
	u_int  size
	)
{
	unhighlight_cursor( screen) ;
	
	if( ! ml_term_is_backscrolling( screen->term))
	{
		enter_backscroll_mode( screen) ;
	}

	ml_term_backscroll_upward( screen->term , size) ;
	
	redraw_screen( screen) ;
	highlight_cursor( screen) ;
	
	return  1 ;
}

int
x_screen_scroll_downward(
	x_screen_t *  screen ,
	u_int  size
	)
{
	unhighlight_cursor( screen) ;
	
	if( ! ml_term_is_backscrolling( screen->term))
	{
		enter_backscroll_mode( screen) ;
	}

	ml_term_backscroll_downward( screen->term , size) ;

	redraw_screen( screen) ;
	highlight_cursor( screen) ;

	return  1 ;
}

int
x_screen_scroll_to(
	x_screen_t *  screen ,
	int  row
	)
{
	unhighlight_cursor( screen) ;

	if( ! ml_term_is_backscrolling( screen->term))
	{
		enter_backscroll_mode( screen) ;
	}
	
	ml_term_backscroll_to( screen->term , row) ;

	redraw_screen( screen) ;
	highlight_cursor( screen) ;

	return  1 ;
}


u_int
x_col_width(
	x_screen_t *  screen
	)
{
	return  x_get_usascii_font( screen->font_man)->width ;
}

u_int
x_line_height(
	x_screen_t *  screen
	)
{
	return  x_get_usascii_font( screen->font_man)->height + screen->line_space ;
}

u_int
x_line_height_to_baseline(
	x_screen_t *  screen
	)
{
	return  x_get_usascii_font( screen->font_man)->height_to_baseline +
			screen->line_space / 2 ;
}

u_int
x_line_top_margin(
	x_screen_t *  screen
	)
{
	return  screen->line_space / 2 ;
}

u_int
x_line_bottom_margin(
	x_screen_t *  screen
	)
{
	return  screen->line_space / 2 + screen->line_space % 2 ;
}


x_picture_modifier_t *
x_screen_get_picture_modifier(
	x_screen_t *  screen
	)
{
	return  get_picture_modifier( screen) ;
}
