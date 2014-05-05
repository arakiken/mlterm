/*
 *	$Id$
 */

#include  "x_draw_str.h"

#include  <ml_drcs.h>

#ifndef  NO_IMAGE
#include  "x_picture.h"

#define  INLINEPIC_ID(glyph)  (((glyph) >> PICTURE_POS_BITS) & (MAX_INLINE_PICTURES - 1))
#define  INLINEPIC_POS(glyph) ((glyph) & ((1 << PICTURE_POS_BITS) - 1))
#endif

#if  0
#define  PERF_DEBUG
#endif


/* --- static functions --- */

static int
adjust_bd_ul_color(
	x_color_manager_t *  color_man ,
	ml_color_t  fg_color ,
	ml_color_t  bg_color ,
	int  is_bold ,
	int  is_underlined
	)
{
	if( is_bold)
	{
		/* If bg_color == ML_FG_COLOR, it seems to be reversed. */
		if( ( fg_color == ML_FG_COLOR || bg_color == ML_FG_COLOR) &&
		    x_color_manager_adjust_bd_color( color_man))
		{
			return  1 ;
		}
	}

	if( is_underlined)
	{
		if( ( fg_color == ML_FG_COLOR || bg_color == ML_FG_COLOR) &&
		    x_color_manager_adjust_ul_color( color_man))
		{
			return  2 ;
		}
	}

	return  0 ;
}

#ifndef  NO_IMAGE
static int
draw_picture(
	x_window_t *  window ,
	u_int32_t *  glyphs ,
	u_int  num_of_glyphs ,
	int  dst_x ,
	int  dst_y ,
	u_int  ch_width ,
	u_int  line_height
	)
{
	u_int  count ;
	x_inline_picture_t *  cur_pic ;
	u_int  num_of_rows ;
	int  src_x ;
	int  src_y ;
	u_int  src_width ;
	u_int  src_height ;
	u_int  dst_width ;
	int  need_clear ;

	cur_pic = NULL ;

	for( count = 0 ; count < num_of_glyphs ; count++)
	{
		x_inline_picture_t *  pic ;
		int  pos ;
		int  x ;
		u_int  w ;

		if( ! ( pic = x_get_inline_picture( INLINEPIC_ID(glyphs[count]))))
		{
			continue ;
		}

		/*
		 * XXX
		 * pic->col_width isn't used in this function, so it can be
		 * removed in the future.
		 */

		if( pic != cur_pic)
		{
			num_of_rows = (pic->height + pic->line_height - 1) / pic->line_height ;
		}

		pos = INLINEPIC_POS(glyphs[count]) ;
		x = (pos / num_of_rows) * ch_width ;

		if( x + ch_width > pic->width)
		{
			w = pic->width > x ? pic->width - x : 0 ;
		}
		else
		{
			w = ch_width ;
		}

		if( count == 0)
		{
			goto  new_picture ;
		}
		else if( w > 0 && pic == cur_pic && src_x + src_width == x)
		{
			if( ! need_clear && w < ch_width)
			{
				x_window_clear( window , dst_x + dst_width ,
					dst_y , ch_width , line_height) ;
			}

			src_width += w ;
			dst_width += ch_width ;

			if( count + 1 < num_of_glyphs)
			{
				continue ;
			}
		}

		if( need_clear > 0)
		{
			x_window_clear( window , dst_x , dst_y , dst_width , line_height) ;
		}

		if( src_width > 0 && src_height > 0 &&
		    pic->disp == window->disp)
		{
		#ifdef  __DEBUG
			kik_debug_printf(
				"Drawing picture at %d %d (pix %p mask %p x %d y %d w %d h %d)\n" ,
				dst_x , dst_y , cur_pic->pixmap , cur_pic->mask ,
				src_x , src_y , src_width , src_height) ;
		#endif

			x_window_copy_area( window , cur_pic->pixmap , cur_pic->mask ,
				src_x , src_y , src_width , src_height , dst_x , dst_y) ;
		}

		dst_x += dst_width ;

	new_picture:
		src_y = (pos % num_of_rows) * line_height ;
		src_x = x ;
		dst_width = ch_width ;
		cur_pic = pic ;
		need_clear = 0 ;

		if( cur_pic->mask)
		{
			need_clear = 1 ;
		}

		if( src_y + line_height > pic->height)
		{
			need_clear = 1 ;
			src_height = pic->height > src_y ? pic->height - src_y : 0 ;
		}
		else
		{
			src_height = line_height ;
		}

		if( strstr( cur_pic->file_path , "mlterm/animx") && cur_pic->next_frame >= 0)
		{
			/* Don't clear if cur_pic is 2nd or later GIF Animation frame. */
			need_clear = -1 ;
		}

		if( (src_width = w) < ch_width && ! need_clear)
		{
			x_window_clear( window , dst_x , dst_y , ch_width , line_height) ;
		}
	}

	return  1 ;
}
#endif

static int
get_drcs_bitmap(
	char *  glyph ,
	u_int  width ,
	int  x ,
	int  y
	)
{
	return  (glyph[(y / 6) * (width + 1) + x] - '?') & (1 << (y % 6)) ;
}

static int
draw_drcs(
	x_window_t *  window ,
	char **  glyphs ,
	u_int  num_of_glyphs ,
	int  x ,
	int  y ,
	u_int  ch_width ,
	u_int  line_height ,
	x_color_t *  fg_xcolor
	)
{
	int  y_off ;

	for( y_off = 0 ; y_off < line_height ; y_off++)
	{
		u_int  w ;
		int  x_off_sum ;	/* x_off for all glyphs */
		int  x_off ;		/* x_off for each glyph */
		char *  glyph ;
		u_int  glyph_width ;
		u_int  glyph_height ;
		u_int  smpl_width ;
		u_int  smpl_height ;

		w = 0 ;

		for( x_off_sum = 0 ; x_off_sum < ch_width * num_of_glyphs ; x_off_sum++)
		{
			int  left_x ;
			int  top_y ;
			int  hit ;
			int  n_smpl ;
			int  smpl_x ;
			int  smpl_y ;

			if( (x_off = x_off_sum % ch_width) == 0)
			{
				glyph = glyphs[x_off_sum / ch_width] ;

				glyph_width = glyph[0] ;
				glyph_height = glyph[1] ;
				glyph += 2 ;

				if( ( smpl_width = glyph_width / ch_width + 1) >= 3)
				{
					smpl_width = 2 ;
				}

				if( ( smpl_height = glyph_height / line_height + 1) >= 3)
				{
					smpl_height = 2 ;
				}
			}

			left_x = ((int)((double)(x_off * glyph_width) /
					(double)ch_width * 10 + 5)) / 10 - smpl_width / 2 ;
			top_y = ((int)((double)(y_off * glyph_height) /
					(double)line_height * 10 + 5)) / 10 - smpl_height / 2 ;

		#if  0
			kik_debug_printf( KIK_DEBUG_TAG
				"x_off %d: center x %f -> %d\n" ,
				x_off , (double)(x_off * glyph_width) / (double)ch_width ,
				left_x + smpl_width / 2) ;
			kik_debug_printf( KIK_DEBUG_TAG
				"y_off %d: center y %f -> %d\n" ,
				y_off , (double)(y_off * glyph_height) / (double)line_height ,
				top_y + smpl_height / 2) ;
		#endif

			hit = n_smpl = 0 ;

			for( smpl_y = 0 ; smpl_y < smpl_height ; smpl_y++)
			{
				for( smpl_x = 0 ; smpl_x < smpl_width ; smpl_x++)
				{
					if( 0 <= left_x + smpl_x &&
					    left_x + smpl_x <= glyph_width &&
					    0 <= top_y + smpl_y &&
					    top_y + smpl_y <= glyph_height)
					{
						if( get_drcs_bitmap( glyph , glyph_width ,
							left_x + smpl_x , top_y + smpl_y))
						{
							hit ++ ;
						}
						n_smpl ++ ;
					}
				}
			}

			if( n_smpl <= hit * 2)
			{
				w ++ ;

				if( x_off_sum + 1 == ch_width * num_of_glyphs)
				{
					/* for x_off - w */
					x_off_sum ++ ;
				}
				else
				{
					continue ;
				}
			}
			else if( w == 0)
			{
				continue ;
			}

			x_window_fill_with( window , fg_xcolor ,
				x + x_off_sum - w , y + y_off , w , 1) ;
			w = 0 ;
		}
	}

	return  1 ;
}


static int
get_state(
	mkf_charset_t  ch_cs ,
	u_int32_t  ch_code ,
	ml_char_t *  comb_chars ,
	u_int32_t *  pic_glyph ,
	char **  drcs_glyph ,
	int *  draw_alone
	)
{
#ifndef  NO_IMAGE
	if( comb_chars && ml_char_cs( comb_chars) == PICTURE_CHARSET)
	{
		*draw_alone = 0 ;	/* forcibly set 0 regardless of xfont. */
		*pic_glyph = ml_char_code( comb_chars) |
		             (ml_char_picture_id( comb_chars) << PICTURE_POS_BITS) ;
		*drcs_glyph = NULL ;

		return  4 ;
	}
	else
#endif
	{
		*pic_glyph = 0 ;

		if( ( *drcs_glyph = ml_drcs_get_glyph( ch_cs , ch_code)))
		{
			*draw_alone = 0 ;	/* forcibly set 0 regardless of xfont. */

			return  3 ;
		}
		else
		{
			if( comb_chars)
			{
				*draw_alone = 1 ;
			}

			if( ch_cs == DEC_SPECIAL)
			{
				return  1 ;
			}
			else
			{
				return  0 ;
			}
		}
	}
}


#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT) || defined(USE_TYPE_CAIRO)

static int
fc_draw_combining_chars(
	x_window_t *  window ,
	x_font_manager_t *  font_man ,
	x_color_manager_t *  color_man ,
	ml_char_t *  chars ,
	u_int  size ,
	int  x ,
	int  y
	)
{
	u_int  count ;
	u_int32_t  ch_code ;
	mkf_charset_t  ch_cs ;
	x_font_t *  xfont ;
	x_color_t *  xcolor ;

	for( count = 0 ; count < size ; count ++)
	{
		if( ml_char_cols( &chars[count]) == 0)
		{
			continue ;
		}

		ch_code = ml_char_code( &chars[count]) ;
		ch_cs = ml_char_cs( &chars[count]) ;
		xfont = x_get_font( font_man , ml_char_font( &chars[count])) ;
		xcolor = x_get_xcolor( color_man , ml_char_fg_color( &chars[count])) ;

		if( ch_cs == DEC_SPECIAL)
		{
			u_char  c ;

			c = ch_code ;
			x_window_draw_decsp_string( window , xfont , xcolor , x , y , &c , 1) ;
		}
		/* ISCII characters never have combined ones. */
		else if( ch_cs == US_ASCII || ch_cs == ISO8859_1_R /* || IS_ISCII(ch_cs) */)
		{
			u_char  c ;

			c = ch_code ;
			x_window_ft_draw_string8( window , xfont , xcolor , x , y , &c , 1) ;
		}
		else
		{
			/* FcChar32 */ u_int32_t  ucs4 ;

			if( ( ucs4 = x_convert_to_xft_ucs4( ch_code , ch_cs)))
			{
				x_window_ft_draw_string32( window , xfont , xcolor ,
					x , y , &ucs4 , 1) ;
			}
		}
	}

	return  1 ;
}

static int
fc_draw_str(
	x_window_t *  window ,
	x_font_manager_t *  font_man ,
	x_color_manager_t *  color_man ,
	u_int *  updated_width ,
	ml_char_t *  chars ,
	u_int  num_of_chars ,
	int  x ,
	int  y ,
	u_int  height ,
	u_int  ascent ,
	u_int  top_margin ,
	u_int  bottom_margin ,
	int  hide_underline
	)
{
	int  count ;
	int  start_draw ;
	int  end_of_str ;
	u_int  current_width ;
	/* FcChar8 */ u_int8_t *  str8 ;
	/* FcChar32 */ u_int32_t *  str32 ;
	u_int	str_len ;

	u_int32_t  ch_code ;
	u_int  ch_width ;
	mkf_charset_t  ch_cs ;

	int  state ;
	x_font_t *  xfont ;
	ml_font_t  font ;
	ml_color_t  fg_color ;
	ml_color_t  bg_color ;
	int  is_underlined ;
	ml_char_t *  comb_chars ;
	u_int  comb_size ;
	int  draw_alone ;
	u_int32_t  pic_glyph ;
	u_int32_t *  pic_glyphs ;
	char *  drcs_glyph ;
	char **  drcs_glyphs ;

	int  next_state ;
	x_font_t *  next_xfont ;
	ml_font_t  next_font ;
	ml_color_t  next_fg_color ;
	ml_color_t  next_bg_color ;
	int  next_is_underlined ;
	ml_char_t *  next_comb_chars ;
	u_int  next_comb_size ;
	u_int  next_ch_width ;
	int  next_draw_alone ;

#ifdef  PERF_DEBUG
	int  draw_count = 0 ;
#endif

	if( num_of_chars == 0)
	{
	#ifdef	__DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " input chars length is 0(x_window_draw_str).\n") ;
	#endif

		return	1 ;
	}

	start_draw = 0 ;
	end_of_str = 0 ;

	count = 0 ;
	while( ml_char_cols( &chars[count]) == 0)
	{
		if( ++ count >= num_of_chars)
		{
			return  1 ;
		}
	}

	ch_code = ml_char_code( &chars[count]) ;
	xfont = x_get_font( font_man , (font = ml_char_font( &chars[count]))) ;
	ch_cs = FONT_CS(font) ;

	ch_width = x_calculate_char_width( xfont , ch_code , ch_cs , &draw_alone) ;

	if( ( current_width = x + ch_width) > window->width ||
	    y + height > window->height)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" draw string outside screen. (x %d w %d y %d h %d)\n" ,
			x , ch_width , y , height) ;
	#endif

		return  0 ;
	}

	comb_chars = ml_get_combining_chars( &chars[count] , &comb_size) ;

	if( ( state = get_state( ch_cs , ch_code , comb_chars ,
			&pic_glyph , &drcs_glyph , &draw_alone)) == 0 &&
	    ch_cs != US_ASCII && ch_cs != ISO8859_1_R && ! IS_ISCII(ch_cs))
	{
		state = 2 ;
	}

	fg_color = ml_char_fg_color( &chars[count]) ;
	bg_color = ml_char_bg_color( &chars[count]) ;

	is_underlined = ml_char_is_underlined( &chars[count]) ;

	if( ! ( str8 = str32 = pic_glyphs = drcs_glyphs =
		alloca( K_MAX(sizeof(*str8),K_MAX(sizeof(*str32),
		        K_MAX(sizeof(*pic_glyphs),sizeof(*drcs_glyphs)))) * num_of_chars)))
	{
		return  0 ;
	}

	str_len = 0 ;

	while( 1)
	{
		if( state <= 1)
		{
			str8[str_len++] = ch_code ;
		}
		else if( state >= 3)
		{
			if( drcs_glyph)
			{
				drcs_glyphs[str_len ++] = drcs_glyph ;
			}
			else /* if( pic_glyph) */
			{
				pic_glyphs[str_len ++] = pic_glyph ;
			}
		}
		else /* if( state == 2) */
		{
			u_int32_t  ucs4 ;

			if( ! ( ucs4 = x_convert_to_xft_ucs4( ch_code , ch_cs)))
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " strange character , ignored.\n") ;
			#endif
				ucs4 = 0x20 ;
			}

			str32[str_len++] = ucs4 ;
		}

		/*
		 * next character.
		 */

		do
		{
			if( ++ count >= num_of_chars)
			{
				start_draw = 1 ;
				end_of_str = 1 ;

				break ;
			}
		}
		while( ml_char_cols( &chars[count]) == 0) ;

		if( ! end_of_str)
		{
			ch_code = ml_char_code( &chars[count]) ;
			next_xfont = x_get_font( font_man ,
					(next_font = ml_char_font( &chars[count]))) ;
			ch_cs = FONT_CS(next_font) ;
			next_fg_color = ml_char_fg_color( &chars[count]) ;
			next_bg_color = ml_char_bg_color( &chars[count]) ;
			next_is_underlined = ml_char_is_underlined( &chars[count]) ;
			next_ch_width = x_calculate_char_width( next_xfont ,
						ch_code , ch_cs , &next_draw_alone) ;
			next_comb_chars = ml_get_combining_chars( &chars[count] ,
							&next_comb_size) ;

			if( ( next_state = get_state( ch_cs , ch_code , next_comb_chars ,
						&pic_glyph , &drcs_glyph ,
						&next_draw_alone)) == 0 &&
			    ch_cs != US_ASCII && ch_cs != ISO8859_1_R && ! IS_ISCII(ch_cs))
			{
				next_state = 2 ;
			}

			if( current_width + next_ch_width > window->width)
			{
				start_draw = 1 ;
				end_of_str = 1 ;
			}
			/*
			 * !! Notice !!
			 * next_xfont != xfont doesn't necessarily detect change of 'state'
			 * (for example, same Unicode font is used for both US_ASCII and
			 * other half-width unicode characters) and 'bold'(x_get_font()
			 * might substitute normal fonts for bold ones), 'next_state' and
			 * 'font & FONT_BOLD' is necessary.
			 */
			else if( next_xfont != xfont
				|| next_fg_color != fg_color
				|| next_bg_color != bg_color
				|| next_is_underlined != is_underlined
				/*
				 * Eevn if both is_underline and next_is_underline are 1,
				 * underline is drawn one by one in vertical mode.
				 */
				|| (is_underlined && xfont->is_vertical)
				|| state != next_state
				|| draw_alone
				|| next_draw_alone
				/* FONT_BOLD flag is not the same. */
			        || ((font ^ next_font) & FONT_BOLD))
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

			int  color_adjusted ;	/* 0 => None , 1 => BD , 2 => UL */
			x_color_t *  fg_xcolor ;
			x_color_t *  bg_xcolor ;

		#ifdef  PERF_DEBUG
			draw_count ++ ;
		#endif

		#ifndef  NO_IMAGE
			if( state == 4)
			{
				draw_picture( window , pic_glyphs , str_len , x , y ,
					ch_width , height) ;

				goto  end_draw ;
			}
		#endif

			color_adjusted = adjust_bd_ul_color( color_man , fg_color , bg_color ,
							font & FONT_BOLD , is_underlined) ;
			fg_xcolor = x_get_xcolor( color_man , fg_color) ;
			bg_xcolor = x_get_xcolor( color_man , bg_color) ;

			/*
			 * clearing background
			 */
			if( bg_color == ML_BG_COLOR)
			{
				if( updated_width)
				{
					x_window_clear( window ,
						x , y , current_width - x , height) ;
				}
			}
			else
			{
				x_window_fill_with( window , bg_xcolor ,
					x , y , current_width - x , height) ;
			}

			/*
			 * drawing string
			 */
			if( state == 0)
			{
				x_window_ft_draw_string8( window , xfont , fg_xcolor ,
					x , y + ascent , str8 , str_len) ;
			}
			else if( state == 1)
			{
				x_window_draw_decsp_string( window , xfont , fg_xcolor ,
					x , y + ascent , str8 , str_len) ;
			}
			else if( state == 2)
			{
				x_window_ft_draw_string32( window , xfont , fg_xcolor ,
					x , y + ascent , str32 , str_len) ;
			}
			else /* if( state == 3) */
			{
				draw_drcs( window , drcs_glyphs , str_len ,
					x , y , ch_width , height , fg_xcolor) ;
			}

			if( comb_chars)
			{
				fc_draw_combining_chars( window , font_man , color_man ,
					comb_chars , comb_size ,
				/*
				 * 'current_width' is for some thai fonts which automatically
				 * draw combining chars.
				 * e.g.)
				 *  -thai-fixed-medium-r-normal--14-100-100-100-m-70-tis620.2529-1
				 *  (distributed by ZzzThai http://zzzthai.fedu.uec.ac.jp/ZzzThai/)
				 *  win32 unicode font.
				 */
				#if  0
					current_width
				#else
					current_width - ch_width
				#endif
					, y + ascent) ;
			}

			if( ! hide_underline && color_adjusted != 2 && is_underlined)
			{
				if( xfont->is_vertical)
				{
					x_window_fill_with( window , fg_xcolor ,
						x , y , (ch_width >> 4) + 1 , height) ;
				}
				else
				{
					x_window_fill_with( window , fg_xcolor ,
						x , y + ascent ,
						current_width - x ,
						((ascent - bottom_margin) >> 4) + 1 ) ;
				}
			}

			if( color_adjusted)
			{
				if( color_adjusted == 1)
				{
					x_color_manager_adjust_bd_color( color_man) ;
				}
				else /* if( color_adjusted == 2) */
				{
					x_color_manager_adjust_ul_color( color_man) ;
				}
			}

		end_draw:
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
		font = next_font ;
		fg_color = next_fg_color ;
		bg_color = next_bg_color ;
		state = next_state ;
		draw_alone = next_draw_alone ;
		comb_chars = next_comb_chars ;
		comb_size = next_comb_size ;
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

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XCORE)

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
	u_int  count ;
	u_int32_t  ch_code ;
	mkf_charset_t  ch_cs ;
	x_font_t *  xfont ;
	x_color_t *  xcolor ;

	for( count = 0 ; count < size ; count ++)
	{
		if( ml_char_cols( &chars[count]) == 0)
		{
			continue ;
		}

		ch_code = ml_char_code( &chars[count]) ;
		ch_cs = ml_char_cs( &chars[count]) ;
		xfont = x_get_font( font_man , ml_char_font( &chars[count])) ;
		xcolor = x_get_xcolor( color_man , ml_char_fg_color( &chars[count])) ;

		if( ch_cs == DEC_SPECIAL)
		{
			u_char  c ;

			c = ch_code ;
			x_window_draw_decsp_string( window , xfont , xcolor , x , y , &c , 1) ;
		}
		else if( ch_code < 0x100)
		{
			u_char  c ;

			c = ch_code ;
			x_window_draw_string( window , xfont , xcolor , x , y , &c , 1) ;
		}
		else if( ch_cs != ISO10646_UCS4_1)
		{
			XChar2b  xch ;

			xch.byte1 = (ch_code >> 8) & 0xff ;
			xch.byte2 = ch_code & 0xff ;

			x_window_draw_string16( window , xfont , xcolor ,
				x , y , &xch , 1) ;
		}
		else
		{
			/* UCS4 */

			/* [2] is for surroage pair. */
			XChar2b  xch[2] ;
			u_int  len ;

			if( ( len = (x_convert_ucs4_to_utf16( xch , ch_code) / 2)) > 0)
			{
				x_window_draw_string16( window , xfont , xcolor ,
					x , y , xch , len) ;
			}
		}
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
	u_int  ascent ,
	u_int  top_margin ,
	u_int  bottom_margin ,
	int  hide_underline
	)
{
	int  count ;
	int  start_draw ;
	int  end_of_str ;
	u_int	current_width ;
	u_char *  str ;
	XChar2b *  str2b ;
	u_int	str_len ;
	u_int32_t  ch_code ;
	mkf_charset_t  ch_cs ;

	int  state ;			/* 0(8bit),1(decsp),2(16bit) */
	ml_char_t *  comb_chars ;
	u_int  comb_size ;
	u_int  ch_width ;
	x_font_t *  xfont ;
	ml_font_t  font ;
	ml_color_t  fg_color ;
	ml_color_t  bg_color ;
	int  is_underlined ;
	int  draw_alone ;
	u_int32_t  pic_glyph ;
	u_int32_t *  pic_glyphs ;
	char *  drcs_glyph ;
	char **  drcs_glyphs ;

	int  next_state ;
	ml_char_t *  next_comb_chars ;
	u_int  next_comb_size ;
	u_int  next_ch_width ;
	x_font_t *  next_xfont ;
	ml_font_t  next_font ;
	ml_color_t  next_fg_color ;
	ml_color_t  next_bg_color ;
	int  next_is_underlined ;
	int  next_draw_alone ;
#ifdef  PERF_DEBUG
	int  draw_count = 0 ;
#endif

	if( num_of_chars == 0)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " input chars length is 0(x_window_draw_str).\n") ;
	#endif

		return	1 ;
	}

	count = 0 ;
	while( ml_char_cols( &chars[count]) == 0)
	{
		if( ++ count >= num_of_chars)
		{
			return  1 ;
		}
	}

	start_draw = 0 ;
	end_of_str = 0 ;

	ch_code = ml_char_code( &chars[count]) ;
	xfont = x_get_font( font_man , (font = ml_char_font( &chars[count]))) ;
	ch_cs = FONT_CS(font) ;

	ch_width = x_calculate_char_width( xfont , ch_code , ch_cs , &draw_alone) ;

	if( ( current_width = x + ch_width) > window->width ||
	    y + height > window->height)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" draw string outside screen. (x %d w %d y %d h %d)\n" ,
			x , ch_width , y , height) ;
	#endif

		return  0 ;
	}

	comb_chars = ml_get_combining_chars( &chars[count] , &comb_size) ;

	if( ( state = get_state( ch_cs , ch_code , comb_chars ,
			&pic_glyph , &drcs_glyph , &draw_alone)) == 0 && ch_code >= 0x100)
	{
		state = 2 ;
	}

	fg_color = ml_char_fg_color( &chars[count]) ;
	bg_color = ml_char_bg_color( &chars[count]) ;
	is_underlined = ml_char_is_underlined( &chars[count]) ;

	if( ! ( str2b = str = pic_glyphs = drcs_glyphs =
		/* '* 2' is for UTF16 surrogate pair. */
		alloca( K_MAX(sizeof(*str2b)*2,K_MAX(sizeof(*str),
		        K_MAX(sizeof(*pic_glyphs),sizeof(*drcs_glyphs)))) * num_of_chars)))
	{
		return	0 ;
	}

	str_len = 0 ;

	while( 1)
	{
		if( state <= 1)
		{
			str[str_len++] = ch_code ;
		}
		else if( state >= 3)
		{
			if( pic_glyph)
			{
				pic_glyphs[str_len++] = pic_glyph ;
			}
			else /* if( drcs_glyph) */
			{
				drcs_glyphs[str_len++] = drcs_glyph ;
			}
		}
		else if( ch_cs != ISO10646_UCS4_1)
		{
			str2b[str_len].byte1 = (ch_code >> 8) & 0xff ;
			str2b[str_len].byte2 = ch_code & 0xff ;
			str_len ++ ;
		}
		else
		{
			/* UCS4 */

			str_len += (x_convert_ucs4_to_utf16( str2b + str_len , ch_code) / 2) ;
		}

		/*
		 * next character.
		 */

		do
		{
			if( ++ count >= num_of_chars)
			{
				start_draw = 1 ;
				end_of_str = 1 ;

				break ;
			}
		}
		while( ml_char_cols( &chars[count]) == 0) ;

		if( ! end_of_str)
		{
			ch_code = ml_char_code( &chars[count]) ;
			next_xfont = x_get_font( font_man ,
					(next_font = ml_char_font( &chars[count]))) ;
			ch_cs = FONT_CS(next_font) ;
			next_fg_color = ml_char_fg_color( &chars[count]) ;
			next_bg_color = ml_char_bg_color( &chars[count]) ;
			next_is_underlined = ml_char_is_underlined( &chars[count]) ;
			next_ch_width = x_calculate_char_width( next_xfont ,
						ch_code , ch_cs , &next_draw_alone) ;
			next_comb_chars = ml_get_combining_chars( &chars[count] ,
							&next_comb_size) ;

			if( ( next_state = get_state( ch_cs , ch_code , next_comb_chars ,
						&pic_glyph , &drcs_glyph ,
						&next_draw_alone)) == 0 && ch_code >= 0x100)
			{
				next_state = 2 ;
			}

			if( current_width + next_ch_width > window->width)
			{
				start_draw = 1 ;
				end_of_str = 1 ;
			}
			/*
			 * !! Notice !!
			 * next_xfont != xfont doesn't necessarily detect change of 'state'
			 * (for example, same Unicode font is used for both US_ASCII and
			 * other half-width unicode characters) and 'bold'(x_get_font()
			 * might substitute normal fonts for bold ones), 'next_state' and
			 * 'font & FONT_BOLD' is necessary.
			 */
			else if( next_xfont != xfont
				|| next_fg_color != fg_color
				|| next_bg_color != bg_color
				|| next_is_underlined != is_underlined
				/*
				 * Eevn if both is_underline and next_is_underline are 1,
				 * underline is drawn one by one in vertical mode.
				 */
				|| (is_underlined && xfont->is_vertical)
				|| next_state != state
				|| draw_alone
				|| next_draw_alone
				/* FONT_BOLD flag is not the same */
			        || ((font ^ next_font) & FONT_BOLD))
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

			int  color_adjusted ;	/* 0 => None , 1 => BD , 2 => UL */
			x_color_t *  fg_xcolor ;
			x_color_t *  bg_xcolor ;

		#ifdef  PERF_DEBUG
			draw_count ++ ;
		#endif

		#ifndef  NO_IMAGE
			if( state == 4)
			{
				draw_picture( window , pic_glyphs , str_len , x , y ,
					ch_width , height) ;

				goto  end_draw ;
			}
		#endif

			color_adjusted = adjust_bd_ul_color( color_man , fg_color , bg_color ,
							font & FONT_BOLD , is_underlined) ;

			fg_xcolor = x_get_xcolor( color_man , fg_color) ;

		#ifdef  USE_FRAMEBUFFER
			if( x_window_has_wall_picture( window) &&
			           bg_color == ML_BG_COLOR)
			{
				bg_xcolor = NULL ;
			}
			else
		#endif
			{
				bg_xcolor = x_get_xcolor( color_man , bg_color) ;
			}

			if(
			#ifndef  USE_FRAMEBUFFER
			    ( x_window_has_wall_picture( window) &&
			           bg_color == ML_BG_COLOR) ||
			#endif
				bottom_margin + top_margin > 0 /* == line space XXX */ ||
				draw_alone || state == 3)
			{
				if( bg_color == ML_BG_COLOR)
				{
					x_window_clear( window ,
						x , y , current_width - x , height) ;
				}
				else
				{
				#ifdef  __DEBUG
					kik_debug_printf( KIK_DEBUG_TAG "prop font is used.\n") ;
				#endif

					x_window_fill_with( window , bg_xcolor ,
						x , y , current_width - x , height) ;
				}

				if( state == 2)
				{
					x_window_draw_string16( window , xfont , fg_xcolor ,
						x , y + ascent , str2b , str_len) ;
				}
				else if( state == 1)
				{
					x_window_draw_decsp_string( window , xfont , fg_xcolor ,
						x , y + ascent , str , str_len) ;
				}
				else if( state == 0)
				{
					x_window_draw_string( window , xfont , fg_xcolor ,
						x , y + ascent , str , str_len) ;
				}
				else /* if( state == 3) */
				{
					draw_drcs( window , drcs_glyphs , str_len ,
						x , y , ch_width , height , fg_xcolor) ;
				}
			}
			else
			{
				if( state == 2)
				{
					x_window_draw_image_string16( window , xfont ,
						fg_xcolor , bg_xcolor ,
						x , y + ascent , str2b , str_len) ;
				}
				else if( state == 1)
				{
					x_window_draw_decsp_image_string( window , xfont ,
						fg_xcolor , bg_xcolor ,
						x , y + ascent , str , str_len) ;
				}
				else /* if( state == 0) */
				{
					x_window_draw_image_string( window , xfont ,
						fg_xcolor , bg_xcolor ,
						x , y + ascent , str , str_len) ;
				}
			}

			if( comb_chars)
			{
				xcore_draw_combining_chars( window , font_man , color_man ,
					comb_chars , comb_size ,
				/*
				 * 'current_width' is for some thai fonts which automatically
				 * draw combining chars.
				 * e.g.)
				 *  -thai-fixed-medium-r-normal--14-100-100-100-m-70-tis620.2529-1
				 *  (distributed by ZzzThai http://zzzthai.fedu.uec.ac.jp/ZzzThai/)
				 *  win32 unicode font.
				 */
				#if  0
					current_width
				#else
					current_width - ch_width
				#endif
					, y + ascent) ;
			}

			if( ! hide_underline && color_adjusted != 2 && is_underlined)
			{
				if( xfont->is_vertical)
				{
					x_window_fill_with( window , fg_xcolor ,
						x , y , (ch_width >> 4) + 1 , height) ;
				}
				else
				{
					x_window_fill_with( window , fg_xcolor ,
						x , y + ascent ,
						current_width - x ,
						((ascent - bottom_margin) >> 4) + 1) ;
				}
			}

			if( color_adjusted)
			{
				if( color_adjusted == 1)
				{
					x_color_manager_adjust_bd_color( color_man) ;
				}
				else /* if( color_adjusted == 2) */
				{
					x_color_manager_adjust_ul_color( color_man) ;
				}
			}

		end_draw:
			start_draw = 0 ;

			x = current_width ;
			str_len = 0 ;
		}

		if( end_of_str)
		{
			break ;
		}

		xfont = next_xfont ;
		font = next_font ;
		fg_color = next_fg_color ;
		bg_color = next_bg_color ;
		is_underlined = next_is_underlined ;
		state = next_state ;
		draw_alone = next_draw_alone ;
		comb_chars = next_comb_chars ;
		comb_size = next_comb_size ;
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
	u_int  ascent ,
	u_int  top_margin ,
	u_int  bottom_margin ,
	int  hide_underline
	)
{
	u_int  updated_width ;

	switch( x_get_type_engine( font_man))
	{
	default:
		return  0 ;

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT) || defined(USE_TYPE_CAIRO)
	case  TYPE_XFT:
	case  TYPE_CAIRO:
		if( ! fc_draw_str( window , font_man , color_man , &updated_width ,
			chars , num_of_chars , x , y , height , ascent ,
			top_margin , bottom_margin , hide_underline))
		{
			return  0 ;
		}

		break ;
#endif

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XCORE)
	case  TYPE_XCORE:
		if( ! xcore_draw_str( window , font_man , color_man , &updated_width ,
			chars , num_of_chars , x , y , height , ascent ,
			top_margin , bottom_margin , hide_underline))
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
	u_int  ascent ,
	u_int  top_margin ,
	u_int  bottom_margin ,
	int  hide_underline
	)
{
	u_int  updated_width ;

	switch( x_get_type_engine( font_man))
	{
	default:
		return  0 ;

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT) || defined(USE_TYPE_CAIRO)
	case  TYPE_XFT:
	case  TYPE_CAIRO:
		x_window_clear( window , x , y , window->width - x , height) ;

		if( ! fc_draw_str( window , font_man , color_man ,
			NULL /* NULL disables x_window_clear() in fc_draw_str() */ ,
			chars , num_of_chars , x , y , height , ascent ,
			top_margin , bottom_margin , hide_underline))
		{
			return  0 ;
		}

		break ;
#endif

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XCORE)
	case  TYPE_XCORE:
		if( ! xcore_draw_str( window , font_man , color_man , &updated_width ,
			chars , num_of_chars , x , y , height , ascent ,
			top_margin , bottom_margin , hide_underline))
		{
			return	0 ;
		}

		if( updated_width < window->width)
		{
			x_window_clear( window , updated_width , y ,
				window->width - updated_width , height) ;
		}

		break ;
#endif
	}

	return	1 ;
}
