/*
 *	$Id$
 */

#include  "../x_font.h"

#include  <stdio.h>
#include  <string.h>		/* memset/strncasecmp */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>	/* kik_snprintf */
#include  <kiklib/kik_mem.h>	/* alloca */
#include  <kiklib/kik_str.h>	/* kik_str_sep/kik_str_to_int */
#include  <kiklib/kik_locale.h>	/* kik_get_lang() */
#include  <mkf/mkf_ucs4_map.h>
#include  <mkf/mkf_ucs_property.h>
#include  <ml_char_encoding.h>	/* x_convert_to_xft_ucs4 */

#ifdef  USE_OT_LAYOUT
#include  "otl.h"
#endif


#define  FOREACH_FONT_ENCODINGS(csinfo,font_encoding_p) \
	for( (font_encoding_p) = &csinfo->encoding_names[0] ; *(font_encoding_p) ; (font_encoding_p) ++)

#if  0
#define  __DEBUG
#endif


typedef struct  wincs_info
{
	DWORD  cs ;
	ml_char_encoding_t  encoding ;
	
} wincs_info_t ;

typedef struct  cs_info
{
	mkf_charset_t  cs ;
	DWORD  wincs ;

} cs_info_t ;


/* --- static variables --- */

static wincs_info_t  wincs_info_table[] =
{
	{ DEFAULT_CHARSET , ML_UNKNOWN_ENCODING , } ,
	{ SYMBOL_CHARSET , ML_UNKNOWN_ENCODING , } ,
	{ OEM_CHARSET , ML_UNKNOWN_ENCODING , } ,
	{ ANSI_CHARSET , ML_CP1252 , } ,
	{ RUSSIAN_CHARSET , ML_CP1251 , } ,
	{ GREEK_CHARSET , ML_CP1253 , } ,
	{ TURKISH_CHARSET , ML_CP1254 , } ,
	{ BALTIC_CHARSET , ML_CP1257 , } ,
	{ HEBREW_CHARSET , ML_CP1255 , } ,
	{ ARABIC_CHARSET , ML_CP1256 , } ,
	{ SHIFTJIS_CHARSET , ML_SJIS , } ,
	{ HANGEUL_CHARSET , ML_UHC , } ,
	{ GB2312_CHARSET , ML_GBK , } ,
	{ CHINESEBIG5_CHARSET , ML_BIG5 } ,
	{ JOHAB_CHARSET , ML_JOHAB , } ,
	{ THAI_CHARSET , ML_TIS620 , } ,
	{ EASTEUROPE_CHARSET , ML_ISO8859_3 , } ,
	{ MAC_CHARSET , ML_UNKNOWN_ENCODING , } ,
} ;

static cs_info_t  cs_info_table[] =
{
	{ ISO10646_UCS4_1 , DEFAULT_CHARSET , } ,
	
	{ DEC_SPECIAL , SYMBOL_CHARSET , } ,
	{ ISO8859_1_R , ANSI_CHARSET , } ,
	{ ISO8859_2_R , DEFAULT_CHARSET , } ,
	{ ISO8859_3_R , EASTEUROPE_CHARSET , } ,
	{ ISO8859_4_R , DEFAULT_CHARSET , } ,
	{ ISO8859_5_R , RUSSIAN_CHARSET , } ,
	{ ISO8859_6_R , ARABIC_CHARSET , } ,
	{ ISO8859_7_R , GREEK_CHARSET , } ,
	{ ISO8859_8_R , HEBREW_CHARSET , } ,
	{ ISO8859_9_R , TURKISH_CHARSET , } ,
	{ ISO8859_10_R , DEFAULT_CHARSET , } ,
	{ TIS620_2533 , THAI_CHARSET , } ,
	{ ISO8859_13_R , DEFAULT_CHARSET , } ,
	{ ISO8859_14_R , DEFAULT_CHARSET , } ,
	{ ISO8859_15_R , DEFAULT_CHARSET , } ,
	{ ISO8859_16_R , DEFAULT_CHARSET , } ,
	{ TCVN5712_3_1993 , VIETNAMESE_CHARSET , } ,
	{ ISCII_ASSAMESE , DEFAULT_CHARSET , } ,
	{ ISCII_BENGALI , DEFAULT_CHARSET , } ,
	{ ISCII_GUJARATI , DEFAULT_CHARSET , } ,
	{ ISCII_HINDI , DEFAULT_CHARSET , } ,
	{ ISCII_KANNADA , DEFAULT_CHARSET , } ,
	{ ISCII_MALAYALAM , DEFAULT_CHARSET , } ,
	{ ISCII_ORIYA , DEFAULT_CHARSET , } ,
	{ ISCII_PUNJABI , DEFAULT_CHARSET , } ,
	{ ISCII_TAMIL , DEFAULT_CHARSET , } ,
	{ ISCII_TELUGU , DEFAULT_CHARSET , } ,
	{ VISCII , VIETNAMESE_CHARSET , } ,
	{ KOI8_R , RUSSIAN_CHARSET , } ,
	{ KOI8_U , RUSSIAN_CHARSET , } ,
	{ KOI8_T , RUSSIAN_CHARSET , } ,
	{ GEORGIAN_PS , DEFAULT_CHARSET , } ,
	{ CP1250 , EASTEUROPE_CHARSET , } ,
	{ CP1251 , RUSSIAN_CHARSET , } ,
	{ CP1252 , ANSI_CHARSET , } ,
	{ CP1253 , GREEK_CHARSET , } ,
	{ CP1254 , TURKISH_CHARSET , } ,
	{ CP1255 , HEBREW_CHARSET , } ,
	{ CP1256 , ARABIC_CHARSET , } ,
	{ CP1257 , BALTIC_CHARSET , } ,
	{ CP1258 , VIETNAMESE_CHARSET , } ,

	{ JISX0201_KATA , SHIFTJIS_CHARSET , } ,
	{ JISX0201_ROMAN , SHIFTJIS_CHARSET , } ,
	{ JISC6226_1978 , SHIFTJIS_CHARSET , } ,
	{ JISX0208_1983 , SHIFTJIS_CHARSET , } ,
	{ JISX0208_1990 , SHIFTJIS_CHARSET , } ,
	{ JISX0212_1990 , SHIFTJIS_CHARSET , } ,
	{ JISX0213_2000_1 , SHIFTJIS_CHARSET , } ,
	{ JISX0213_2000_2 , SHIFTJIS_CHARSET , } ,
	
	{ KSC5601_1987 , HANGEUL_CHARSET , } ,
	{ UHC , HANGEUL_CHARSET , } ,
	{ JOHAB , JOHAB_CHARSET , } ,

	{ GB2312_80 , GB2312_CHARSET , } ,
	{ GBK , GB2312_CHARSET , } ,
	{ BIG5 , CHINESEBIG5_CHARSET , } ,
	{ HKSCS , DEFAULT_CHARSET , } ,
	{ CNS11643_1992_1 , GB2312_CHARSET , } ,
	{ CNS11643_1992_2 , GB2312_CHARSET , } ,
	{ CNS11643_1992_3 , GB2312_CHARSET , } ,
	{ CNS11643_1992_4 , GB2312_CHARSET , } ,
	{ CNS11643_1992_5 , GB2312_CHARSET , } ,
	{ CNS11643_1992_6 , GB2312_CHARSET , } ,
	{ CNS11643_1992_7 , GB2312_CHARSET , } ,

} ;

static GC  display_gc ;

static int  use_point_size ;


/* --- static functions --- */

static wincs_info_t *
get_wincs_info(
	mkf_charset_t  cs
	)
{
	int  count ;

	for( count = 0 ; count < sizeof( cs_info_table) / sizeof( cs_info_t) ;
		count ++)
	{
		if( cs_info_table[count].cs == cs)
		{
			DWORD  wincs ;

			wincs = cs_info_table[count].wincs ;
			
			for( count = 0 ; count < sizeof( wincs_info_table) / sizeof( wincs_info_t) ;
				count ++)
			{
				if( wincs_info_table[count].cs == wincs)
				{
					return  &wincs_info_table[count] ;
				}
			}

			break ;
		}
	}

#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " not supported cs(%x).\n" , cs) ;
#endif

	return  NULL ;
}

static int
parse_font_name(
	char **  font_family ,
	int *  font_weight ,	/* if weight is not specified in font_name , not changed. */
	int *  is_italic ,	/* if slant is not specified in font_name , not changed. */
	double *  font_size ,	/* if size is not specified in font_name , not changed. */
	u_int *  percent ,	/* if percent is not specified in font_name , not changed. */
	char *  font_name	/* modified by this function. */
	)
{
	char *  p ;
	size_t  len ;
	
	/*
	 * Format.
	 * [Family]( [WEIGHT] [SLANT] [SIZE]:[Percentage])
	 */

	*font_family = font_name ;

	if( ( p = strrchr( font_name , ':')))
	{
		/* Parsing ":[Percentage]" */
		if( kik_str_to_uint( percent , p + 1))
		{
			*p = '\0' ;
		}
	#ifdef  DEBUG
		else
		{
			kik_warn_printf( KIK_DEBUG_TAG
				" Percentage(%s) is illegal.\n" , p + 1) ;
		}
	#endif
	}
	
	/*
	 * Parsing "[Family] [WEIGHT] [SLANT] [SIZE]".
	 * Following is the same as x_font.c:parse_xft_font_name()
	 * except FW_* and is_italic.
	 */

#if  0
	kik_debug_printf( "Parsing %s for [Family] [Weight] [Slant]\n" , *font_family) ;
#endif

	p = kik_str_chop_spaces( *font_family) ;
	len = strlen( p) ;
	while( len > 0)
	{
		size_t  step = 0 ;
		
		if( *p == ' ')
		{
			char *  orig_p ;


			orig_p = p ;
			do
			{
				p ++ ;
				len -- ;
			}
			while( *p == ' ') ;

			if( len == 0)
			{
				*orig_p = '\0' ;
				
				break ;
			}
			else
			{
				int  count ;
				struct
				{
					char *  style ;
					int  weight ;
					int  is_italic ;

				} styles[] =
				{
					/*
					 * Portable styles.
					 */
					/* slant */
					{ "italic" , 0 , 1 , } ,
					/* weight */
					{ "bold" , FW_BOLD , 0 , } ,

					/*
					 * Hack for styles which can be returned by
					 * gtk_font_selection_dialog_get_font_name().
					 */
					/* slant */
					{ "oblique" ,	/* XXX This style is ignored. */
						0 , 0 , } ,
					/* weight */
					{ "light" , /* e.g. "Bookman Old Style Light" */
						FW_LIGHT , 0 , } ,
					{ "semi-bold" , FW_SEMIBOLD , 0 , } ,
					{ "heavy" , /* e.g. "Arial Black Heavy" */
						FW_HEAVY , 0 , }	,
					/* other */
					{ "semi-condensed" , /* XXX This style is ignored. */
						0 , 0 , } ,
				} ;

				for( count = 0 ; count < sizeof(styles) / sizeof(styles[0]) ;
					count ++)
				{
					size_t  len_v ;

					len_v = strlen( styles[count].style) ;
					
					/* XXX strncasecmp is not portable? */
					if( len >= len_v &&
					    strncasecmp( p , styles[count].style , len_v) == 0)
					{
						*orig_p = '\0' ;
						step = len_v ;
						if( styles[count].weight)
						{
							*font_weight = styles[count].weight ;
						}
						else if( styles[count].is_italic)
						{
							*is_italic = 1 ;
						}

						goto  next_char ;
					}
				}

				if( *p != '0' || /* In case of "DevLys 010" font family. */
				    *(p + 1) == '\0')	/* "MS Gothic 0" => "MS Gothic" + "0" */
				{
					char *  end ;
					double  size ;

					size = strtod( p , &end) ;
					if( *end == '\0')
					{
						/* p has no more parameters. */

						*orig_p = '\0' ;
						if( size > 0)
						{
							*font_size = size ;
						}

						break ;
					}
				}

				step = 1 ;
			}
		}
		else
		{
			step = 1 ;
		}

next_char:
		p += step ;
		len -= step ;
	}
	
	return  1 ;
}

static u_int
calculate_char_width(
	x_font_t *  font ,
	u_int32_t  ch ,
	mkf_charset_t  cs
	)
{
	SIZE  sz ;

	if( ! display_gc)
	{
		/*
		 * Cached as far as x_caculate_char_width is called.
		 * display_gc is deleted in x_font_new or x_font_delete.
		 */
		display_gc = CreateIC( "Display" , NULL , NULL , NULL) ;
	}

	SelectObject( display_gc , font->fid) ;

	if( cs != US_ASCII && ! IS_ISCII(cs))
	{
		u_int32_t  ucs4_code ;
		u_char  utf16[4] ;
		int  len ;

		if( cs == ISO10646_UCS4_1)
		{
			ucs4_code = ch ;
		}
		else
		{
			mkf_char_t  non_ucs ;
			mkf_char_t  ucs4 ;

			non_ucs.size = CS_SIZE(cs) ;
			non_ucs.property = 0 ;
			non_ucs.cs = cs ;
			mkf_int_to_bytes( non_ucs.ch , non_ucs.size , ch) ;

			if( ml_is_msb_set( cs))
			{
				u_int  count ;

				for( count = 0 ; count < non_ucs.size ; count ++)
				{
					non_ucs.ch[count] &= 0x7f ;
				}
			}

			if( mkf_map_to_ucs4( &ucs4 , &non_ucs))
			{
				ucs4_code = mkf_bytes_to_int( ucs4.ch , 4) ;
			}
			else
			{
				return  0 ;
			}
		}

		len = x_convert_ucs4_to_utf16( utf16 , ucs4_code) / 2 ;

		if( ( font->use_ot_layout /* && font->ot_font */) ?
		    ! GetTextExtentPointIW( display_gc , utf16 , len , &sz) :
		    ! GetTextExtentPoint32W( display_gc , utf16 , len , &sz))
		{
			return  0 ;
		}
	}
	else
	{
		u_char  c ;

		c = ch ;
		if( ! GetTextExtentPoint32A( display_gc , &c , 1 , &sz))
		{
			return  0 ;
		}
	}

	return  sz.cx ;
}


/* --- global functions --- */

int
x_compose_dec_special_font(void)
{
	/* Do nothing for now in win32. */
	return  0 ;
}


x_font_t *
x_font_new(
	Display *  display ,
	ml_font_t  id ,
	int  size_attr ,
	x_type_engine_t  type_engine ,
	x_font_present_t  font_present ,
	const char *  fontname ,
	u_int  fontsize ,
	u_int  col_width ,
	int  use_medium_for_bold ,
	u_int  letter_space	/* Ignored for now. */
	)
{
	x_font_t *  font ;
	wincs_info_t *  wincsinfo ;
	char *  font_family ;
	int  weight ;
	int  is_italic ;
	double  fontsize_d ;
	u_int  percent ;
	
	if( type_engine != TYPE_XCORE || ( font = calloc( 1 , sizeof( x_font_t))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif

		return  NULL ;
	}

	font->display = display ;
	font->id = id ;

	if( font->id & FONT_FULLWIDTH)
	{
		font->cols = 2 ;
	}
	else
	{
		font->cols = 1 ;
	}

	if( font_present & FONT_VAR_WIDTH)
	{
		font->is_var_col_width = 1 ;
	}
	else if( IS_ISCII(FONT_CS(font->id)))
	{
		/*
		 * For exampe, 'W' width and 'l' width of OR-TTSarala font for ISCII_ORIYA
		 * are the same by chance, though it is actually a proportional font.
		 */
		font->is_var_col_width = font->is_proportional = 1 ;
	}

	if( font_present & FONT_VERTICAL)
	{
		font->is_vertical = 1 ;
	}

	if( ( wincsinfo = get_wincs_info( FONT_CS(font->id))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " charset(0x%.2x) is not supported.\n" ,
			FONT_CS(font->id)) ;
	#endif

		free( font) ;
		
		return  NULL ;
	}

	if( font->id & FONT_BOLD)
	{
	#if  0
		weight = FW_BOLD ;
	#else
		/*
		 * XXX
		 * Width of bold font is not necessarily the same as
		 * that of normal font in win32.
		 * So ignore weight and if font->id is bold use double-
		 * drawing method for now.
		 */
		weight = FW_DONTCARE ;
	#endif
	}
	else
	{
		weight = FW_MEDIUM ;
	}

	if( font->id & FONT_ITALIC)
	{
		is_italic = TRUE ;
	}
	else
	{
		is_italic = FALSE ;
	}

	if( size_attr)
	{
		if( size_attr >= DOUBLE_HEIGHT_TOP)
		{
			fontsize *= 2 ;
		}

		col_width *= 2 ;
	}

	font_family = NULL ;
	percent = 0 ;
	fontsize_d = (double)fontsize ;

	if( FONT_CS(font->id) == DEC_SPECIAL)
	{
		font_family = "Tera Special" ;
	}
	else if( fontname)
	{
		char *  p ;

		if( ( p = kik_str_alloca_dup( fontname)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
		#endif

			free( font) ;
			
			return  NULL ;
		}
		
		parse_font_name( &font_family , &weight , &is_italic , &fontsize_d , &percent , p) ;
	}
	else
	{
		/* Default font */
		font_family = "Courier New" ;
	}

	if( ! display_gc)
	{
		display_gc = CreateIC( "Display" , NULL , NULL , NULL) ;
	}

  	font->fid = CreateFont( use_point_size ?	/* Height */
				-MulDiv( (int)fontsize_d ,
					GetDeviceCaps( display_gc , LOGPIXELSY) , 72) :
				(int)fontsize_d ,
			#if  0
				col_width ? (font->is_vertical ? col_width / 2 : col_width) :
					(int)fontsize_d / 2 ,
			#else
				size_attr == DOUBLE_WIDTH ?
					(use_point_size ?
						-MulDiv( (int)fontsize_d ,
						GetDeviceCaps( display_gc , LOGPIXELSY) , 72) :
						(int)fontsize_d) :
					0 ,		/* Width (0=auto) */
			#endif
				0 ,			/* text angle */
				0 ,			/* char angle */
				weight ,		/* weight */
				is_italic ,		/* italic */
				FALSE ,			/* underline */
				FALSE ,			/* eraseline */
				wincsinfo->cs ,
				OUT_DEFAULT_PRECIS ,
				CLIP_DEFAULT_PRECIS ,
				(font_present & FONT_AA) ? ANTIALIASED_QUALITY : PROOF_QUALITY ,
				FIXED_PITCH | FF_MODERN ,
				font_family ) ;

	if( ! font->fid)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " CreateFont failed.\n") ;
		free( font) ;
	#endif

		return  NULL ;
	}
	else
	{
		TEXTMETRIC  tm ;
		SIZE  w_sz ;
		SIZE  l_sz ;

		SelectObject( display_gc , font->fid) ;

		GetTextMetrics( display_gc , &tm) ;

		/*
		 * Note that fixed pitch font containing both Hankaku and Zenkaku characters like
		 * MS Gothic is regarded as VARIABLE_PITCH. (tm.tmPitchAndFamily)
		 * So "w" and "l" width is compared to check if font is proportional or not.
		 */
		GetTextExtentPoint32A( display_gc , "w" , 1 , &w_sz) ;
		GetTextExtentPoint32A( display_gc , "l" , 1 , &l_sz) ;

	#if  0
		kik_debug_printf(
		"Family %s Size %d CS %x => AveCharWidth %d MaxCharWidth %d 'w' width %d 'l' width %d Height %d Ascent %d ExLeading %d InLeading %d Pitch&Family %d Weight %d\n" ,
		font_family , fontsize , wincsinfo->cs ,
		tm.tmAveCharWidth , tm.tmMaxCharWidth , w_sz.cx , l_sz.cx ,
		tm.tmHeight , tm.tmAscent , tm.tmExternalLeading , tm.tmInternalLeading ,
		tm.tmPitchAndFamily , tm.tmWeight) ;
	#endif

		if( w_sz.cx != l_sz.cx)
		{
			font->is_proportional = 1 ;
			font->width = tm.tmAveCharWidth * font->cols ;
		}
		else
		{
			font->width = w_sz.cx * font->cols ;
		}

		font->height = tm.tmHeight ;
		font->ascent = tm.tmAscent ;

		if( ( font->id & FONT_BOLD) && tm.tmWeight <= FW_MEDIUM)
		{
			font->double_draw_gap = 1 ;
		}
	}

	/*
	 * Following processing is same as x_font.c:set_xfont()
	 */

	if( col_width == 0)
	{
		/* standard(usascii) font */

		if( percent > 0)
		{
			u_int  ch_width ;

			if( font->is_vertical)
			{
				/*
				 * !! Notice !!
				 * The width of full and half character font is the same.
				 */
				ch_width = fontsize * percent / 100 ;
			}
			else
			{
				ch_width = fontsize * percent / 200 ;
			}

			if( ! font->is_var_col_width && font->width != ch_width)
			{
				font->is_proportional = 1 ;

				if( font->width < ch_width)
				{
					font->x_off = (ch_width - font->width) / 2 ;
				}

				font->width = ch_width ;
			}
		}
		else if( font->is_vertical)
		{
			/*
			 * !! Notice !!
			 * The width of full and half character font is the same.
			 */

			font->is_proportional = 1 ;
			font->x_off = font->width / 2 ;
			font->width *= 2 ;
		}

		/* letter_space is ignored in variable column width mode. */
		if( ! font->is_var_col_width && letter_space > 0)
		{
			font->is_proportional = 1 ;
			font->width += letter_space ;
			font->x_off += (letter_space / 2) ;
		}
	}
	else
	{
		/* not a standard(usascii) font */

		/*
		 * XXX hack
		 * forcibly conforming non standard font width to standard font width.
		 */

		if( font->is_vertical)
		{
			/*
			 * !! Notice !!
			 * The width of full and half character font is the same.
			 */

			if( font->width != col_width)
			{
				font->is_proportional = 1 ;

				/* is_var_col_width is always false if is_vertical is true. */
				if( /* ! font->is_var_col_width && */ font->width < col_width)
				{
					font->x_off = (col_width - font->width) / 2 ;
				}

				font->width = col_width ;
			}
		}
		else
		{
			if( font->width != col_width * font->cols)
			{
				kik_msg_printf( "Font width(%d) is not matched with "
					"standard width(%d).\n" ,
					font->width , col_width * font->cols) ;

				font->is_proportional = 1 ;

				if( ! font->is_var_col_width &&
				    font->width < col_width * font->cols)
				{
					font->x_off = (col_width * font->cols - font->width) / 2 ;
				}

				font->width = col_width * font->cols ;
			}
		}
	}

	font->size_attr = size_attr ;


	if( wincsinfo->cs != ANSI_CHARSET && wincsinfo->cs != SYMBOL_CHARSET &&
	    FONT_CS(font->id) != ISO10646_UCS4_1)
	{
		if( ! ( font->conv = ml_conv_new( wincsinfo->encoding)))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ml_conv_new(font id %x) failed.\n" ,
				font->id) ;
		#endif
		}
	}

	if( font->is_proportional && ! font->is_var_col_width)
	{
		kik_msg_printf(
			"Characters (cs %d) are drawn *one by one* to arrange column width.\n" ,
			FONT_CS(font->id)) ;

	}

	return  font ;
}

int
x_font_delete(
	x_font_t *  font
	)
{
#ifdef  USE_OT_LAYOUT
	if( font->ot_font)
	{
		otl_close( font->ot_font) ;
	}
#endif

	if( font->fid)
	{
		DeleteObject( font->fid) ;
	}
	
	if( font->conv)
	{
		font->conv->delete( font->conv) ;
	}

	free( font) ;

	if( display_gc)
	{
		DeleteDC( display_gc) ;
		display_gc = None ;
	}

	return  1 ;
}

int
x_change_font_cols(
	x_font_t *  font ,
	u_int  cols	/* 0 means default value */
	)
{
	if( cols == 0)
	{
		if( font->id & FONT_FULLWIDTH)
		{
			font->cols = 2 ;
		}
		else
		{
			font->cols = 1 ;
		}
	}
	else
	{
		font->cols = cols ;
	}

	return  1 ;
}

#ifdef  USE_OT_LAYOUT
int
x_font_has_ot_layout_table(
	x_font_t *  font
	)
{
	if( ! font->ot_font)
	{
		u_char  buf[4] ;
		void *  font_data ;
		u_int  size ;
		int  is_ttc ;

		if( font->ot_font_not_found)
		{
			return  0 ;
		}

		if( ! display_gc)
		{
			/*
			 * Cached as far as x_caculate_char_width is called.
			 * display_gc is deleted in x_font_new or x_font_delete.
			 */
			display_gc = CreateIC( "Display" , NULL , NULL , NULL) ;
		}

		SelectObject( display_gc , font->fid) ;

#define  TTC_TAG  ('t' << 0) + ('t' << 8) + ('c' << 16) + ('f' << 24)

		if( GetFontData( display_gc , TTC_TAG , 0 , &buf , 1) == 1)
		{
			is_ttc = 1 ;
			size = GetFontData( display_gc , TTC_TAG , 0 , NULL , 0) ;
		}
		else
		{
			is_ttc = 0 ;
			size = GetFontData( display_gc , 0 , 0 , NULL , 0) ;
		}

		if( ! ( font_data = malloc( size)))
		{
			font->ot_font = NULL ;
		}
		else
		{
			if( is_ttc)
			{
				GetFontData( display_gc , TTC_TAG , 0 , font_data , size) ;
			}
			else
			{
				GetFontData( display_gc , 0 , 0 , font_data , size) ;
			}

			font->ot_font = otl_open( font_data , size) ;

			free( font_data) ;
		}

		if( ! font->ot_font)
		{
			font->ot_font_not_found = 1 ;

			return  0 ;
		}
	}

	return  1 ;
}

u_int
x_convert_text_to_glyphs(
	x_font_t *  font ,
	u_int32_t *  shaped ,
	u_int  shaped_len ,
	int8_t *  offsets ,
	u_int8_t *  widths ,
	u_int32_t *  cmapped ,
	u_int32_t *  src ,
	u_int  src_len ,
	const char *  script ,
	const char *  features
	)
{
	return  otl_convert_text_to_glyphs( font->ot_font , shaped , shaped_len , offsets ,
			widths , cmapped , src , src_len , script , features) ;
}
#endif	/* USE_OT_LAYOUT */

u_int
x_calculate_char_width(
	x_font_t *  font ,
	u_int32_t  ch ,
	mkf_charset_t  cs ,
	int *  draw_alone
	)
{
	if( draw_alone)
	{
		*draw_alone = 0 ;
	}

	if( font->is_proportional)
	{
		if( font->is_var_col_width)
		{
			u_int  width ;

			if( ( width = calculate_char_width( font , ch , cs)) == 0)
			{
				goto  fixed_col_width ;
			}

			return  width ;
		}

	fixed_col_width:
		if( draw_alone)
		{
			*draw_alone = 1 ;
		}
	}
	else if( draw_alone && cs == ISO10646_UCS4_1)
	{
		if( mkf_get_ucs_property( ch) & MKF_AWIDTH)
		{
			if( calculate_char_width( font , ch , cs) != font->width)
			{
				*draw_alone = 1 ;
			}
		}
	}

	return  font->width ;
}

void
x_font_use_point_size(
	int  use
	)
{
	use_point_size = use ;
}

/* Return written size */
size_t
x_convert_ucs4_to_utf16(
	u_char *  dst ,	/* 4 bytes. Little endian. */
	u_int32_t  src
	)
{
	if( src < 0x10000)
	{
		dst[1] = (src >> 8) & 0xff ;
		dst[0] = src & 0xff ;
	
		return  2 ;
	}
	else if( src < 0x110000)
	{
		/* surrogate pair */

		u_char  c ;

		src -= 0x10000 ;
		c = (u_char)( src / (0x100 * 0x400)) ;
		src -= (c * 0x100 * 0x400) ;
		dst[1] = c + 0xd8 ;

		c = (u_char)( src / 0x400) ;
		src -= (c * 0x400) ;
		dst[0] = c ;

		c = (u_char)( src / 0x100) ;
		src -= (c * 0x100) ;
		dst[3] = c + 0xdc ;
		dst[2] = (u_char)src ;

		return  4 ;
	}

	return  0 ;
}


#ifdef  DEBUG

int
x_font_dump(
	x_font_t *  font
	)
{
	kik_msg_printf( "  id %x: Font %p" , font->id , font->fid) ;

	if( font->is_proportional)
	{
		kik_msg_printf( " (proportional)") ;
	}
	kik_msg_printf( "\n") ;

	return  1 ;
}

#endif
