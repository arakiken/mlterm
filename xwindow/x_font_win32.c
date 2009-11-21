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
	{ ANSI_CHARSET , ML_ISO8859_1 , } ,
	{ RUSSIAN_CHARSET , ML_CP1251 , } ,
	{ GREEK_CHARSET , ML_CP1253 , } ,
	{ TURKISH_CHARSET , ML_CP1254 , } ,
	{ BALTIC_CHARSET , ML_CP1257 , } ,
	{ HEBREW_CHARSET , ML_CP1255 , } ,
	{ ARABIC_CHARSET , ML_CP1256 , } ,
	{ SHIFTJIS_CHARSET , ML_SJIS , } ,
	{ HANGEUL_CHARSET , ML_UHC , } ,
	{ GB2312_CHARSET , ML_EUCCN , } ,
	{ CHINESEBIG5_CHARSET , ML_BIG5 } ,
	{ JOHAB_CHARSET , ML_JOHAB , } ,
	{ THAI_CHARSET , ML_TIS620 , } ,
	{ EASTEUROPE_CHARSET , ML_ISO8859_3 , } ,
	{ MAC_CHARSET , ML_UNKNOWN_ENCODING , } ,
} ;

static cs_info_t  cs_info_table[] =
{
	{ DEC_SPECIAL , DEFAULT_CHARSET , } ,
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
	{ ISCII , DEFAULT_CHARSET , } ,
	{ TIS620_2533 , THAI_CHARSET , } ,
	{ ISO8859_13_R , DEFAULT_CHARSET , } ,
	{ ISO8859_14_R , DEFAULT_CHARSET , } ,
	{ ISO8859_15_R , DEFAULT_CHARSET , } ,
	{ TCVN5712_3_1993 , DEFAULT_CHARSET , } ,

	{ VISCII , DEFAULT_CHARSET , } ,
	{ KOI8_R , RUSSIAN_CHARSET , } ,
	{ KOI8_U , RUSSIAN_CHARSET , } ,
	{ KOI8_T , RUSSIAN_CHARSET , } ,
	{ GEORGIAN_PS , DEFAULT_CHARSET , } ,
	{ CP1251 , RUSSIAN_CHARSET , } ,
	{ CP1255 , HEBREW_CHARSET , } ,
	{ CP1253 , GREEK_CHARSET , } ,
	{ CP1254 , TURKISH_CHARSET , } ,
	{ CP1256 , ARABIC_CHARSET , } ,
	{ CP1257 , BALTIC_CHARSET , } ,

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
	{ ISO10646_UCS4_1 , DEFAULT_CHARSET , } ,

} ;

static int  compose_dec_special_font ;


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
	 */

#if  0
	kik_debug_printf( "Parsing %s for [Family] [Weight] [Slant]\n" , *font_family) ;
#endif

	p = kik_str_chop_spaces( *font_family) ;
	len = strlen( p) ;
	while( len > 0)
	{
		if( *p == ' ')
		{
			char *  orig_p ;
			char  italic[] = "italic" ;
			char  bold[] = "bold" ;
		#if  0
			char  light[] = "light" ;
		#endif
			float  size_f ;

			orig_p = p ;
			do
			{
				p ++ ;
				len -- ;
			}
			while( *p == ' ') ;

			/* XXX strncasecmp is not portable? */

			if( len == 0)
			{
				*orig_p = '\0' ;
				break ;
			}
			else if( strncasecmp( p , italic , K_MIN(len,sizeof(italic) - 1)) == 0)
			{
				*orig_p = '\0' ;
				*is_italic = TRUE ;
				p += (sizeof(italic) - 1) ;
				len -= (sizeof(italic) - 1) ;
			}
			else if( strncasecmp( p , bold , K_MIN(len,(sizeof(bold) - 1))) == 0)
			{
				*orig_p = '\0' ;
				*font_weight = FW_BOLD ;
				p += (sizeof(bold) - 1) ;
				len -= (sizeof(bold) - 1) ;
			}
		#if  0
			else if( strncasecmp( p , light , K_MIN(len,(sizeof(light) - 1))) == 0)
			{
				*orig_p = '\0' ;
				*font_weight = FW_LIGHT ;
				p += (sizeof(light) - 1) ;
				len -= (sizeof(light) - 1) ;
			}
		#endif
			else if( sscanf( p , "%f" , &size_f) == 1)
			{
				/* If matched with %f, p has no more parameters. */

				*orig_p = '\0' ;
				*font_size = size_f ;
				
				break ;
			}
			else
			{
				p ++ ;
				len -- ;
			}
		}
		else
		{
			p ++ ;
			len -- ;
		}
	}
	
	return  1 ;
}


/* --- global functions --- */

int
x_compose_dec_special_font(void)
{
#ifdef  USE_WIN32GUI
	return  0 ;
#else
	compose_dec_special_font = 1 ;

	return  1 ;
#endif
}


x_font_t *
x_font_new(
	Display *  display ,
	ml_font_t  id ,
	x_type_engine_t  type_engine ,
	x_font_present_t  font_present ,
	char *  fontname ,
	u_int  fontsize ,
	u_int  col_width ,
	int  use_medium_for_bold
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

	if( (font->id & FONT_BIWIDTH) || IS_BIWIDTH_CS(FONT_CS(font->id)))
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
	else
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
		weight = FW_BOLD ;
	}
	else
	{
		weight = FW_MEDIUM ;
	}

	is_italic = FALSE ;
	font_family = NULL ;
	percent = 0 ;
	fontsize_d = (double)fontsize ;
	
	if( fontname)
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
				0 ,			/* Width (0=auto) */
				0 ,			/* text angle */
				0 ,			/* char angle */
				weight ,		/* weight */
				is_italic ,		/* italic */
				FALSE ,			/* underline */
				FALSE ,			/* eraseline */
				wincsinfo->cs ,
				OUT_DEFAULT_PRECIS ,
				CLIP_DEFAULT_PRECIS ,
				PROOF_QUALITY ,
				FIXED_PITCH | FF_MODERN ,
				font_family ) ;

	if( ! font->fid)
	{
		kik_warn_printf( KIK_DEBUG_TAG " CreateFont failed.\n") ;
		free( font) ;
	
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
		font->width = tm.tmAveCharWidth * font->cols ;
		font->height = tm.tmHeight ;
		font->height_to_baseline = tm.tmAscent ;
		ReleaseDC( win, gc) ;
		DestroyWindow( win) ;
	#else
		gc = CreateIC( "Display", NULL, NULL, NULL) ;
		SelectObject( gc, font->fid) ;
		GetTextMetrics( gc, &tm) ;
		font->width = tm.tmAveCharWidth * font->cols ;
		font->height = tm.tmHeight ;
		font->height_to_baseline = tm.tmAscent ;
		DeleteDC( gc) ;
	#endif
	#if  0
		kik_debug_printf(
		"AveCharWidth %d MaxCharWidth %d Height %d Ascent %d ExLeading %d InLeading %d\n" ,
		tm.tmAveCharWidth, tm.tmMaxCharWidth, tm.tmHeight, tm.tmAscent,
		tm.tmExternalLeading, tm.tmInternalLeading) ;
	#endif
	}

	if( wincsinfo->cs == ANSI_CHARSET)
	{
		font->conv = NULL ;
	}
	else if( ( font->conv = ml_conv_new( wincsinfo->encoding)) == NULL)
	{
		kik_warn_printf( KIK_DEBUG_TAG " ml_conv_new failed.\n") ;
		
		DeleteObject( font->fid) ;
		free( font) ;

		return  NULL ;
	}
	
	font->decsp_font = NULL ;

	font->x_off = 0 ;
	font->is_proportional = 0 ;
	
	font->is_double_drawing = 0 ;

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
		if( (font->id & FONT_BIWIDTH) || IS_BIWIDTH_CS(FONT_CS(font->id)))
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
	u_char *  ch ,
	size_t  len ,
	mkf_charset_t  cs
	)
{
	return  font->width ;
}

char **
x_font_get_cs_names(
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
