/*
 *	$Id$
 */

#include  "x_font.h"

#include  <stdio.h>
#include  <string.h>		/* memset/strncasecmp */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>	/* kik_snprintf */
#include  <kiklib/kik_mem.h>	/* alloca */
#include  <kiklib/kik_str.h>	/* kik_str_sep/kik_str_to_int */
#include  <kiklib/kik_util.h>	/* DIGIT_STR_LEN/K_MIN */
#include  <kiklib/kik_locale.h>	/* kik_get_lang() */

#include  "ml_char_encoding.h"	/* x_convert_to_xft_ucs4 */


#define  FOREACH_FONT_ENCODINGS(csinfo,font_encoding_p) \
	for( (font_encoding_p) = &csinfo->encoding_names[0] ; *(font_encoding_p) ; (font_encoding_p) ++)

#if  0
#define  __DEBUG
#endif

#if  0
#define  ENABLE_PROPORTIONAL
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
	{ ISCII_ROMAN , DEFAULT_CHARSET , } ,
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
				float  size_f ;
				
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
				
				if( sscanf( p , "%f" , &size_f) == 1)
				{
					/* If matched with %f, p has no more parameters. */

					*orig_p = '\0' ;
					*font_size = size_f ;

					break ;
				}
				else
				{
					step = 1 ;
				}
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
	
	if( ( font = malloc( sizeof( x_font_t))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif

		return  NULL ;
	}

	font->display = display ;
	font->id = id ;

	if( font->id & FONT_BIWIDTH)
	{
		font->cols = 2 ;
	}
	else
	{
		font->cols = 1 ;
	}

#if  0
	if( font_present & FONT_VAR_WIDTH)
	{
		font->is_var_col_width = 1 ;
	}
	else
#endif
	{
		font->is_var_col_width = 0 ;
	}

	if( font_present & FONT_VERTICAL)
	{
		font->is_vertical = 1 ;
	}
	else
	{
		font->is_vertical = 0 ;
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

	is_italic = FALSE ;
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

  	font->fid = CreateFont( (int)fontsize_d ,	/* Height */
			#if  0
				col_width ? (font->is_vertical ? col_width / 2 : col_width) :
					(int)fontsize_d / 2 ,
			#else
				0 ,			/* Width (0=auto) */
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
		GC  gc ;
		TEXTMETRIC  tm ;
	#if  0
		Window  win ;

		win = CreateWindow( "mlterm", "font test", WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			NULL, NULL, font->display->hinst, NULL) ;
		gc = GetDC( win) ;
		SelectObject( gc, font->fid) ;
		GetTextMetrics( gc, &tm) ;
		ReleaseDC( win, gc) ;
		DestroyWindow( win) ;
	#else
		gc = CreateIC( "Display", NULL, NULL, NULL) ;
		SelectObject( gc, font->fid) ;
		GetTextMetrics( gc, &tm) ;
		DeleteDC( gc) ;
	#endif
	#if  0
		kik_debug_printf(
		"Family %s Size %d CS %x => AveCharWidth %d MaxCharWidth %d Height %d Ascent %d ExLeading %d InLeading %d Pitch&Family %d Weight %d\n" ,
		font_family , fontsize , wincsinfo->cs ,
		tm.tmAveCharWidth , tm.tmMaxCharWidth , tm.tmHeight , tm.tmAscent ,
		tm.tmExternalLeading , tm.tmInternalLeading ,
		tm.tmPitchAndFamily , tm.tmWeight) ;
	#endif

	#ifdef  ENABLE_PROPORTIONAL
		if( font->cols == 2)
		{
			/*
			 * XXX
			 * MaxCharWidth of MS Gothic is AveCharWidth * 2 + 1.
			 *                                               ^^^
			 */
			font->width = tm.tmMaxCharWidth ;
		}
		else
		{
			font->width = tm.tmAveCharWidth ;
		}
	#else
		font->width = tm.tmAveCharWidth * font->cols ;
	#endif
		font->height = tm.tmHeight ;
		font->height_to_baseline = tm.tmAscent ;

	#ifdef  ENABLE_PROPORTIONAL
		/*
		 * XXX
		 * Note that fixed pitch font containing both Hankaku and Zenkaku characters like
		 * MS Gothic is regarded as VARIABLE_PITCH.
		 */
		if( tm.tmPitchAndFamily & VARIABLE_PITCH)
		{
			font->is_proportional = 1 ;
		}
		else
	#endif
		{
			font->is_proportional = 0 ;
		}

		if( ( font->id & FONT_BOLD) && tm.tmWeight <= FW_MEDIUM)
		{
			font->is_double_drawing = 1 ;
		}
		else
		{
			font->is_double_drawing = 0 ;
		}
	}

	/*
	 * Following processing is same as x_font.c:set_xfont()
	 */
	 
	font->x_off = 0 ;

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

			if( font->width != ch_width)
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

				if( font->width < col_width)
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

				if( font->width < col_width * font->cols)
				{
					font->x_off = (col_width * font->cols - font->width) / 2 ;
				}

				font->width = col_width * font->cols ;
			}
		}
	}


	if( wincsinfo->cs == ANSI_CHARSET || wincsinfo->cs == SYMBOL_CHARSET ||
	    FONT_CS(font->id) == ISO10646_UCS4_1)
	{
		font->conv = NULL ;
	}
	else
	{
		if( ! ( font->conv = ml_conv_new( wincsinfo->encoding)))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ml_conv_new(font id %x) failed.\n" ,
				font->id) ;
		#endif
		}
	}
	
	font->decsp_font = NULL ;

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
	if( font->fid)
	{
		DeleteObject(font->fid) ;
	}
	
	if( font->conv)
	{
		font->conv->delete( font->conv) ;
	}

#ifndef  USE_WIN32GUI
	if( font->decsp_font)
	{
		x_decsp_font_delete( font->decsp_font , font->display) ;
		font->decsp_font = NULL ;
	}
#endif

	free( font) ;

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
		if( font->id & FONT_BIWIDTH)
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

u_int
x_calculate_char_width(
	x_font_t *  font ,
	const u_char *  ch ,
	size_t  len ,
	mkf_charset_t  cs
	)
{
#if  0
	if( font->is_var_col_width)
	{
		/* XXX */
		
		return  font->width ;
	}
	else
#endif
	{
		return  font->width ;
	}
}

char **
x_font_get_encoding_names(
	mkf_charset_t  cs
	)
{
	static char *  csnames[] = { "iso8859-1" } ;	/* dummy */
	
	return  csnames ;
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
