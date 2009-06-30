/*
 *	$Id$
 */

#include  "x_font.h"

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


#ifndef  USE_WIN32GUI

static int
set_decsp_font(
	x_font_t *  font
	)
{
	/*
	 * freeing font->xfont or font->xft_font
	 */
#ifdef  USE_TYPE_XFT
	if( font->xft_font)
	{
		XftFontClose( font->display , font->xft_font) ;
		font->xft_font = NULL ;
	}
#endif
#ifdef  USE_TYPE_XCORE
	if( font->xfont)
	{
		XFreeFont( font->display , font->xfont) ;
		font->xfont = NULL ;
	}
#endif

	if( ( font->decsp_font = x_decsp_font_new( font->display , font->width , font->height ,
					font->height_to_baseline)) == NULL)
	{
		return  0 ;
	}

	/* decsp_font is impossible to draw double with. */
	font->is_double_drawing = 0 ;

	/* decsp_font is always fixed pitch. */
	font->is_proportional = 0 ;

	return  1 ;
}

#ifdef  USE_TYPE_XFT

static u_int
xft_calculate_char_width(
	Display *  display ,
	XftFont *  xfont ,
	u_char *  ch ,		/* US-ASCII or Unicode */
	size_t  len
	)
{
	XGlyphInfo  extents ;

	if( len == 1)
	{
		XftTextExtents8( display , xfont , ch , 1 , &extents) ;
	}
	else if( len == 2)
	{
		XftChar16  xch ;

		xch = ((ch[0] << 8) & 0xff00) | (ch[1] & 0xff) ;

		XftTextExtents16( display , xfont , &xch , 1 , &extents) ;
	}
	else if( len == 4)
	{
		XftChar32  xch ;

		xch = ((ch[0] << 24) & 0xff000000) | ((ch[1] << 16) & 0xff0000) |
			((ch[2] << 8) & 0xff00) | (ch[3] & 0xff) ;

		XftTextExtents32( display , xfont , &xch , 1 , &extents) ;
	}
	else
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " char size %d is too large.\n" , len) ;
	#endif

		return  0 ;
	}

	return  extents.xOff ;
}

static int
parse_xft_font_name(
	char **  font_family ,
	char **  font_encoding ,
	char **  percent ,
	char *  font_name
	)
{
	/*
	 * XftFont format.
	 * [Font Family]-[Font Encoding](:[Percentage])
	 */

	if( ( *font_family = font_name))
	{
		/*
		 * It seems that XftFont format allows hyphens to be escaped.
		 * (e.g. Foo\-Bold-iso10646-1)
		 */
		int  i ;
		int  j ;
		char *  s ;

		s = font_name ;
		for( i = 0 , j = 0 ; s[i] && s[i] != '-' ; i ++ , j ++)
		{
			/* escaped? */
			if( s[i] == '\\' && s[i + 1])
			{
				/* skip backslash */
				i ++ ;
			}
			s[j] = s[i] ;
		}
		if( !s[i])
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " encoding part is missing(%s).\n" ,
				font_name) ;
		#endif
			return  0 ;
		}
		/* replace delimiter */
		s[j] = '\0' ;
		/* move forward to the next token */
		font_name = s + i + 1 ;
	}

	if( *font_family == NULL || font_name == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " illegal true type font name(%s).\n" ,
			font_name) ;
	#endif

		return  0 ;
	}

	if( ( *font_encoding = kik_str_sep( &font_name , ":")) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " illegal true type font name(%s).\n" ,
			font_name) ;
	#endif

		return  0 ;
	}

	/* may be NULL */
	*percent = font_name ;

	return  1 ;
}

static u_int
get_xft_col_width(
	x_font_t *  font ,
	char *  family ,
	u_int  fontsize
	)
{
	XftFont *  xfont ;

	/*
	 * XXX
	 * DefaultScreen() should not be used , but ...
	 */
	if( ( xfont = XftFontOpen( font->display , DefaultScreen( font->display) ,
		XFT_FAMILY , XftTypeString , family ,
		XFT_PIXEL_SIZE , XftTypeDouble , (double)fontsize ,
		XFT_ENCODING , XftTypeString , "iso8859-1" ,
		XFT_SPACING , XftTypeInteger , XFT_PROPORTIONAL , NULL)))
	{
		u_int  w_width ;

		w_width = xft_calculate_char_width( font->display , xfont , "W" , 1) ;

		XftFontClose( font->display , xfont) ;

		if( w_width > 0)
		{
			return  w_width ;
		}
	}

	/* XXX this may be inaccurate. */
	return  fontsize / 2 ;
}

static int
set_xft_font(
	x_font_t *  font ,
	char *  fontname ,
	u_int  fontsize ,
	u_int  col_width ,	/* if usascii font wants to be set , 0 will be set. */
	int  use_medium_for_bold ,
	int  is_aa
	)
{
	int  weight ;
	cs_info_t *  csinfo ;
	u_int  ch_width ;
	XftFont *  xfont ;
	char **  font_encoding_p ;

	if( ( csinfo = get_cs_info( FONT_CS(font->id))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " charset(0x%.2x) is not supported.\n" ,
			FONT_CS(font->id)) ;
	#endif

		return  0 ;
	}

	if( font->id & FONT_BOLD)
	{
		weight = XFT_WEIGHT_BOLD ;
	}
	else
	{
		weight = XFT_WEIGHT_MEDIUM ;
	}

	if( fontname)
	{
		char *  p ;
		char *  font_family ;
		char *  font_encoding ;
		char *  percent_str ;
		u_int  percent ;

		if( ( p = kik_str_alloca_dup( fontname)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
		#endif

			return  0 ;
		}

		if( parse_xft_font_name( &font_family , &font_encoding , &percent_str , p))
		{
			if( col_width == 0)
			{
				/* basic font (e.g. usascii) width */

				if( percent_str == NULL || ! kik_str_to_uint( &percent , percent_str) ||
					percent == 0)
				{
					ch_width = get_xft_col_width( font , font_family , fontsize) ;
				}
				else
				{
					ch_width = (fontsize * font->cols * percent) / 200 ;
				}

				if( font->is_vertical)
				{
					/*
					 * !! Notice !!
					 * The width of full and half character font is the same.
					 */
					ch_width *= 2 ;
				}
			}
			else
			{
				if( font->is_vertical)
				{
					/*
					 * !! Notice !!
					 * The width of full and half character font is the same.
					 */
					ch_width = col_width ;
				}
				else
				{
					ch_width = col_width * font->cols ;
				}
			}

			if( font->is_var_col_width)
			{
				if( ( xfont = XftFontOpen( font->display , DefaultScreen( font->display) ,
						XFT_FAMILY , XftTypeString , font_family ,
						XFT_PIXEL_SIZE , XftTypeDouble , (double)fontsize ,
						XFT_ENCODING , XftTypeString , font_encoding ,
						XFT_WEIGHT , XftTypeInteger , weight ,
						XFT_SPACING , XftTypeInteger , XFT_PROPORTIONAL ,
						XFT_ANTIALIAS , XftTypeBool , is_aa ? True : False ,
						NULL)))
				{
					goto  font_found ;
				}
			}
			else
			{
				if( ( xfont = XftFontOpen( font->display , DefaultScreen( font->display) ,
						XFT_FAMILY , XftTypeString , font_family ,
						XFT_PIXEL_SIZE , XftTypeDouble , (double)fontsize ,
						XFT_ENCODING , XftTypeString , font_encoding ,
						XFT_CHAR_WIDTH , XftTypeInteger , ch_width ,
						XFT_WEIGHT , XftTypeInteger , weight ,
						XFT_SPACING , XftTypeInteger , XFT_CHARCELL ,
						XFT_ANTIALIAS , XftTypeBool , is_aa ? True : False ,
						NULL)))
				{
					goto  font_found ;
				}
			}
		}

		kik_msg_printf( " font %s (for size %d) couln't be loaded.\n" , fontname , fontsize) ;
	}

	if( col_width == 0)
	{
		/* basic font (e.g. usascii) width */
		ch_width = get_xft_col_width( font , "" , fontsize) ;

		/*
		 * !! Notice !!
		 * The width of full and half character font is the same.
		 */
		if( font->is_vertical)
		{
			ch_width *= 2 ;
		}
	}
	else
	{
		if( font->is_vertical)
		{
			/*
			 * !! Notice !!
			 * The width of full and half character font is the same.
			 */
			ch_width = col_width ;
		}
		else
		{
			ch_width = col_width * font->cols ;
		}
	}

	FOREACH_FONT_ENCODINGS(csinfo,font_encoding_p)
	{
		if( font->is_var_col_width)
		{
			if( ( xfont = XftFontOpen( font->display , DefaultScreen( font->display) ,
					XFT_PIXEL_SIZE , XftTypeDouble , (double)fontsize ,
					XFT_ENCODING , XftTypeString , *font_encoding_p ,
					XFT_WEIGHT , XftTypeInteger , weight ,
					XFT_SPACING , XftTypeInteger , XFT_PROPORTIONAL ,
					XFT_ANTIALIAS , XftTypeBool , is_aa ? True : False ,
					NULL)))
			{
				goto  font_found ;
			}
		}
		else
		{
			if( ( xfont = XftFontOpen( font->display , DefaultScreen( font->display) ,
					XFT_PIXEL_SIZE , XftTypeDouble , (double)fontsize ,
					XFT_ENCODING , XftTypeString , *font_encoding_p ,
					XFT_WEIGHT , XftTypeInteger , weight ,
					XFT_CHAR_WIDTH , XftTypeInteger , ch_width ,
					XFT_SPACING , XftTypeInteger , XFT_CHARCELL ,
					XFT_ANTIALIAS , XftTypeBool , is_aa ? True : False ,
					NULL)))
			{
				goto  font_found ;
			}
		}
	}

#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " XftFontOpen(%s) failed.\n" , fontname) ;
#endif

	return  0 ;

font_found:

#ifdef FC_EMBOLDEN /* Synthetic emboldening (fontconfig >= 2.3.0) */
	font->is_double_drawing = 0 ;
#else
	if( weight == XFT_WEIGHT_BOLD &&
		XftPatternGetInteger( xfont->pattern , XFT_WEIGHT , 0 , &weight) == XftResultMatch &&
		weight != XFT_WEIGHT_BOLD)
	{
		font->is_double_drawing = 1 ;
	}
	else
	{
		font->is_double_drawing = 0 ;
	}
#endif

	font->xft_font = xfont ;

	font->height = xfont->height ;
	font->height_to_baseline = xfont->ascent ;

	font->x_off = 0 ;
	font->width = ch_width ;

	font->is_proportional = font->is_var_col_width ;

	/*
	 * checking if font height/height_to_baseline member is sane.
	 * font width must be always sane.
	 */

	if( font->height == 0)
	{
		/* XXX this may be inaccurate. */
		font->height = fontsize ;
	}

	if( font->height_to_baseline == 0)
	{
		/* XXX this may be inaccurate. */
		font->height_to_baseline = fontsize ;
	}

	/*
	 * set_decsp_font() is called after dummy font is loaded to get font metrics.
	 * Since dummy font encoding is "iso8859-1", loading rarely fails.
	 */
	/* XXX dec specials must always be composed for now */
	if( /* compose_dec_special_font && */ FONT_CS(font->id) == DEC_SPECIAL)
	{
		return  set_decsp_font( font) ;
	}

	return  1 ;
}

#endif  /* USE_TYPE_XFT */

#ifdef  USE_TYPE_XCORE

static u_int
calculate_char_width(
	Display *  display ,
	XFontStruct *  xfont ,
	u_char *  ch ,
	size_t  len
	)
{
	if( len == 1)
	{
		return  XTextWidth( xfont , ch , 1) ;
	}
	else if( len == 2)
	{
		XChar2b  c ;

		c.byte1 = ch[0] ;
		c.byte2 = ch[1] ;

		return  XTextWidth16( xfont , &c , 1) ;
	}
	else if( len == 4)
	{
		/* is UCS4 */

		XChar2b  c ;

		/* dealing as UCS2 */

		c.byte1 = ch[2] ;
		c.byte2 = ch[3] ;

		return  XTextWidth16( xfont , &c , 1) ;
	}
	else
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " char size %d is too large.\n" , len) ;
	#endif

		return  0 ;
	}
}

static int
parse_xfont_name(
	char **  font_xlfd ,
	char **  percent ,
	char *  font_name
	)
{
	/*
	 * XftFont format.
	 * [Font XLFD](:[Percentage])
	 */

	if( ( *font_xlfd = kik_str_sep( &font_name , ":")) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " illegal true type font name(%s).\n" ,
			font_name) ;
	#endif

		return  0 ;
	}

	/* may be NULL */
	*percent = font_name ;

	return  1 ;
}

static XFontStruct *
load_xfont(
	Display *  display ,
	char *  family ,
	char *  weight ,
	char *  slant ,
	char *  width ,
	u_int  fontsize ,
	char *  spacing ,
	char *  encoding
	)
{
	XFontStruct *  xfont ;
	char *  fontname ;
	size_t  max_len ;

	/* "+ 20" means the num of '-' , '*'(19byte) and null chars. */
	max_len = 3 /* gnu */ + strlen(family) + 7 /* unifont */ + strlen( weight) +
		strlen( slant) + strlen( width) + 2 /* lang */ + DIGIT_STR_LEN(fontsize) +
		strlen( spacing) + strlen( encoding) + 20 ;

	if( ( fontname = alloca( max_len)) == NULL)
	{
		return  NULL ;
	}

	kik_snprintf( fontname , max_len , "-*-%s-%s-%s-%s--%d-*-*-*-%s-*-%s" ,
		family , weight , slant , width , fontsize , spacing , encoding) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " loading %s.\n" , fontname) ;
#endif

	if( ( xfont = XLoadQueryFont( display , fontname)))
	{
		return  xfont ;
	}

	if( strcmp( encoding , "iso10646-1") == 0 && strcmp( family , "biwidth") == 0)
	{
		/* XFree86 Unicode font */

		kik_snprintf( fontname , max_len , "-*-*-%s-%s-%s-%s-%d-*-*-*-%s-*-%s" ,
			weight , slant , width , kik_get_lang() , fontsize , spacing , encoding) ;

	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " loading %s.\n" , fontname) ;
	#endif

		if( ( xfont = XLoadQueryFont( display , fontname)))
		{
			return  xfont ;
		}

		if( strcmp( kik_get_lang() , "ja") != 0)
		{
			kik_snprintf( fontname , max_len , "-*-*-%s-%s-%s-ja-%d-*-*-*-%s-*-%s" ,
				weight , slant , width , fontsize , spacing , encoding) ;

		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " loading %s.\n" , fontname) ;
		#endif

			if( ( xfont = XLoadQueryFont( display , fontname)))
			{
				return  xfont ;
			}
		}

		/* GNU Unifont */

		kik_snprintf( fontname , max_len , "-gnu-unifont-%s-%s-%s--%d-*-*-*-%s-*-%s" ,
			weight , slant , width , fontsize , spacing , encoding) ;

	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " loading %s.\n" , fontname) ;
	#endif

		return  XLoadQueryFont( display , fontname) ;
	}
	else
	{
		return  NULL ;
	}
}

static int
set_xfont(
	x_font_t *  font ,
	char *  fontname ,
	u_int  fontsize ,
	u_int  col_width ,	/* if usascii font wants to be set , 0 will be set */
	int  use_medium_for_bold
	)
{
	XFontStruct *  xfont ;
	char *  weight ;
	char *  slant ;
	char *  width ;
	char *  family ;
	cs_info_t *  csinfo ;
	char **  font_encoding_p ;
	u_int  percent ;

	if( ( csinfo = get_cs_info( FONT_CS(font->id))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " get_cs_info(cs %x(id %x)) failed.\n" ,
			FONT_CS(font->id) , font->id) ;
	#endif

		return  0 ;
	}

	if( use_medium_for_bold)
	{
		font->is_double_drawing = 1 ;
	}
	else
	{
		font->is_double_drawing = 0 ;
	}

	if( fontname)
	{
		char *  p ;
		char *  font_xlfd ;
		char *  percent_str ;

		if( ( p = kik_str_alloca_dup( fontname)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
		#endif

			return  0 ;
		}

		if( parse_xfont_name( &font_xlfd , &percent_str , p))
		{
		#ifdef __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " loading %s font.\n" , font_xlfd) ;
		#endif

			if( ( xfont = XLoadQueryFont( font->display , font_xlfd)))
			{
				if( percent_str == NULL || ! kik_str_to_uint( &percent , percent_str))
				{
					percent = 0 ;
				}

				goto  font_found ;
			}

			kik_msg_printf( " font %s couln't be loaded.\n" , font_xlfd) ;
		}
	}

	percent = 0 ;

	/*
	 * searching apropriate font by using font info.
	 */

#ifdef  __DEBUG
	kik_debug_printf( "font for id %x will be loaded.\n" , font->id) ;
#endif

	if( font->id & FONT_BOLD)
	{
		weight = "bold" ;
	}
	else
	{
		weight = "medium" ;
	}

	slant = "r" ;

	width = "normal" ;

	if( font->id & FONT_BIWIDTH)
	{
		family = "biwidth" ;
	}
	else
	{
		family = "*" ;
	}

	FOREACH_FONT_ENCODINGS(csinfo,font_encoding_p)
	{
		if( ( xfont = load_xfont( font->display , family , weight , slant ,
			width , fontsize , "c" , *font_encoding_p)))
		{
			goto  font_found ;
		}
	}

	if( font->id & FONT_BOLD)
	{
		FOREACH_FONT_ENCODINGS(csinfo,font_encoding_p)
		{
			if( ( xfont = load_xfont( font->display , family , weight , "*" , "*" ,
				fontsize , "c" , *font_encoding_p)))
			{
				goto  font_found ;
			}
		}

		/*
		 * loading variable width font :(
		 */

		FOREACH_FONT_ENCODINGS(csinfo,font_encoding_p)
		{
			if( ( xfont = load_xfont( font->display , family , weight , "*" , "*" ,
				fontsize , "m" , *font_encoding_p)))
			{
				goto   font_found ;
			}
		}

		FOREACH_FONT_ENCODINGS(csinfo,font_encoding_p)
		{
			if( ( xfont = load_xfont( font->display , family , weight , "*" , "*" ,
				fontsize , "p" , *font_encoding_p)))
			{
				goto   font_found ;
			}
		}

		/* no bold font is found. */
		font->is_double_drawing = 1 ;
	}

	FOREACH_FONT_ENCODINGS(csinfo,font_encoding_p)
	{
		if( ( xfont = load_xfont( font->display , family , "*" , "*" , "*" , fontsize ,
			"c" , *font_encoding_p)))
		{
			goto   font_found ;
		}
	}

	/*
	 * loading variable width font :(
	 */

	FOREACH_FONT_ENCODINGS(csinfo,font_encoding_p)
	{
		if( ( xfont = load_xfont( font->display , family , "*" , "*" , "*" , fontsize ,
			"m" , *font_encoding_p)))
		{
			goto   font_found ;
		}
	}

	FOREACH_FONT_ENCODINGS(csinfo,font_encoding_p)
	{
		if( ( xfont = load_xfont( font->display , family , "*" , "*" , "*" , fontsize ,
			"p" , *font_encoding_p)))
		{
			goto   font_found ;
		}
	}

	return  0 ;

font_found:

	font->xfont = xfont ;

	font->height = xfont->ascent + xfont->descent ;
	font->height_to_baseline = xfont->ascent ;

	/*
	 * calculating actual font glyph width.
	 */
	if( ( FONT_CS(font->id) == ISO10646_UCS4_1 && ! (font->id & FONT_BIWIDTH)) ||
		FONT_CS(font->id) == TIS620_2533)
	{
		/*
		 * XXX hack
		 * a font including combining chars or an half width unicode font
		 * (which may include both half and full width glyphs).
		 * in this case , whether the font is proportional or not cannot be
		 * determined by comparing min_bounds and max_bounds.
		 * so , if `i' and `W' chars have different width , the font is regarded
		 * as proportional(and `W' width is used as font->width)
		 */

		u_int  w_width ;
		u_int  i_width ;

		w_width = calculate_char_width( font->display , font->xfont , "W" , 1) ;
		i_width = calculate_char_width( font->display , font->xfont , "i" , 1) ;

		if( w_width == 0)
		{
			font->is_proportional = 1 ;
			font->width = xfont->max_bounds.width ;
		}
		else if( i_width == 0 || w_width != i_width)
		{
			font->is_proportional = 1 ;
			font->width = w_width ;
		}
		else
		{
			font->is_proportional = 0 ;
			font->width = w_width ;
		}
	}
	else if( FONT_CS(font->id) == ISO10646_UCS4_1 && font->id & FONT_BIWIDTH)
	{
		/*
		 * XXX
		 * at the present time , all full width unicode fonts (which may include both
		 * half width and full width glyphs) are regarded as fixed.
		 * since I don't know what chars to be compared to determine font proportion
		 * and width.
		 */

		font->is_proportional = 0 ;
		font->width = xfont->max_bounds.width ;
	}
	else
	{
		if( xfont->max_bounds.width != xfont->min_bounds.width)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG
				" max font width(%d) and min one(%d) are mismatched.\n" ,
				xfont->max_bounds.width , xfont->min_bounds.width) ;
		#endif

			font->is_proportional = 1 ;
		}
		else
		{
			font->is_proportional = 0 ;
		}

		font->width = xfont->max_bounds.width ;
	}

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
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG
					" font width(%d) is not matched with standard width(%d).\n" ,
					font->width , col_width * font->cols) ;
			#endif

				font->is_proportional = 1 ;

				if( font->width < col_width * font->cols)
				{
					font->x_off = (col_width * font->cols - font->width) / 2 ;
				}

				font->width = col_width * font->cols ;
			}
		}
	}


	/*
	 * checking if font width/height/height_to_baseline member is sane.
	 */

	if( font->width == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " font width is 0.\n") ;
	#endif

		font->is_proportional = 1 ;

		/* XXX this may be inaccurate. */
		font->width = (fontsize / 2) * font->cols ;
	}

	if( font->height == 0)
	{
		/* XXX this may be inaccurate. */
		font->height = fontsize ;
	}

	if( font->height_to_baseline == 0)
	{
		/* XXX this may be inaccurate. */
		font->height_to_baseline = fontsize ;
	}

	/*
	 * set_decsp_font() is called after dummy font is loaded to get font metrics.
	 * Since dummy font encoding is "iso8859-1", loading rarely fails.
	 */
	if( compose_dec_special_font && FONT_CS(font->id) == DEC_SPECIAL)
	{
		return  set_decsp_font( font) ;
	}

	return  1 ;
}

#endif	/* USE_TYPE_XCORE */

#endif  /* USE_WIN32GUI */


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

  	font->fid = CreateFont( fontsize, fontsize / 2,
                           0, /* text angle */
                           0, /* char angle */
                           font->id & FONT_BOLD ? FW_BOLD : FW_REGULAR, /* weight */
                           FALSE, /* italic */
                           FALSE, /* underline */
                           FALSE, /* eraseline */
                           wincsinfo->cs, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           PROOF_QUALITY, FIXED_PITCH | FF_MODERN, fontname) ;

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

#ifndef  USE_WIN32GUI
	switch( type_engine)
	{
	default:
		return  NULL ;

#ifdef  USE_TYPE_XFT
	case  TYPE_XFT:
		if( ! set_xft_font( font , fontname , fontsize , col_width , use_medium_for_bold ,
			(font_present & FONT_AA) == FONT_AA))
		{
			free( font) ;

			return  NULL ;
		}

		break ;
#endif

#ifdef  USE_TYPE_XCORE
	case  TYPE_XCORE:
		if( font_present & FONT_AA)
		{
			return  NULL ;
		}
		else if( ! set_xfont( font , fontname , fontsize , col_width , use_medium_for_bold))
		{
			free( font) ;

			return  NULL ;
		}

		break ;
#endif
	}
#endif	/* USE_WIN32GUI */

	return  font ;
}

int
x_font_delete(
	x_font_t *  font
	)
{
#ifdef  USE_WIN32GUI
	if( font->fid)
	{
		DeleteObject(font->fid) ;
	}
	
	if( font->conv)
	{
		font->conv->delete( font->conv) ;
	}
#else
#ifdef  USE_TYPE_XFT
	if( font->xft_font)
	{
		XftFontClose( font->display , font->xft_font) ;
		font->xft_font = NULL ;
	}
#endif
#ifdef  USE_TYPE_XCORE
	if( font->xfont)
	{
		XFreeFont( font->display , font->xfont) ;
		font->xfont = NULL ;
	}
#endif

	if( font->decsp_font)
	{
		x_decsp_font_delete( font->decsp_font , font->display) ;
		font->decsp_font = NULL ;
	}
#endif	/* USE_WIN32GUI */

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
#ifndef  USE_WIN32GUI
	if( font->is_var_col_width && ! font->decsp_font)
	{
	#ifdef  USE_TYPE_XFT
		if( font->xft_font)
		{
			u_char  ucs4[4] ;

			if( cs != US_ASCII)
			{
				if( ! ml_convert_to_xft_ucs4( ucs4 , ch , len , cs))
				{
					return  0 ;
				}

				ch = ucs4 ;
				len = 4 ;
			}

			return  xft_calculate_char_width( font->display , font->xft_font , ch , len) ;
		}
	#endif
	#ifdef  USE_TYPE_XCORE
		if( font->xfont)
		{
			return  calculate_char_width( font->display , font->xfont , ch , len) ;
		}
	#endif

		kik_error_printf( __FUNCTION__ " couldn't calculate correct font width.\n") ;
	}
#endif

	return  font->width ;
}

char **
x_font_get_cs_names(
	mkf_charset_t  cs
	)
{
#ifdef  USE_WIN32GUI
	static char *  csnames[] = { "iso8859-1" } ;	/* dummy */
	
	return  csnames ;
#else
	cs_info_t  * info ;

	info = get_cs_info( cs) ;

	return  info->encoding_names ;
#endif
}

#ifdef  DEBUG

int
x_font_dump(
	x_font_t *  font
	)
{
#ifdef  USE_WIN32GUI
	kik_msg_printf( "  id %x: XFont %p" , font->id , font->fid) ;
#else
#ifdef  USE_TYPE_XCORE
	kik_msg_printf( "  id %x: XFont %p" , font->id , font->xfont) ;
#endif
#ifdef  USE_TYPE_XFT
	kik_msg_printf( " XftFont %p" , font->id , font->xft_font) ;
#endif
#endif
	if( font->is_proportional)
	{
		kik_msg_printf( " (proportional)") ;
	}
	kik_msg_printf( "\n") ;

	return  1 ;
}

#endif
