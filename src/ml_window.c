/*
 *	$Id$
 */

#include  "ml_window.h"

#ifdef  DEBUG
#include  <stdio.h>		/* fprintf */
#endif

#include  <string.h>		/* memset/memcpy */
#include  <X11/Xutil.h>		/* for XSizeHints */
#include  <X11/Xatom.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>	/* alloca/realloc */
#include  <mkf/mkf_charset.h>	/* mkf_charset */

#include  "ml_xic.h"
#include  "ml_window_manager.h"
#include  "ml_window_intern.h"
#include  "ml_char_encoding.h"	/* ml_convert_to_xft_ucs4 */


#define  DOUBLE_CLICK_INTERVAL  1000	/* mili second */

#define  MAX_CLICK  3			/* max is triple click */

#if  0
#define  __DEBUG
#endif

#if  0
#define  PERF_DEBUG
#endif


/* --- static variables --- */

/*
 * initialized in ml_window_new()
 */
static Atom  xa_utf8_string ;
static Atom  xa_compound_text ;
static Atom  xa_text ;
static Atom  xa_selection_prop ;
static Atom  xa_delete_window ;


/* --- static functions --- */

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
	ml_window_t *  win ,
	ml_char_t *  chars ,
	u_int  size ,
	int  x ,
	int  y
	)
{
	int  counter ;
	ml_font_t *  font ;
	u_char *  ch_bytes ;
	size_t  ch_size ;

	for( counter = 0 ; counter < size ; counter ++)
	{
		if( ( font = ml_char_font( &chars[counter])) == NULL)
		{
			continue ;
		}

		ch_bytes = ml_char_bytes( &chars[counter]) ;
		ch_size = ml_char_size( &chars[counter]) ;

		if( ch_size == 1)
		{
			XftDrawString8( win->xft_draw , XFT_COLOR(win,ml_char_fg_color( &chars[counter])) ,
				font->xft_font , x , y , ch_bytes , 1) ;
		}
		else
		{
		#ifndef  USE_UCS4
			XftChar16  xch ;
		#else
			XftChar32  xch ;
		#endif
			
			char  ucs4_bytes[4] ;

			if( ! ml_convert_to_xft_ucs4( ucs4_bytes , ch_bytes , ch_size ,
				ml_char_cs(&chars[counter])))
			{
				return  0 ;
			}

		#ifndef  USE_UCS4
			xch = ((ucs4_bytes[2] << 8) & 0xff00) | (ucs4_bytes[3] & 0xff) ;
			XftDrawString16( win->xft_draw , XFT_COLOR(win,ml_char_fg_color( &chars[counter])) ,
				font->xft_font , x , y , &xch , 1) ;
		#else
			xch = ((ucs4_bytes[0] << 24) & 0xff000000) |
				((ucs4_bytes[1] << 16) & 0xff0000) |
				((ucs4_bytes[2] << 8) & 0xff00) |
				(ucs4_bytes[3] & 0xff) ;
			XftDrawString32( win->xft_draw , XFT_COLOR(win,ml_char_fg_color( &chars[counter])) ,
				font->xft_font , x , y , &xch , 1) ;
		#endif
		}
	}

	return  1 ;
}

static int
xft_draw_str(
	ml_window_t *  win ,
	u_int *  updated_width ,
	ml_char_t *  chars ,
	u_int	num_of_chars ,
	int  x ,
	int  y ,
	u_int	height ,
	u_int	std_height_to_baseline
	)
{
	int  counter ;
	int  start_draw ;
	int  end_of_str ;
	u_int  height_to_baseline ;
	u_int	current_width ;
	XftChar8 *  str8 ;
	XftChar16 *  str16 ;
	XftChar32 *  str32 ;
	u_int	str_len ;
	
	u_char *  ch_bytes ;
	
	size_t  ch_size ;
	u_int  ch_width ;
	ml_font_t *  font ;
	XftColor *  fg_color ;
	XftColor *  bg_color ;
	ml_font_decor_t	 decor ;
	ml_char_t *  comb_chars ;
	u_int  comb_size ;
	int  is_reversed ;

	size_t  next_ch_size ;
	u_int  next_ch_width ;
	ml_font_t *  next_font ;
	XftColor *  next_fg_color ;
	XftColor *  next_bg_color ;
	ml_font_decor_t	 next_decor ;

#ifdef	PERF_DEBUG
	int  draw_count ;

	draw_count = 0 ;
#endif

	if( x > win->width || y + height > win->height)
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
		kik_debug_printf( KIK_DEBUG_TAG " input chars length is 0(ml_window_draw_str).\n") ;
	#endif
		
		return	1 ;
	}

	start_draw = 0 ;
	end_of_str = 0 ;

	x += win->margin ;
	y += win->margin ;

	counter = 0 ;
	
	ch_width = ml_char_width( &chars[counter]) ;
	
	current_width = x + ch_width ;

	ch_size = ml_char_size( &chars[counter]) ;
	
	if( ( font = ml_char_font( &chars[counter])) == NULL || font->xft_font == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " font is empty.\n") ;
	#endif
		
		return  0 ;
	}

	if( font != win->font)
	{
		win->font = font ;
	}

	fg_color = XFT_COLOR(win,ml_char_fg_color( &chars[counter])) ;
	bg_color = XFT_COLOR(win,ml_char_bg_color( &chars[counter])) ;

	decor = ml_char_font_decor( &chars[counter]) ;
	
	if( ( str8 = alloca( sizeof( XftChar8) * num_of_chars)) == NULL)
	{
		return	0 ;
	}

	if( ( str16 = alloca( sizeof( XftChar16) * num_of_chars)) == NULL)
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
		ch_bytes = ml_char_bytes( &chars[counter]) ;
		
		if( ch_size == 1)
		{
			str8[str_len++] = ch_bytes[0] ;
		}
		else
		{
			char  ucs4_bytes[4] ;

			if( ! ml_convert_to_xft_ucs4( ucs4_bytes , ch_bytes , ch_size ,
				ml_char_cs(&chars[counter])))
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " strange character , ignored.\n") ;
			#endif
				counter ++ ;
				
				continue ;
			}
			
		#ifndef USE_UCS4
			str16[str_len++] = ((ucs4_bytes[2] << 8) & 0xff00) | (ucs4_bytes[3] & 0xff) ;
			ch_size = 2 ;	
		#else
			str32[str_len++] = ((ucs4_bytes[0] << 24) & 0xff000000) |
				((ucs4_bytes[1] << 16) & 0xff0000) | ((ucs4_bytes[2] << 8) & 0xff00) |
				(ucs4_bytes[3] & 0xff) ;
			ch_size = 4 ;
		#endif
		}

		comb_chars = ml_get_combining_chars( &chars[counter] , &comb_size) ;

		is_reversed = ml_char_is_reversed( &chars[counter]) ;

		/*
		 * next character.
		 */
		
		counter ++ ;

		if( counter >= num_of_chars)
		{
			start_draw = 1 ;
			end_of_str = 1 ;
		}
		else
		{
			next_ch_size = ml_char_size( &chars[counter]) ;
			next_ch_width = ml_char_width( &chars[counter]) ;
			next_font = ml_char_font( &chars[counter]) ;
			next_fg_color = XFT_COLOR( win,ml_char_fg_color( &chars[counter])) ;
			next_bg_color = XFT_COLOR( win,ml_char_bg_color( &chars[counter])) ;
			next_decor = ml_char_font_decor( &chars[counter]) ;

			if( current_width + next_ch_width > win->width + win->margin)
			{
				start_draw = 1 ;
				end_of_str = 1 ;
			}
			else if( next_font != win->font
				|| fg_color != next_fg_color
				|| bg_color != next_bg_color
				|| next_decor != decor
				|| (decor & FONT_LEFTLINE)
				|| comb_chars != NULL
				|| ch_size != next_ch_size
				|| (next_font->is_proportional && ! next_font->is_var_col_width)
				|| (win->font->is_proportional && ! win->font->is_var_col_width))
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

		#ifdef	PERF_DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " %d th draw in line\n" , draw_count ++) ;
		#endif

			if( height == win->font->height)
			{
				height_to_baseline = win->font->height_to_baseline ;
			}
			else
			{
				height_to_baseline = std_height_to_baseline ;
			}
		
			/*
			 * clearing background
			 */
			if( win->wall_picture_is_set && ! is_reversed)
			{
				XClearArea( win->display , win->drawable ,
					x , y , current_width - x , height , 0) ;
			}
			else
			{
			#if  0
				/* XXX  I don't know why but bg_color is blured ... */
				XftDrawRect( win->xft_draw , bg_color ,
					x , y , current_width - x , height) ;
			#else
				XSetForeground( win->display , win->gc , bg_color->pixel) ;
				XFillRectangle( win->display , win->drawable , win->gc ,
					x  , y  , current_width - x , height) ;
				XSetForeground( win->display , win->gc , FG_COLOR_PIXEL(win)) ;
			#endif
			}
			
			/*
			 * drawing string
			 */
		#ifdef  USE_UCS4
			if( ch_size == 4)
			{
				XftDrawString32( win->xft_draw , fg_color , win->font->xft_font ,
					x , y + height_to_baseline , str32 , str_len) ;
			}
			else
		#endif
			if( ch_size == 2)
			{
				XftDrawString16( win->xft_draw , fg_color , win->font->xft_font ,
					x , y + height_to_baseline , str16 , str_len) ;
			}
			else
			{
				XftDrawString8( win->xft_draw , fg_color , win->font->xft_font ,
					x , y + height_to_baseline , str8 , str_len) ;
			}

			if( comb_chars)
			{
				if( char_combining_is_supported( ml_font_cs( win->font)))
				{
					xft_draw_combining_chars( win , comb_chars , comb_size ,
						current_width , y + height_to_baseline) ;
				}
				else
				{
					xft_draw_combining_chars( win , comb_chars , comb_size ,
						current_width - ch_width ,
						y + height_to_baseline) ;
				}
			}
			
			if( decor & FONT_UNDERLINE)
			{
				XFillRectangle( win->display , win->drawable , win->gc ,
					x , y + height - 1 , current_width - x , 1) ;
			}

			if( decor & FONT_LEFTLINE)
			{
				XFillRectangle( win->display , win->drawable , win->gc ,
					x , y , 1 , height) ;
			}

			start_draw = 0 ;
			
			x = current_width ;
			str_len = 0 ;
		}

		if( end_of_str)
		{
			break ;
		}

		current_width += next_ch_width ;
		ch_width = next_ch_width ;

		if( next_ch_size != ch_size)
		{
			ch_size = next_ch_size ;
		}
				
		if( next_font == NULL || next_font->xft_font == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " the font of char is lost.\n") ;
		#endif
		
			return  0 ;
		}
		
		if( next_font != win->font)
		{
		#ifdef	__DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " font changed...\n") ;
		#endif

			win->font = next_font ;
		}

		if( next_fg_color != fg_color)
		{
			fg_color = next_fg_color ;
		}

		if( next_bg_color != bg_color)
		{
			bg_color = next_bg_color ;
		}

		if( next_decor != decor)
		{
			decor = next_decor ;
		}
	}

	if( updated_width != NULL)
	{
		*updated_width = current_width - win->margin ;
	}

	return	1 ;
}

#endif

static void
x_draw_string(
	ml_window_t *  win ,
	ml_font_t *  font ,
	int  x ,
	int  y ,
	u_char *  str ,
	int  len
	)
{
	XDrawString( win->display , win->drawable , win->gc , x , y , str , len) ;

	if( font->is_double_drawing)
	{
		XDrawString( win->display , win->drawable , win->gc , x + 1 , y , str , len) ;
	}
}

static void
x_draw_string16(
	ml_window_t *  win ,
	ml_font_t *  font ,
	int  x ,
	int  y ,
	XChar2b *  str ,
	int  len
	)
{
	XDrawString16( win->display , win->drawable , win->gc , x , y , str , len) ;

	if( font->is_double_drawing)
	{
		XDrawString16( win->display , win->drawable , win->gc , x + 1 , y , str , len) ;
	}
}

static void
x_draw_image_string(
	ml_window_t *  win ,
	ml_font_t *  font ,
	int  x ,
	int  y ,
	u_char *  str ,
	int  len
	)
{
	XDrawImageString( win->display , win->drawable , win->gc , x , y , str , len) ;

	if( font->is_double_drawing)
	{
		XDrawString( win->display , win->drawable , win->gc , x + 1 , y , str , len) ;
	}
}

static void
x_draw_image_string16(
	ml_window_t *  win ,
	ml_font_t *  font ,
	int  x ,
	int  y ,
	XChar2b *  str ,
	int  len
	)
{
	XDrawImageString16( win->display , win->drawable , win->gc , x , y , str , len) ;

	if( font->is_double_drawing)
	{
		XDrawString16( win->display , win->drawable , win->gc , x + 1 , y , str , len) ;
	}
}

static int
draw_combining_chars(
	ml_window_t *  win ,
	ml_char_t *  chars ,
	u_int  size ,
	int  x ,
	int  y ,
	u_long  fg_color ,
	u_long  bg_color
	)
{
	int  counter ;

	u_char *  ch_bytes ;
	size_t  ch_size ;
	
	u_long  cur_fg ;
	u_long  cur_bg ;
	ml_font_t *  cur_font ;
	u_long  next_fg ;
	u_long  next_bg ;
	ml_font_t *  next_font ;

	cur_fg = fg_color ;
	cur_bg = bg_color ;
	cur_font = win->font ;
	
	for( counter = 0 ; counter < size ; counter ++)
	{
		next_fg = COLOR_PIXEL( win , ml_char_fg_color( &chars[counter])) ;
		next_bg = COLOR_PIXEL( win , ml_char_bg_color( &chars[counter])) ;
		next_font = ml_char_font( &chars[counter]) ;
		
		if( cur_fg != next_fg)
		{
			XSetForeground( win->display , win->gc , next_fg) ;
			cur_fg = next_fg ;
		}
		
		if( cur_bg != next_bg)
		{
			XSetBackground( win->display , win->gc , next_bg) ;
			cur_bg = next_bg ;
		}

		if( next_font && next_font != cur_font)
		{
			XSetFont( win->display , win->gc , next_font->xfont->fid) ;
			cur_font = next_font ;
		}

		ch_bytes = ml_char_bytes( &chars[counter]) ;
		ch_size = ml_char_size( &chars[counter]) ;
		
		if( ch_size == 2)
		{
			XChar2b  xch ;

			xch.byte1 = ch_bytes[0] ;
			xch.byte2 = ch_bytes[1] ;

			x_draw_string16( win , cur_font , x , y , &xch , 1) ;
		}
		else if( ch_size == 1)
		{
			x_draw_string( win , cur_font , x , y , ch_bytes , 1) ;
		}
	}
	
	if( cur_fg != fg_color)
	{
		XSetForeground( win->display , win->gc , fg_color) ;
	}

	if( cur_bg != bg_color)
	{
		XSetBackground( win->display , win->gc , bg_color) ;
	}

	if( cur_font != win->font)
	{
		XSetFont( win->display , win->gc , win->font->xfont->fid) ;
	}

	return  1 ;
}

static int
draw_str(
	ml_window_t *  win ,
	u_int *  updated_width ,
	ml_char_t *  chars ,
	u_int	num_of_chars ,
	int  x ,
	int  y ,
	u_int	height ,
	u_int	std_height_to_baseline
	)
{
	int  counter ;
	int  start_draw ;
	int  end_of_str ;
	u_int  height_to_baseline ;
	u_char *  str ;
	u_int	current_width ;
	XChar2b *  str2b ;
	u_int	str_len ;
	int  is_mb ;
	ml_char_t *  comb_chars ;
	u_int  comb_size ;

	u_char *  ch_bytes ;
	
	size_t  ch_size ;
	u_int  ch_width ;
	ml_font_t *  font ;
	u_long  fg_color ;
	u_long  bg_color ;
	ml_font_decor_t	 decor ;
	int  is_reversed ;

	size_t  next_ch_size ;
	u_int  next_ch_width ;
	ml_font_t *  next_font ;
	u_long  next_fg_color ;
	u_long  next_bg_color ;
	ml_font_decor_t  next_decor ;
	
#ifdef	PERF_DEBUG
	int  draw_count ;

	draw_count = 0 ;
#endif

	if( x > win->width || y + height > win->height)
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
		kik_debug_printf( KIK_DEBUG_TAG " input chars length is 0(ml_window_draw_str).\n") ;
	#endif
		
		return	1 ;
	}

	counter = 0 ;
	
	if( ( font = ml_char_font( &chars[counter])) == NULL || font->xfont == NULL)
	{
	#ifdef  ANTI_ALIAS
		if( font->xft_font)
		{
			return  xft_draw_str( win , updated_width , chars , num_of_chars ,
					x , y , height , std_height_to_baseline) ;
		}
	#endif

	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " the font of char is lost.\n") ;
	#endif
	
		return  0 ;
	}

	if( font != win->font)
	{
		XSetFont( win->display , win->gc , font->xfont->fid) ;
		win->font = font ;
	}

	start_draw = 0 ;
	end_of_str = 0 ;

	x += win->margin ;
	y += win->margin ;

	ch_size = ml_char_size( &chars[counter]) ;
	
	ch_width = ml_char_width( &chars[counter]) ;
	
	current_width = x + ch_width ;

	fg_color = COLOR_PIXEL(win,ml_char_fg_color( &chars[counter])) ;
	if( fg_color != FG_COLOR_PIXEL(win))
	{
		XSetForeground( win->display , win->gc , fg_color) ;
	}

	bg_color = COLOR_PIXEL(win,ml_char_bg_color( &chars[counter])) ;
	if( bg_color != BG_COLOR_PIXEL(win))
	{
		XSetBackground( win->display , win->gc , bg_color) ;
	}

	decor = ml_char_font_decor( &chars[counter]) ;
	
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
		ch_bytes = ml_char_bytes( &chars[counter]) ;
		
		if( ch_size == 1)
		{
			is_mb = 0 ;
			str[str_len] = ch_bytes[0] ;
			str_len ++ ;
		}
		else if( ch_size == 2)
		{
			is_mb = 1 ;
			str2b[str_len].byte1 = ch_bytes[0] ;
			str2b[str_len].byte2 = ch_bytes[1] ;
			str_len ++ ;
		}
		else if( ch_size == 4)
		{
			/* is UCS4 */
			
			is_mb = 1 ;
			str2b[str_len].byte1 = ch_bytes[2] ;
			str2b[str_len].byte2 = ch_bytes[3] ;
			str_len ++ ;
		}
		else
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " strange character , ignored.\n") ;
		#endif
		
			counter ++ ;
			
			continue ;
		}

		comb_chars = ml_get_combining_chars( &chars[counter] , &comb_size) ;

		is_reversed = ml_char_is_reversed( &chars[counter]) ;

		/*
		 * next character.
		 */
		
		counter ++ ;

		if( counter >= num_of_chars)
		{
			start_draw = 1 ;
			end_of_str = 1 ;
		}
		else
		{
			next_ch_size = ml_char_size( &chars[counter]) ;
			next_ch_width = ml_char_width( &chars[counter]) ;
			next_font = ml_char_font( &chars[counter]) ;
			next_fg_color = COLOR_PIXEL( win,ml_char_fg_color( &chars[counter])) ;
			next_bg_color = COLOR_PIXEL( win,ml_char_bg_color( &chars[counter])) ;
			next_decor = ml_char_font_decor( &chars[counter]) ;

			if( current_width + next_ch_width > win->width + win->margin)
			{
				start_draw = 1 ;
				end_of_str = 1 ;
			}
			else if( next_font != win->font
				|| fg_color != next_fg_color
				|| bg_color != next_bg_color
				|| next_decor != decor
				|| (decor & FONT_LEFTLINE)
				|| comb_chars != NULL
				|| (is_mb == 1 && next_ch_size == 1)
				|| (is_mb == 0 && next_ch_size > 1)
				|| (next_font->is_proportional && ! next_font->is_var_col_width)
				|| (win->font->is_proportional && ! win->font->is_var_col_width))
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

		#ifdef	PERF_DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " %d th draw in line\n" , draw_count ++) ;
		#endif

			if( win->font->height == height)
			{
				height_to_baseline = win->font->height_to_baseline ;
			}
			else
			{
				height_to_baseline = std_height_to_baseline ;
			}

			if( ( win->wall_picture_is_set && ! is_reversed) ||
				(win->font->is_proportional && ! win->font->is_var_col_width))
			{
				if( win->wall_picture_is_set)
				{
					XClearArea( win->display , win->drawable ,
						x , y , current_width - x , height , 0) ;
				}
				else
				{
				#ifdef  __DEBUG
					kik_debug_printf( KIK_DEBUG_TAG "prop font is used.\n") ;
				#endif
				
					XSetForeground( win->display , win->gc , bg_color) ;
					XFillRectangle( win->display , win->drawable , win->gc ,
						x , y , current_width - x , height) ;
					XSetForeground( win->display , win->gc , fg_color) ;
				}
				
				if( is_mb)
				{
					x_draw_string16( win , win->font , x , y + height_to_baseline ,
						str2b , str_len) ;
				}
				else
				{
					x_draw_string( win , win->font , x , y + height_to_baseline ,
						str , str_len) ;
				}
			}
			else
			{
				if( is_mb)
				{
					x_draw_image_string16( win , win->font ,
						x , y + height_to_baseline , str2b , str_len) ;
				}
				else
				{
					x_draw_image_string( win , win->font , x , y + height_to_baseline ,
						str , str_len) ;
				}
			}
			
			if( comb_chars)
			{
				if( char_combining_is_supported( ml_font_cs( win->font)))
				{
					draw_combining_chars( win , comb_chars , comb_size ,
						current_width , y + height_to_baseline ,
						fg_color , bg_color) ;
				}
				else
				{
					draw_combining_chars( win , comb_chars , comb_size ,
						current_width - ch_width ,
						y + height_to_baseline , fg_color , bg_color) ;
				}
			}

			if( decor & FONT_UNDERLINE)
			{
				XFillRectangle( win->display , win->drawable , win->gc ,
					x , y + height - 1 , current_width - x , 1) ;
			}

			if( decor & FONT_LEFTLINE)
			{
				XFillRectangle( win->display , win->drawable , win->gc ,
					x , y , 1 , height) ;
			}

			start_draw = 0 ;

			x = current_width ;
			str_len = 0 ;
		}

		if( end_of_str)
		{
			break ;
		}

		current_width += next_ch_width ;
		ch_width = next_ch_width ;

		if( next_ch_size != ch_size)
		{
			ch_size = next_ch_size ;
		}
				
		if( next_font == NULL || next_font->xfont == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " the font of char is lost.\n") ;
		#endif
		
			return  0 ;
		}
		
		if( next_font != win->font)
		{
		#ifdef	__DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " font changed...\n") ;
		#endif

			XSetFont( win->display , win->gc , next_font->xfont->fid) ;
			win->font = next_font ;
		}
		
		if( next_fg_color != fg_color)
		{
			XSetForeground( win->display , win->gc , next_fg_color) ;
			
			fg_color = next_fg_color ;
		}

		if( next_bg_color != bg_color)
		{
			XSetBackground( win->display , win->gc , next_bg_color) ;
			
			bg_color = next_bg_color ;
		}

		if( next_decor != decor)
		{
			decor = next_decor ;
		}
	}

	if( fg_color != FG_COLOR_PIXEL(win))
	{
		XSetForeground( win->display , win->gc , FG_COLOR_PIXEL(win)) ;
	}

	if( bg_color != BG_COLOR_PIXEL(win))
	{
		XSetBackground( win->display , win->gc , BG_COLOR_PIXEL(win)) ;
	}

	if( updated_width != NULL)
	{
		*updated_width = current_width - win->margin ;
	}

	return	1 ;
}

static int
update_fg_color(
	ml_window_t *  win
	)
{
	XSetForeground( win->display , win->gc , FG_COLOR_PIXEL(win)) ;

	return  1 ;
}

static int
update_bg_color(
	ml_window_t *  win
	)
{
	XSetBackground( win->display , win->gc , BG_COLOR_PIXEL(win)) ;

	if( ! win->is_transparent && ! win->wall_picture_is_set)
	{
		XSetWindowBackground( win->display , win->my_window , BG_COLOR_PIXEL(win)) ;
	}

	return  1 ;
}

static int
update_pic_transparent(
	ml_window_t *  win
	)
{
	ml_picture_t  pic ;

	if( ml_picture_init( &pic , win , win->pic_mod) &&
		ml_picture_load_background( &pic))
	{
		/*
		 * !! Notice !!
		 * this must be done before ml_window_set_wall_picture() because
		 * ml_window_set_wall_picture() doesn't do anything if is_transparent
		 * flag is on.
		 */
		win->is_transparent = 0 ;

		ml_window_set_wall_picture( win , pic.pixmap) ;

		win->is_transparent = 1 ;
	}

	ml_picture_final( &pic) ;
	
	return  1 ;
}

static void
notify_focus_in_to_children(
	ml_window_t *  win
	)
{
	int  counter ;

	if( win->window_focused)
	{
		(*win->window_focused)( win) ;
	}

	ml_xic_set_focus( win) ;

	for( counter = 0 ; counter < win->num_of_children ; counter ++)
	{
		notify_focus_in_to_children( win->children[counter]) ;
	}
}

static void
notify_focus_out_to_children(
	ml_window_t *  win
	)
{
	int  counter ;

	if( win->window_unfocused)
	{
		(*win->window_unfocused)( win) ;
	}
	
	ml_xic_unset_focus( win) ;

	for( counter = 0 ; counter < win->num_of_children ; counter ++)
	{
		notify_focus_out_to_children( win->children[counter]) ;
	}
}

static void
notify_configure_to_children(
	ml_window_t *  win
	)
{
	int  counter ;

	if( win->is_transparent)
	{
		if( win->pic_mod)
		{
			update_pic_transparent( win) ;
		}
		else
		{
			ml_window_clear_all( win) ;
			(*win->window_exposed)( win , 0 , 0 , win->width , win->height) ;
		}
	}

	for( counter = 0 ; counter < win->num_of_children ; counter ++)
	{
		notify_configure_to_children( win->children[counter]) ;
	}
}

static void
notify_reparent_to_children(
	ml_window_t *  win
	)
{
	int  counter ;

	if( win->is_transparent)
	{
		ml_window_set_transparent( win , win->pic_mod) ;
	}

	for( counter = 0 ; counter < win->num_of_children ; counter ++)
	{
		notify_reparent_to_children( win->children[counter]) ;
	}
}

static int
is_descendant_window(
	ml_window_t *  win ,
	Window  window
	)
{
	int  counter ;
	
	if( win->my_window == window)
	{
		return  1 ;
	}

	for( counter = 0 ; counter < win->num_of_children ; counter ++)
	{
		if( is_descendant_window( win->children[counter] , counter))
		{
			return  1 ;
		}
	}

	return  0 ;
}

static int
is_in_the_same_window_family(
	ml_window_t *  win ,
	Window   window
	)
{
	return  is_descendant_window( ml_get_root_window( win) , window) ;
}


/* --- global functions --- */

int
ml_window_init(
	ml_window_t *  win ,
	ml_color_table_t  color_table ,
	u_int  width ,
	u_int  height ,
	u_int  min_width ,
	u_int  min_height ,
	u_int  width_inc ,
	u_int  height_inc ,
	u_int  margin
	)
{
	/*
	 * these values are set later
	 */
	 
	win->display = NULL ;
	win->screen = 0 ;
	
	win->parent_window = 0 ;
	win->my_window = 0 ;
	
	win->drawable = 0 ;
	win->pixmap = 0 ;
	win->gc = 0 ;
#ifdef  ANTI_ALIAS
	win->xft_draw = NULL ;
#endif

	win->parent = NULL ;
	win->children = NULL ;
	win->num_of_children = 0 ;

	win->use_pixmap = 0 ;

	win->pic_mod = NULL ;
	
	win->wall_picture_is_set = 0 ;
	win->is_transparent = 0 ;
	
	win->cursor_shape = 0 ;

	win->event_mask = ExposureMask | VisibilityChangeMask | FocusChangeMask ;

	/* if visibility is partially obsucured , scrollable will be 0. */
	win->is_scrollable = 1 ;

	win->font = NULL ;

	win->x = 0 ;
	win->y = 0 ;	
	win->width = width ;
	win->height = height ;
	win->min_width = min_width ,
	win->min_height = min_height ;
	win->width_inc = width_inc ;
	win->height_inc = height_inc ;
	win->margin = margin ;

	win->color_table = color_table ;
	win->orig_fg_xcolor = NULL ;
	win->orig_bg_xcolor = NULL ;

	win->xic = NULL ;
	win->xim = NULL ;
	win->xim_listener = NULL ;

	win->prev_clicked_time = 0 ;
	win->prev_clicked_button = -1 ;
	win->click_num = 0 ;
	win->button_is_pressing = 0 ;

	win->window_realized = NULL ;
	win->window_finalized = NULL ;
	win->window_exposed = NULL ;
	win->window_focused = NULL ;
	win->window_unfocused = NULL ;
	win->key_pressed = NULL ;
	win->button_motion = NULL ;
	win->button_released = NULL ;
	win->button_pressed = NULL ;
	win->button_press_continued = NULL ;
	win->window_resized = NULL ;
	win->child_window_resized = NULL ;
	win->selection_cleared = NULL ;
	win->xct_selection_requested = NULL ;
	win->utf8_selection_requested = NULL ;
	win->xct_selection_notified = NULL ;
	win->utf8_selection_notified = NULL ;
	win->xct_selection_request_failed = NULL ;
	win->utf8_selection_request_failed = NULL ;
	win->window_deleted = NULL ;
		
	return	1 ;
}

int
ml_window_final(
	ml_window_t *  win
	)
{
	int  counter ;

#ifdef  __DEBUG
	kik_debug_printf( "deleting child windows ==>\n") ;
	ml_window_dump_children( win) ;
#endif
	
	for( counter = 0 ; counter < win->num_of_children ; counter ++)
	{
		ml_window_final( win->children[counter]) ;
	}

	if( win->children)
	{
		free( win->children) ;
	}
	
	if( win->orig_fg_xcolor)
	{
		ml_unload_xcolor( win->display , win->screen , win->color_table[MLC_FG_COLOR]) ;
		free( win->color_table[MLC_FG_COLOR]) ;
	}

	if( win->orig_bg_xcolor)
	{
		ml_unload_xcolor( win->display , win->screen , win->color_table[MLC_BG_COLOR]) ;
		free( win->color_table[MLC_BG_COLOR]) ;
	}
	
	free( win->color_table) ;

	ml_window_manager_clear_selection( ml_get_root_window( win)->win_man , win) ;
	XFreeGC( win->display , win->gc) ;
	XDestroyWindow( win->display , win->my_window) ;

	ml_xic_deactivate( win) ;
	
	if( win->window_finalized)
	{
		(*win->window_finalized)( win) ;
	}

	return  1 ;
}

int
ml_window_init_atom(
	Display *  display
	)
{
	xa_compound_text = XInternAtom( display , "COMPOUND_TEXT" , False) ;
	xa_text = XInternAtom( display , "TEXT" , False) ;
	xa_utf8_string = XInternAtom( display , "UTF8_STRING" , False) ;
	xa_selection_prop = XInternAtom( display , "MLTERM_SELECTION" , False) ;
	xa_delete_window = XInternAtom( display , "WM_DELETE_WINDOW" , False) ;

	return  1 ;
}

int
ml_window_init_event_mask(
	ml_window_t *  win ,
	long  event_mask
	)
{
	if( win->my_window)
	{
		/* this can be used before ml_window_show() */
		
		return  0 ;
	}

#if  0
	if( event_mask & ButtonMotionMask)
	{
		event_mask &= ~ButtonMotionMask ;
		event_mask |= ( Button1MotionMask | Button2MotionMask |
			Button3MotionMask | Button4MotionMask | Button5MotionMask) ;
	}
#endif

	win->event_mask |= event_mask ;

	return  1 ;
}

int
ml_window_add_event_mask(
	ml_window_t *  win ,
	long  event_mask
	)
{
#if  0
	if( event_mask & ButtonMotionMask)
	{
		event_mask &= ~ButtonMotionMask ;
		event_mask |= ( Button1MotionMask | Button2MotionMask |
			Button3MotionMask | Button4MotionMask | Button5MotionMask) ;
	}
#endif

	win->event_mask |= event_mask ;

	XSelectInput( win->display , win->my_window , win->event_mask) ;

	return  1 ;
}

int
ml_window_remove_event_mask(
	ml_window_t *  win ,
	long  event_mask
	)
{
#if  0
	if( event_mask & ButtonMotionMask)
	{
		event_mask &= ~ButtonMotionMask ;
		event_mask |= ( Button1MotionMask | Button2MotionMask |
			Button3MotionMask | Button4MotionMask | Button5MotionMask) ;
	}
#endif

	win->event_mask &= ~event_mask ;

	XSelectInput( win->display , win->my_window , win->event_mask) ;

	return  1 ;
}

int
ml_window_set_wall_picture(
	ml_window_t *  win ,
	Pixmap  pic
	)
{
	if( win->drawable != win->my_window)
	{
		/*
		 * wall picture can not be used in double buffering mode.
		 */
		
		return  0 ;
	}

	if( win->is_transparent)
	{
		/*
		 * unset transparent before setting wall picture !
		 */
		 
		return  0 ;
	}

	if( win->event_mask & VisibilityChangeMask)
	{
		/* rejecting VisibilityNotify event. is_scrollable is always false */
		win->event_mask &= ~VisibilityChangeMask ;
		XSelectInput( win->display , win->my_window , win->event_mask) ;
		win->is_scrollable = 0 ;
	}

	XSetWindowBackgroundPixmap( win->display , win->my_window , pic) ;
	
	win->wall_picture_is_set = 1 ;

	if( win->window_exposed)
	{
		ml_window_clear_all( win) ;
		(*win->window_exposed)( win , 0 , 0 , win->width , win->height) ;
	}

	return  1 ;
}

int
ml_window_unset_wall_picture(
	ml_window_t *  win
	)
{
	if( win->drawable != win->my_window)
	{
		/*
		 * wall picture can not be used in double buffering mode.
		 */
		 
		return  0 ;
	}

	if( ! win->wall_picture_is_set)
	{
		/* already unset */
		
		return  1 ;
	}

	if( win->is_transparent)
	{
		/*
		 * transparent background is not a wall picture :)
		 * this case is regarded as not using a wall picture.
		 */
	
		return  1 ;
	}

	if( !( win->event_mask & VisibilityChangeMask))
	{
		/* accepting VisibilityNotify event. is_scrollable is changed dynamically. */
		win->event_mask |= VisibilityChangeMask ;
		XSelectInput( win->display , win->my_window , win->event_mask) ;

		/* setting 0 in case the current status is VisibilityPartiallyObscured. */
		win->is_scrollable = 0 ;
	}
	
	XSetWindowBackgroundPixmap( win->display , win->my_window , None) ;
	XSetWindowBackground( win->display , win->my_window , BG_COLOR_PIXEL(win)) ;
	
	win->wall_picture_is_set = 0 ;

	if( win->window_exposed)
	{
		ml_window_clear_all( win) ;
		(*win->window_exposed)( win , 0 , 0 , win->width , win->height) ;
	}

	return  1 ;
}

int
ml_window_set_transparent(
	ml_window_t *  win ,
	ml_picture_modifier_t *  pic_mod
	)
{
	Window  parent ;
	Window  root ;
	Window *  list ;
	u_int  n ;
	int  counter ;

	win->pic_mod = pic_mod ;
	
	if( win->my_window == None)
	{
		/*
		 * If Window is not still created , actual drawing is delayed and
		 * ReparentNotify event will do transparent processing automatically after
		 * ml_window_show().
		 */
		 
		win->is_transparent = 1 ;

		goto  end ;
	}

	if( win->pic_mod)
	{
		update_pic_transparent( win) ;
	}
	else
	{
		/*
		 * !! Notice !!
		 * this must be done before ml_window_set_wall_picture() because
		 * ml_window_set_wall_picture() doesn't do anything if is_transparent
		 * flag is on.
		 */
		win->is_transparent = 0 ;

		if( ! ml_window_set_wall_picture( win , ParentRelative))
		{
			return  0 ;
		}
	
		win->is_transparent = 1 ;

		XSetWindowBackgroundPixmap( win->display , win->my_window , ParentRelative) ;

		parent = win->my_window ;

		while( 1)
		{
			XQueryTree( win->display , parent , &root , &parent , &list , &n) ;
			XFree(list) ;

			if( parent == DefaultRootWindow(win->display))
			{
				break ;
			}

			XSetWindowBackgroundPixmap( win->display , parent , ParentRelative) ;
		}

		if( win->window_exposed)
		{
			ml_window_clear_all( win) ;
			(*win->window_exposed)( win , 0 , 0 , win->width , win->height) ;
		}
	}

end:
	for( counter = 0 ; counter < win->num_of_children ; counter ++)
	{
		ml_window_set_transparent( win->children[counter] , pic_mod) ;
	}
	
	return  1 ;
}

int
ml_window_unset_transparent(
	ml_window_t *  win
	)
{
	int  counter ;
#if  0
	Window  parent ;
	Window  root ;
	Window *  list ;
	int  n ;
#endif
	
	if( ! win->is_transparent)
	{
		/* already unset */

		return  1 ;
	}

	/*
	 * !! Notice !!
	 * this must be done before ml_window_unset_wall_picture() because
	 * ml_window_unset_wall_picture() doesn't do anything if is_transparent
	 * flag is on.
	 */
	win->is_transparent = 0 ;
	win->pic_mod = NULL ;

	if( ! ml_window_unset_wall_picture( win))
	{
		return  0 ;
	}

#if  0
	/*
	 * XXX
	 * is this necessary ?
	 */
	parent = win->my_window ;

	for( counter = 0 ; counter < 2 ; counter ++)
	{
		XQueryTree( win->display , parent , &root , &parent , &list , &n) ;
		XFree(list) ;
		if( parent == DefaultRootWindow(win->display))
		{
			break ;
		}

		XSetWindowBackgroundPixmap( win->display , parent , None) ;
	}
#endif
	
	if( win->window_exposed)
	{
		ml_window_clear_all( win) ;
		(*win->window_exposed)( win , 0 , 0 , win->width , win->height) ;
	}
	
	for( counter = 0 ; counter < win->num_of_children ; counter ++)
	{
		ml_window_unset_transparent( win->children[counter]) ;
	}
	
	return  1 ;
}

int
ml_window_use_pixmap(
	ml_window_t *  win
	)
{
	win->use_pixmap = 1 ;
	
	win->event_mask &= ~VisibilityChangeMask ;

	/* always true */
	win->is_scrollable = 1 ;

	return  1 ;
}

int
ml_window_set_cursor(
	ml_window_t *  win ,
	u_int  cursor_shape
	)
{
	win->cursor_shape = cursor_shape ;

	return  1 ;
}

int
ml_window_set_fg_color(
	ml_window_t *  win ,
	ml_color_t  fg_color
	)
{
	if( fg_color < 0 || MAX_ACTUAL_COLORS <= fg_color)
	{
		/* is not valid color */
		
		return  0 ;
	}
	
	win->color_table[MLC_FG_COLOR] = win->color_table[fg_color] ;

	if( win->drawable)
	{
		update_fg_color( win) ;

		if( win->window_exposed)
		{
			ml_window_clear_all( win) ;
			(*win->window_exposed)( win , 0 , 0 , win->width , win->height) ;
		}
	}

	return  1 ;
}

int
ml_window_set_bg_color(
	ml_window_t *  win ,
	ml_color_t  bg_color
	)
{
	if( bg_color < 0 ||  MAX_ACTUAL_COLORS <= bg_color)
	{
		/* is not valid color */
		
		return  0 ;
	}
	
	win->color_table[MLC_BG_COLOR] = win->color_table[bg_color] ;

	if( win->drawable)
	{
		update_bg_color( win) ;
		
		if( win->window_exposed)
		{
			ml_window_clear_all( win) ;
			(*win->window_exposed)( win , 0 , 0 , win->width , win->height) ;
		}
	}

	return  1 ;
}

int
ml_window_get_fg_color(
	ml_window_t *  win
	)
{
	ml_color_t  color ;

	for( color = 0 ; color < MAX_ACTUAL_COLORS ; color ++)
	{
		if( win->color_table[MLC_FG_COLOR] == win->color_table[color])
		{
			return  color ;
		}
	}

	return  MLC_UNKNOWN_COLOR ;
}

int
ml_window_get_bg_color(
	ml_window_t *  win
	)
{
	ml_color_t  color ;

	for( color = 0 ; color < MAX_ACTUAL_COLORS ; color ++)
	{
		if( win->color_table[MLC_BG_COLOR] == win->color_table[color])
		{
			return  color ;
		}
	}

	return  MLC_UNKNOWN_COLOR ;
}

int
ml_window_fade(
	ml_window_t *  win ,
	u_int8_t  fade_ratio
	)
{
	x_color_t *  xcolor ;
	u_short  red ;
	u_short  green ;
	u_short  blue ;

	if( fade_ratio >= 100 || win->orig_fg_xcolor || win->orig_bg_xcolor || win->wall_picture_is_set)
	{
		return  1 ;
	}

	/*
	 * fading fg color
	 */
	 
	ml_get_xcolor_rgb( &red , &green , &blue , win->color_table[MLC_FG_COLOR]) ;

	red = (red * fade_ratio) / 100 ;
	green = (green * fade_ratio) / 100 ;
	blue = (blue * fade_ratio) / 100 ;

	if( ( xcolor = malloc( sizeof( x_color_t))) == NULL)
	{
		return  0 ;
	}

	ml_load_rgb_xcolor( win->display , win->screen , xcolor , red , green , blue) ;

	win->orig_fg_xcolor = win->color_table[MLC_FG_COLOR] ;
	win->color_table[MLC_FG_COLOR] = xcolor ;

	update_fg_color( win) ;


	/*
	 * fading bg color
	 */
	 
	ml_get_xcolor_rgb( &red , &green , &blue , win->color_table[MLC_BG_COLOR]) ;

	red = (red * fade_ratio) / 100 ;
	green = (green * fade_ratio) / 100 ;
	blue = (blue * fade_ratio) / 100 ;

	if( ( xcolor = malloc( sizeof( x_color_t))) == NULL)
	{
		return  0 ;
	}

	ml_load_rgb_xcolor( win->display , win->screen , xcolor , red , green , blue) ;

	win->orig_bg_xcolor = win->color_table[MLC_BG_COLOR] ;
	win->color_table[MLC_BG_COLOR] = xcolor ;

	update_bg_color( win) ;

	
	if( win->window_exposed)
	{
		ml_window_clear_all( win) ;
		(*win->window_exposed)( win , 0 , 0 , win->width , win->height) ;
	}

	return  1 ;
}

int
ml_window_unfade(
	ml_window_t *  win
	)
{
	if( win->orig_fg_xcolor == NULL && win->orig_bg_xcolor == NULL)
	{
		return  1 ;
	}
	
	if( win->orig_fg_xcolor)
	{
		ml_unload_xcolor( win->display , win->screen , win->color_table[MLC_FG_COLOR]) ;
		free( win->color_table[MLC_FG_COLOR]) ;
		
		win->color_table[MLC_FG_COLOR] = win->orig_fg_xcolor ;
		win->orig_fg_xcolor = NULL ;
		
		update_fg_color( win) ;
	}

	if( win->orig_bg_xcolor)
	{
		ml_unload_xcolor( win->display , win->screen , win->color_table[MLC_BG_COLOR]) ;
		free( win->color_table[MLC_BG_COLOR]) ;
		
		win->color_table[MLC_BG_COLOR] = win->orig_bg_xcolor ;
		win->orig_bg_xcolor = NULL ;

		update_bg_color( win) ;
	}
	
	if( win->window_exposed)
	{
		ml_window_clear_all( win) ;
		(*win->window_exposed)( win , 0 , 0 , win->width , win->height) ;
	}

	return  1 ;
}

int
ml_window_add_child(
	ml_window_t *  win ,
	ml_window_t *  child ,
	int  x ,
	int  y
	)
{
	void *  p ;
	
	if( ( p = realloc( win->children , sizeof( *win->children) * (win->num_of_children + 1)))
		== NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " realloc failed.\n") ;
	#endif
	
		return  0 ;
	}

	win->children = p ;

	/*
	 * on the contrary , the root window does not have the ref of parent , but that of win_man.
	 */
	child->win_man = NULL ;
	child->parent = win ;
	child->x = x ;
	child->y = y ;
	
	win->children[ win->num_of_children ++] = child ;

	return  1 ;
}

ml_window_t *
ml_get_root_window(
	ml_window_t *  win
	)
{
	while( win->parent != NULL)
	{
		win = win->parent ;
	}

	return  win ;
}

int
ml_window_show(
	ml_window_t *  win ,
	int  hint
	)
{
	XGCValues  gc_value ;
	int  counter ;

	if( win->my_window)
	{
		/* already shown */
		
		return  0 ;
	}

	if( win->color_table[MLC_FG_COLOR] == NULL ||
		win->color_table[MLC_BG_COLOR] == NULL)
	{
		kik_msg_printf( "fg color / bg_color is not set! window is not shown.\n") ;

		return  0 ;
	}
	
	if( win->parent)
	{
		win->display = win->parent->display ;
		win->screen = win->parent->screen ;
		win->parent_window = win->parent->my_window ;
	}

	if( hint & XNegative)
	{
		win->x += (DisplayWidth( win->display , win->screen) - ACTUAL_WIDTH(win)) ;
	}

	if( hint & YNegative)
	{
		win->y += (DisplayHeight( win->display , win->screen) - ACTUAL_HEIGHT(win)) ;
	}

	win->my_window = XCreateSimpleWindow(
		win->display , win->parent_window ,
		win->x , win->y , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
		0 , BlackPixel(win->display , win->screen) , BG_COLOR_PIXEL(win)) ;

	if( win->use_pixmap)
	{
		win->drawable = win->pixmap = XCreatePixmap( win->display , win->parent_window ,
				ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
				DefaultDepth( win->display , win->screen)) ;
	}
	else
	{
		win->drawable = win->my_window ;
		win->pixmap = 0 ;
	}

	if( win->cursor_shape)
	{
		XDefineCursor( win->display , win->my_window ,
			XCreateFontCursor( win->display , win->cursor_shape)) ;
	}

#ifdef  ANTI_ALIAS
	win->xft_draw = XftDrawCreate( win->display , win->drawable ,
					DefaultVisual( win->display , win->screen) ,
					DefaultColormap( win->display , win->screen)) ;
#endif

	/*
	 * graphic context.
	 */

	gc_value.foreground = FG_COLOR_PIXEL(win) ;
	gc_value.background = BG_COLOR_PIXEL(win) ;
	gc_value.graphics_exposures = 0 ;

	win->gc = XCreateGC( win->display , win->my_window ,
		GCForeground | GCBackground | GCGraphicsExposures , &gc_value) ;

	if( win->parent == NULL)
	{
		XSizeHints  size_hints ;
		XWMHints  wm_hints ;
		int  argc = 1 ;
		char *  argv[] = { "mlterm" , NULL , } ;
		
		win->event_mask |= StructureNotifyMask ;

		/*
		 * XXX
		 * x/y/width/height are obsoleted. (see XSizeHints(3))
		 */
		size_hints.x = win->x ;
		size_hints.y = win->y ;
		size_hints.width = ACTUAL_WIDTH(win) ;
		size_hints.height = ACTUAL_HEIGHT(win) ;
		
		size_hints.base_width = 2 * win->margin ;
		size_hints.base_height = 2 * win->margin ;
		size_hints.width_inc = win->width_inc ;
		size_hints.height_inc = win->height_inc ;
		size_hints.min_width = win->min_width + 2 * win->margin ;
		size_hints.min_height = win->min_height + 2 * win->margin ;

		if( hint & XNegative)
		{
			if( hint & YNegative)
			{
				size_hints.win_gravity = SouthEastGravity ;
			}
			else
			{
				size_hints.win_gravity = NorthEastGravity ;
			}
		}
		else
		{
			if( hint & YNegative)
			{
				size_hints.win_gravity = SouthWestGravity ;
			}
			else
			{
				size_hints.win_gravity = NorthWestGravity ;
			}
		}

		size_hints.flags = PSize | PMinSize | PResizeInc | PBaseSize | PWinGravity ;
		
		if( hint & (XValue | YValue))
		{
			size_hints.flags |= PPosition ;
		}

		wm_hints.window_group = win->my_window ;
		wm_hints.initial_state = NormalState ;	/* or IconicState */
		wm_hints.input = True ;			/* wants FocusIn/FocusOut */
		wm_hints.flags = WindowGroupHint | StateHint | InputHint ;
		
		/* notify to window manager */
		XmbSetWMProperties( win->display , win->my_window , "mlterm" , "mlterm" ,
			argv , argc , &size_hints , &wm_hints , NULL) ;
			
		XSetWMProtocols( win->display , win->my_window , &xa_delete_window , 1) ;
	}

	XSelectInput( win->display , win->my_window , win->event_mask) ;

#if  0	
	ml_window_clear_all( win) ;
#endif
	
	if( win->window_realized)
	{
		(*win->window_realized)( win) ;
	}

	/*
	 * showing child windows.
	 */

	for( counter = 0 ; counter < win->num_of_children ; counter ++)
	{
		ml_window_show( win->children[counter] , 0) ;
	}

	XMapWindow( win->display , win->my_window) ;

	return  1 ;
}

#if  0
int
ml_window_map(
	ml_window_t *  win
	)
{
	XMapWindow( win->display , win->my_window) ;

	return  1 ;
}

int
ml_window_unmap(
	ml_window_t *  win
	)
{
	XUnmapSubwindows( win->display , win->my_window) ;

	return  1 ;
}
#endif

int
ml_window_reset_font(
	ml_window_t *  win
	)
{
	win->font = NULL ;

	return  1 ;
}

int
ml_window_resize(
	ml_window_t *  win ,
	u_int  width ,		/* excluding margin */
	u_int  height ,		/* excluding margin */
	ml_event_dispatch_t  flag	/* NOTIFY_TO_PARENT , NOTIFY_TO_MYSELF */
	)
{
	if( win->width == width && win->height == height)
	{
		return  0 ;
	}
	
	win->width = width ;
	win->height = height ;
	
	if( win->pixmap)
	{
		XFreePixmap( win->display , win->pixmap) ;
		win->pixmap = XCreatePixmap( win->display ,
			win->parent_window , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
			DefaultDepth( win->display , win->screen)) ;
		win->drawable = win->pixmap ;

	#ifdef  ANTI_ALIAS
		XftDrawChange( win->xft_draw , win->drawable) ;
	#endif
	}

	if( (flag & NOTIFY_TO_PARENT) && win->parent && win->parent->child_window_resized)
	{
		(*win->parent->child_window_resized)( win->parent , win) ;
	}
	
	XResizeWindow( win->display , win->my_window , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win)) ;
	
	if( (flag & NOTIFY_TO_MYSELF) && win->window_resized)
	{
		(*win->window_resized)( win) ;
	}

	return  1 ;
}

/*
 * !! Notice !!
 * This function is not recommended.
 * Use ml_window_resize if at all possible.
 */
int
ml_window_resize_with_margin(
	ml_window_t *  win ,
	u_int  width ,
	u_int  height ,
	ml_event_dispatch_t  flag	/* NOTIFY_TO_PARENT , NOTIFY_TO_MYSELF */
	)
{
	return  ml_window_resize( win , width - win->margin * 2 , height - win->margin * 2 , flag) ;
}

int
ml_window_set_normal_hints(
	ml_window_t *  win ,
	u_int  width_inc ,
	u_int  height_inc ,
	u_int  min_width ,
	u_int  min_height
	)
{
	XSizeHints  size_hints ;

	win = ml_get_root_window(win) ;

	/*
	 * these hints must be set at the same time !
	 */	
	size_hints.min_width = min_width + 2 * win->margin ;
	size_hints.min_height = min_height + 2 * win->margin ;
	size_hints.width_inc = width_inc ;
	size_hints.height_inc = height_inc ;
	size_hints.base_width = 2 * win->margin ;
	size_hints.base_height = 2 * win->margin ;
	size_hints.flags = PMinSize | PResizeInc | PBaseSize ;

	win->min_width = min_width ;
	win->min_height = min_height  ;
	win->width_inc = width_inc ;
	win->height_inc = height_inc ;

	XSetWMNormalHints( win->display , win->my_window , &size_hints) ;
	
	return  1 ;
}

int
ml_window_move(
	ml_window_t *  win ,
	int  x ,
	int  y
	)
{
	XMoveWindow( win->display , win->my_window , x , y) ;

	return  1 ;
}

int
ml_window_reverse_video(
	ml_window_t *  win
	)
{
	void *  color ;

	color = win->color_table[MLC_FG_COLOR] ;
	win->color_table[MLC_FG_COLOR] = win->color_table[MLC_BG_COLOR]  ;
	win->color_table[MLC_BG_COLOR] = color ;

	XSetForeground( win->display , win->gc , FG_COLOR_PIXEL(win)) ;
	XSetBackground( win->display , win->gc , BG_COLOR_PIXEL(win)) ;

	if( ! win->wall_picture_is_set)
	{
		XSetWindowBackground( win->display , win->my_window , BG_COLOR_PIXEL(win)) ;
	}

	ml_window_clear_all( win) ;

	return  1 ;
}

void
ml_window_clear(
	ml_window_t *  win ,
	int  x ,
	int  y ,
	u_int	width ,
	u_int	height
	)
{
	if( win->drawable == win->pixmap)
	{
		XSetForeground( win->display , win->gc , BG_COLOR_PIXEL(win)) ;
		XFillRectangle( win->display , win->drawable , win->gc ,
			x + win->margin , y + win->margin , width , height) ;
		XSetForeground( win->display , win->gc , FG_COLOR_PIXEL(win)) ;
	}
	else if( win->drawable == win->my_window)
	{
		XClearArea( win->display , win->drawable , x + win->margin , y + win->margin ,
			width , height , 0) ;
	}
}

void
ml_window_clear_all(
	ml_window_t *  win
	)
{
	if( win->drawable == win->pixmap)
	{
		XSetForeground( win->display , win->gc , BG_COLOR_PIXEL(win)) ;
		XFillRectangle( win->display , win->drawable , win->gc ,
			0 , 0 , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win)) ;
		XSetForeground( win->display , win->gc , FG_COLOR_PIXEL(win)) ;
	}
	else if( win->drawable == win->my_window)
	{
		XClearArea( win->display , win->drawable , 0 , 0 ,
			ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) , 0) ;
	}
}

void
ml_window_fill(
	ml_window_t *  win ,
	int  x ,
	int  y ,
	u_int	width ,
	u_int	height
	)
{
	XFillRectangle( win->display , win->drawable , win->gc , x + win->margin , y + win->margin ,
		width , height) ;
}

void
ml_window_fill_all(
	ml_window_t *  win
	)
{
	XFillRectangle( win->display , win->drawable , win->gc , 0 , 0 ,
		ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win)) ;
}

/*
 * only for double buffering
 */
int
ml_window_update_view(
	ml_window_t *  win ,
	int  x ,
	int  y ,
	u_int	width ,
	u_int	height
	)
{
	if( win->pixmap == win->drawable)
	{
		if( win->width < x + width)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG
				" update size (x)%d (w)%d is overflowed.(screen width %d)\n" ,
				x , width , win->width) ;
		#endif

			width = win->width - x ;

		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " width is modified -> %d\n" , width) ;
		#endif
		}

		if( win->height < y + height)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG
				" update size (y)%d (h)%d is overflowed.(screen height is %d)\n" ,
				y , height , win->height) ;
		#endif
				
			height = win->height - y ;

		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " height is modified -> %d\n" , height) ;
		#endif
		}

	#ifdef  PERF_DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " updating y %d - height %d\n" , y , height) ;
	#endif

		XCopyArea( win->display , win->drawable , win->my_window , win->gc ,
			x + win->margin , y + win->margin , width , height ,
			x + win->margin , y + win->margin) ;

		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

int
ml_window_update_view_all(
	ml_window_t *  win
	)
{
	return	ml_window_update_view( win , 0 , 0 , win->width , win->height) ;
}

void
ml_window_idling(
	ml_window_t *  win
	)
{
	int  counter ;
	
	for( counter = 0 ; counter < win->num_of_children ; counter ++)
	{
		ml_window_idling( win->children[counter]) ;
	}

#ifdef  __DEBUG
	if( win->button_is_pressing)
	{
		kik_debug_printf( KIK_DEBUG_TAG " button is pressing...\n") ;
	}
#endif

	if( win->button_is_pressing && win->button_press_continued)
	{
		(*win->button_press_continued)( win , &win->prev_button_press_event) ;
	}
}

int
ml_window_receive_event(
	ml_window_t *   win ,
	XEvent *  event
	)
{
	int  counter ;
	
	for( counter = 0 ; counter < win->num_of_children ; counter ++)
	{
		if( ml_window_receive_event( win->children[counter] , event))
		{
			return  1 ;
		}
	}

	if( win->my_window != event->xany.window)
	{
		/*
		 * XXX
		 * if some window invokes xim window open event and it doesn't have any xic ,
		 * no xim window will be opened at XFilterEvent() in ml_window_manager_receive_next_event().
		 * but it is desired to open xim window of ml_term_screen when its event is invoked
		 * on scrollbar or title bar.
		 * this hack enables it , but this way won't deal with the case that multiple 
		 * xics exist.
		 */
		if( win->xic)
		{
			if( is_in_the_same_window_family( win , event->xany.window) &&
				XFilterEvent( event , win->my_window))
			{
				return  1 ;
			}
		}

		return  0 ;
	}

	if( event->type == KeyPress)
	{
		if( win->key_pressed)
		{
			(*win->key_pressed)( win , &event->xkey) ;
		}
	}
	else if( event->type == FocusIn)
	{
	#ifdef  __DEBUG
		kik_debug_printf( "FOCUS IN %p\n" , event->xany.window) ;
	#endif

	#if  1
		/* root window event only */
		if( win->parent == NULL)
	#endif
		{
			notify_focus_in_to_children( win) ;
		}
	}
	else if( event->type == FocusOut)
	{
	#ifdef  __DEBUG
		kik_debug_printf( "FOCUS OUT %p\n" , event->xany.window) ;
	#endif

	#if  1
		/* root window event only */
		if( win->parent == NULL)
	#endif
		{
			notify_focus_out_to_children( win) ;
		}
	}
	else if( event->type == MotionNotify)
	{
		XEvent  ahead ;
		
		while( XEventsQueued(win->display , QueuedAfterReading))
		{
			XPeekEvent( win->display , &ahead) ;

			if( ahead.type != MotionNotify || ahead.xmotion.window != event->xmotion.window)
			{
				break ;
			}

			XNextEvent( win->display , event) ;
		}

		event->xmotion.x -= win->margin ;
		event->xmotion.y -= win->margin ;
	
		if( win->button_motion)
		{
			(*win->button_motion)( win , &event->xmotion) ;
		}

		if( win->button_is_pressing)
		{
			/* following button motion ... */

			win->prev_button_press_event.x = event->xmotion.x ;
			win->prev_button_press_event.y = event->xmotion.y ;
			win->prev_button_press_event.time = event->xmotion.time ;
		}
	}
	else if( event->type == ButtonRelease)
	{
		event->xbutton.x -= win->margin ;
		event->xbutton.y -= win->margin ;
		
		if( win->button_released)
		{
			(*win->button_released)( win , &event->xbutton) ;
		}

		win->button_is_pressing = 0 ;
	}
	else if( event->type == ButtonPress)
	{
		event->xbutton.x -= win->margin ;
		event->xbutton.y -= win->margin ;

		if( win->click_num == MAX_CLICK)
		{
			win->click_num = 0 ;
		}

		if( win->prev_clicked_time + DOUBLE_CLICK_INTERVAL >= event->xbutton.time &&
			event->xbutton.button == win->prev_clicked_button)
		{
			win->click_num ++ ;
			win->prev_clicked_time = event->xbutton.time ;
		}
		else
		{
			win->click_num = 1 ;
			win->prev_clicked_time = event->xbutton.time ;
			win->prev_clicked_button = event->xbutton.button ;
		}
		
		if( win->button_pressed)
		{
			(*win->button_pressed)( win , &event->xbutton , win->click_num) ;
		}

		if( win->event_mask & ButtonReleaseMask)
		{
			/*
			 * if ButtonReleaseMask is not set and ml_window_t doesn't receive
			 * ButtonRelease event , button_is_pressing flag must never be set ,
			 * since once it is set , it will never unset.
			 */
			win->button_is_pressing = 1 ;
			win->prev_button_press_event = event->xbutton ;
		}
	}
	else if( event->type == Expose /* && event->xexpose.count == 0 */)
	{
		int  x ;
		int  y ;
		u_int  width ;
		u_int  height ;

		if( event->xexpose.x < win->margin)
		{
			x = 0 ;

			if( x + event->xexpose.width > win->width)
			{
				width = win->width ;
			}
			else
			{
				width = event->xexpose.width - (win->margin - event->xexpose.x) ;
			}
		}
		else
		{
			x = event->xexpose.x - win->margin ;

			if( x + event->xexpose.width > win->width)
			{
				width = win->width - x ;
			}
			else
			{
				width = event->xexpose.width ;
			}
		}

		if( event->xexpose.y < win->margin)
		{
			y = 0 ;

			if( y + event->xexpose.height > win->height)
			{
				height = win->height ;
			}
			else
			{
				height = event->xexpose.height - (win->margin - event->xexpose.y) ;
			}
		}
		else
		{
			y = event->xexpose.y - win->margin ;

			if( y + event->xexpose.height > win->height)
			{
				height = win->height - y ;
			}
			else
			{
				height = event->xexpose.height ;
			}
		}

		if( ! ml_window_update_view( win , x , y , width , height))
		{
			if( win->window_exposed)
			{
				(*win->window_exposed)( win , x , y , width , height) ;
			}
		}
	}
	else if( event->type == ConfigureNotify)
	{
		int  is_changed ;
		XEvent  next_ev ;

		is_changed = 0 ;
		
		if( event->xconfigure.x != win->x || event->xconfigure.y != win->y)
		{
			win->x = event->xconfigure.x ;
			win->y = event->xconfigure.y ;

			is_changed = 1 ;
		}
		
		if( event->xconfigure.width != ACTUAL_WIDTH(win) ||
			event->xconfigure.height != ACTUAL_HEIGHT(win))
		{
			win->width = event->xconfigure.width - win->margin * 2 ;
			win->height = event->xconfigure.height - win->margin * 2 ;

			if( win->pixmap)
			{
				XFreePixmap( win->display , win->pixmap) ;
				win->pixmap = XCreatePixmap( win->display ,
					win->parent_window , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
					DefaultDepth( win->display , win->screen)) ;
				win->drawable = win->pixmap ;

			#ifdef  ANTI_ALIAS
				XftDrawChange( win->xft_draw , win->drawable) ;
			#endif
			}

			ml_window_clear_all( win) ;

			if( win->window_resized)
			{
				(*win->window_resized)( win) ;
			}

			ml_xic_resized( win) ;

			is_changed = 1 ;
		}

		/*
		 * transparent processing.
		 */
		 
		if( is_changed &&
			XCheckMaskEvent( win->display , StructureNotifyMask , &next_ev))
		{
			if( next_ev.type == ConfigureNotify)
			{
				is_changed = 0 ;
			}

			XPutBackEvent( win->display , &next_ev) ;
		}
		
		if( is_changed)
		{
			notify_configure_to_children( win) ;
		}
	}
	else if( event->type == ReparentNotify)
	{
		/*
		 * transparent processing.
		 */
		
		notify_reparent_to_children( win) ;
	}
	else if( event->type == SelectionClear)
	{
		if( win->selection_cleared)
		{
			(*win->selection_cleared)( win , &event->xselectionclear) ;
		}

		ml_window_manager_clear_selection( ml_get_root_window( win)->win_man , win) ;
	}
	else if( event->type == SelectionRequest)
	{
		if( event->xselectionrequest.target == XA_STRING)
		{
			if( win->xct_selection_requested)
			{
				(*win->xct_selection_requested)( win , &event->xselectionrequest ,
					event->xselectionrequest.target) ; 
			}
		}
		else if( event->xselectionrequest.target == xa_text ||
			event->xselectionrequest.target == xa_compound_text)
		{
			if( win->xct_selection_requested)
			{
				/*
				 * kterm requests selection with "TEXT" atom , but
				 * wants it to be sent back with "COMPOUND_TEXT" atom.
				 * why ?
				 */
				 
				(*win->xct_selection_requested)( win , &event->xselectionrequest ,
					xa_compound_text) ; 
			}
		}
		else if( event->xselectionrequest.target == xa_utf8_string)
		{
			if( win->utf8_selection_requested)
			{
				(*win->utf8_selection_requested)( win , &event->xselectionrequest ,
					xa_utf8_string) ;
			}
		}
		else
		{
			ml_window_send_selection( win , &event->xselectionrequest , NULL , 0 , 0) ;
		}
	}
	else if( event->type == SelectionNotify)
	{
		if( event->xselection.property == None)
		{
			/*
			 * trying with xa_compound_text => xa_text => XA_STRING
			 */

			if( event->xselection.target == xa_utf8_string)
			{
				if( win->utf8_selection_request_failed)
				{
					(*win->utf8_selection_request_failed)( win , &event->xselection) ;
				}
			}
			else if( event->xselection.target == xa_compound_text)
			{
				XConvertSelection( win->display , XA_PRIMARY , xa_text ,
					xa_selection_prop , win->my_window , CurrentTime) ;
			}
			else if( event->xselection.target == xa_text)
			{
				XConvertSelection( win->display , XA_PRIMARY , XA_STRING ,
					xa_selection_prop , win->my_window , CurrentTime) ;
			}
			else if( win->xct_selection_request_failed)
			{
				(*win->xct_selection_request_failed)( win , &event->xselection) ;
			}
			
			return  1 ;
		}

		if( event->xselection.selection == XA_PRIMARY &&
			( event->xselection.target == XA_STRING ||
			event->xselection.target == xa_text ||
			event->xselection.target == xa_compound_text ||
			event->xselection.target == xa_utf8_string))
		{
			u_long  bytes_after ;
			XTextProperty  ct ;
			int  seg ;

			for( seg = 0 ; ; seg += ct.nitems)
			{
				/*
				 * XXX
				 * long_offset and long_len is the same as rxvt-2.6.3 ,
				 * but I'm not confident if this is ok.
				 */
				if( XGetWindowProperty( win->display , event->xselection.requestor ,
					event->xselection.property , seg / 4 , 4096 , False ,
					AnyPropertyType , &ct.encoding , &ct.format ,
					&ct.nitems , &bytes_after , &ct.value) != Success)
				{
					break ;
				}
				
				if( ct.value == NULL || ct.nitems == 0)
				{
					break ;
				}

				if( event->xselection.target == XA_STRING ||
					event->xselection.target == xa_text ||
					event->xselection.target == xa_compound_text)
				{
					if( win->xct_selection_notified)
					{
						(*win->xct_selection_notified)(
							win , ct.value , ct.nitems) ;
					}
				}
				else if( event->xselection.target == xa_utf8_string)
				{
					if( win->utf8_selection_notified)
					{
						(*win->utf8_selection_notified)(
							win , ct.value , ct.nitems) ;
					}
				}

				XFree( ct.value) ;

				if( bytes_after == 0)
				{
					break ;
				}
			}
		}
		
		XDeleteProperty( win->display , event->xselection.requestor ,
			event->xselection.property) ;
	}
	else if( event->type == VisibilityNotify)
	{
		/* this event is changeable in run time. */
		
		if( win->event_mask & VisibilityChangeMask)
		{
			/*
			 * this event is only received if use_pixmap flag is not set in ml_window_init.
			 */

			if( event->xvisibility.state == VisibilityPartiallyObscured)
			{
				win->is_scrollable = 0 ;
			}
			else if( event->xvisibility.state == VisibilityUnobscured)
			{
				win->is_scrollable = 1 ;
			}
		}
	}
	else if( event->type == ClientMessage)
	{
		if( event->xclient.format == 32 && event->xclient.data.l[0] == xa_delete_window)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG
				" DeleteWindow message is received. exiting...\n") ;
		#endif
			if( win->window_deleted)
			{
				(*win->window_deleted)( win) ;
			}
			else
			{
				exit(0) ;
			}
		}
	}
#ifdef  __DEBUG
	else
	{
		kik_warn_printf( KIK_DEBUG_TAG " event %d is received, but not processed.\n" ,
			event->type) ;
	}
#endif

	return  1 ;
}

size_t
ml_window_get_str(
	ml_window_t *  win ,
	u_char *  seq ,
	size_t  seq_len ,
	mkf_parser_t **  parser ,
	KeySym *  keysym ,
	XKeyEvent *  event
	)
{
	size_t  len ;
	
	if( ( len = ml_xic_get_str( win , seq , seq_len , parser , keysym , event)) > 0)
	{
		return  len ;
	}
	 
	if( ( len = XLookupString( event , seq , seq_len , keysym , NULL)) > 0)
	{
		*parser = NULL ;

                return  len ;
	}

	if( ( len = ml_xic_get_utf8_str( win , seq , seq_len , parser , keysym , event)) > 0)
	{
		return  len ;
	}

	return  0 ;
}

inline int
ml_window_is_scrollable(
	ml_window_t *  win
	)
{
	return  win->is_scrollable ;
}

/*
 * the caller side should clear the scrolled area.
 */
int
ml_window_scroll_upward(
	ml_window_t *  win ,
	u_int	height
	)
{
	if( ! win->is_scrollable)
	{
		return  0 ;
	}
	
	return  ml_window_scroll_upward_region( win , 0 , win->height , height) ;
}

/*
 * the caller side should clear the scrolled out area.
 */
int
ml_window_scroll_upward_region(
	ml_window_t *  win ,
	int  boundary_start ,
	int  boundary_end ,
	u_int	height
	)
{
	if( ! win->is_scrollable)
	{
		return  0 ;
	}
	
	if( boundary_start < 0 || boundary_end > win->height || boundary_end <= boundary_start + height)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" boundary start %d end %d height %d in window((h) %d (w) %d)\n" ,
			boundary_start , boundary_end , height , win->height , win->width) ;
	#endif
	
		return  0 ;
	}
	
	XCopyArea( win->display , win->drawable , win->drawable , win->gc ,
		win->margin , win->margin + boundary_start + height ,	/* src */
		win->width , boundary_end - boundary_start - height ,	/* size */
		win->margin , win->margin + boundary_start) ;		/* dst */

	return  1 ;
}

/*
 * the caller side should clear the scrolled out area.
 */
int
ml_window_scroll_downward(
	ml_window_t *  win ,
	u_int	height
	)
{
	if( ! win->is_scrollable)
	{
		return  0 ;
	}
	
	return  ml_window_scroll_downward_region( win , 0 , win->height , height) ;
}

/*
 * the caller side should clear the scrolled out area.
 */
int
ml_window_scroll_downward_region(
	ml_window_t *  win ,
	int  boundary_start ,
	int  boundary_end ,
	u_int	height
	)
{
	if( ! win->is_scrollable)
	{
		return  0 ;
	}

	if( boundary_start < 0 || boundary_end > win->height || boundary_end <= boundary_start + height)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " boundary start %d end %d height %d\n" ,
			boundary_start , boundary_end , height) ;
	#endif
	
		return  0 ;
	}

	XCopyArea( win->display , win->drawable , win->drawable , win->gc ,
		win->margin , win->margin + boundary_start ,
		win->width , boundary_end - boundary_start - height ,
		win->margin , win->margin + boundary_start + height) ;

	return  1 ;
}

int
ml_window_draw_str(
	ml_window_t *  win ,
	ml_char_t *  chars ,
	u_int	num_of_chars ,
	int  x ,
	int  y ,
	u_int	height ,
	u_int	height_to_baseline
	)
{
	return  draw_str( win , NULL , chars , num_of_chars , x , y , height , height_to_baseline) ;
}

int
ml_window_draw_str_to_eol(
	ml_window_t *  win ,
	ml_char_t *  chars ,
	u_int	num_of_chars ,
	int  x ,
	int  y ,
	u_int	height ,
	u_int	height_to_baseline
	)
{
	u_int  updated_width ;

	if( ! draw_str( win , &updated_width , chars , num_of_chars ,
		x , y , height , height_to_baseline))
	{
		return	0 ;
	}

	if( updated_width < win->width)
	{
		ml_window_clear( win , updated_width , y , win->width - updated_width , height) ;
	}

	return	1 ;
}

int
ml_window_draw_cursor(
	ml_window_t *  win ,
	ml_char_t *  ch ,
	int  x ,
	int  y ,
	u_int  height ,
	u_int  height_to_baseline
	)
{
	int  result ;

	if( win->wall_picture_is_set)
	{
		win->wall_picture_is_set = 0 ;
		result = draw_str( win , NULL , ch , 1 , x , y , height , height_to_baseline) ;
		win->wall_picture_is_set = 1 ;
	}
	else
	{
		result = draw_str( win , NULL , ch , 1 , x , y , height , height_to_baseline) ;
	}
	
	return  result ;	
}

int
ml_window_draw_rect_frame(
	ml_window_t *  win ,
	int  x1 ,
	int  y1 ,
	int  x2 ,
	int  y2
	)
{
	XDrawLine( win->display , win->drawable , win->gc , x1 , y1 , x2 , y1) ;
	XDrawLine( win->display , win->drawable , win->gc , x1 , y1 , x1 , y2) ;
	XDrawLine( win->display , win->drawable , win->gc , x2 , y2 , x2 , y1) ;
	XDrawLine( win->display , win->drawable , win->gc , x2 , y2 , x1 , y2) ;

	return  1 ;
}

int
ml_window_set_selection_owner(
	ml_window_t *  win ,
	Time  time
	)
{
	XSetSelectionOwner( win->display , XA_PRIMARY , win->my_window , time) ;
	
	if( win->my_window != XGetSelectionOwner( win->display , XA_PRIMARY))
	{
		return  0 ;
	}
	else
	{
		return  ml_window_manager_own_selection( ml_get_root_window( win)->win_man , win) ;
	}
}

int
ml_window_xct_selection_request(
	ml_window_t *  win ,
	Time  time
	)
{
	XConvertSelection( win->display , XA_PRIMARY , xa_compound_text , xa_selection_prop ,
		win->my_window , time) ;

	return  1 ;
}

int
ml_window_utf8_selection_request(
	ml_window_t *  win ,
	Time  time
	)
{
	XConvertSelection( win->display , XA_PRIMARY , xa_utf8_string , xa_selection_prop ,
		win->my_window , time) ;

	return  1 ;
}

int
ml_window_send_selection(
	ml_window_t *  win ,
	XSelectionRequestEvent *  req_ev ,
	u_char *  sel_str ,
	size_t  sel_len ,
	Atom  sel_type
	)
{
	XEvent  res_ev ;

	res_ev.xselection.type = SelectionNotify ;
	res_ev.xselection.display = req_ev->display ;
	res_ev.xselection.requestor = req_ev->requestor ;
	res_ev.xselection.selection = req_ev->selection ;
	res_ev.xselection.target = req_ev->target ;
	res_ev.xselection.time = req_ev->time ;

	if( sel_str == NULL)
	{
		res_ev.xselection.property = None ;
	}
	else
	{
		XChangeProperty( win->display , req_ev->requestor ,
			req_ev->property , sel_type ,
			8 , PropModeReplace , sel_str , sel_len) ;
		res_ev.xselection.property = req_ev->property ;
	}
	
	XSendEvent( win->display , res_ev.xselection.requestor , False , 0 , &res_ev) ;

	return  1 ;
}

int
ml_set_window_name(
	ml_window_t *  win ,
	u_char *  name
	)
{
	XTextProperty  prop ;

	if( XmbTextListToTextProperty( win->display , (char**)&name , 1 , XStdICCTextStyle , &prop)
		>= Success)
	{
		XSetWMName( win->display , ml_get_root_window( win)->my_window , &prop) ;

		XFree( prop.value) ;
	}
	else
	{
		/* XXX which is better , doing this or return 0 without doing anything ? */
		XStoreName( win->display , ml_get_root_window( win)->my_window , name) ;
	}

	return  1 ;
}

int
ml_set_icon_name(
	ml_window_t *  win ,
	u_char *  name
	)
{
	XTextProperty  prop ;

	if( XmbTextListToTextProperty( win->display , (char**)&name , 1 , XStdICCTextStyle , &prop)
		>= Success)
	{
		XSetWMIconName( win->display , ml_get_root_window( win)->my_window , &prop) ;

		XFree( prop.value) ;
	}
	else
	{
		/* XXX which is better , doing this or return 0 without doing anything ? */
		XSetIconName( win->display , ml_get_root_window( win)->my_window , name) ;
	}

	return  1 ;
}

#if  0
/*
 * XXX
 * at the present time , not used and not maintained.
 */
int
ml_window_paste(
	ml_window_t *  win ,
	Drawable  src ,
	int  src_x ,
	int  src_y ,
	u_int  src_width ,
	u_int  src_height ,
	int  dst_x ,
	int  dst_y
	)
{
	if( win->width < dst_x + src_width)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " size (x)%d (w)%d is overflowed.(screen width %d)\n" ,
			dst_x , src_width , win->width) ;
	#endif

		src_width = win->width - dst_x ;

		kik_warn_printf( KIK_DEBUG_TAG " width is modified -> %d\n" , src_width) ;
	}

	if( win->height < dst_y + src_height)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " size (y)%d (h)%d is overflowed.(screen height is %d)\n" ,
			dst_y , src_height , win->height) ;
	#endif

		src_height = win->height - dst_y ;

		kik_warn_printf( KIK_DEBUG_TAG " height is modified -> %d\n" , src_height) ;
	}

	XCopyArea( win->display , src , win->drawable , win->gc ,
		src_x , src_y , src_width , src_height ,
		dst_x + win->margin , dst_y + win->margin) ;

	return  1 ;
}
#endif

#ifdef  DEBUG
void
ml_window_dump_children(
	ml_window_t *  win
	)
{
	int  counter ;

	fprintf( stderr , "%p(%li) => " , win , win->my_window) ;
	for( counter = 0 ; counter < win->num_of_children ; counter ++)
	{
		fprintf( stderr , "%p(%li) " , win->children[counter] ,
			win->children[counter]->my_window) ; 
	}
	fprintf( stderr , "\n") ;

	for( counter = 0 ; counter < win->num_of_children ; counter ++)
	{
		ml_window_dump_children( win->children[counter]) ;
	}
}
#endif
