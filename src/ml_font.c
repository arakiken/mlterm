/*
 *	$Id$
 */

#include  "ml_font.h"

#include  <stdio.h>		/* sprintf */
#include  <string.h>		/* memset */
#include  <X11/Xatom.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>	/* alloca */
#include  <kiklib/kik_str.h>	/* kik_str_sep/kik_str_to_int */
#include  <kiklib/kik_util.h>	/* DIGIT_STR_LEN */
#include  <kiklib/kik_locale.h>	/* kik_get_lang() */

#include  "ml_font_intern.h"


#define  FOREACH_FONT_ENCODINGS(csinfo,font_encoding_p) \
	for( (font_encoding_p) = &csinfo->encoding_names[0] ; *(font_encoding_p) ; (font_encoding_p) ++)


#if  0
#define  __DEBUG
#endif


typedef struct  cs_info
{
	mkf_charset_t  cs ;
	u_int  cols ;
	char *   encoding_names[3] ;	/* default encoding. the last element must be NULL. */
	
} cs_info_t ;


/* --- static variables --- */

static cs_info_t  cs_info_table[] =
{
	{ DEC_SPECIAL , 1 , { "iso8859-1" , NULL , NULL , } , } ,
	{ ISO8859_1_R , 1 , { "iso8859-1" , NULL , NULL , } , } ,
	{ ISO8859_2_R , 1 , { "iso8859-2" , NULL , NULL , } , } ,
	{ ISO8859_3_R , 1 , { "iso8859-3" , NULL , NULL , } , } ,
	{ ISO8859_4_R , 1 , { "iso8859-4" , NULL , NULL , } , } ,
	{ ISO8859_5_R , 1 , { "iso8859-5" , NULL , NULL , } , } ,
	{ ISO8859_6_R , 1 , { "iso8859-6" , NULL , NULL , } , } ,
	{ ISO8859_7_R , 1 , { "iso8859-7" , NULL , NULL , } , } ,
	{ ISO8859_8_R , 1 , { "iso8859-8" , NULL , NULL , } , } ,
	{ ISO8859_9_R , 1 , { "iso8859-9" , NULL , NULL , } , } ,
	{ ISO8859_10_R , 1 , { "iso8859-10" , NULL , NULL , } , } ,
	{ TIS620_2533 , 1 , { "tis620.2533-1" , "tis620.2529-1" , NULL , } , } ,
	{ ISO8859_13_R , 1 , { "iso8859-13" , NULL , NULL , } , } ,
	{ ISO8859_14_R , 1 , { "iso8859-14" , NULL , NULL , } , } ,
	{ ISO8859_15_R , 1 , { "iso8859-15" , NULL , NULL , } , } ,
	/*
	 * XXX
	 * the encoding of TCVN font is iso8859-1 , and its font family is
	 * .VnTime or .VnTimeH ...
	 * how to deal with it ?
	 */
	{ TCVN5712_3_1993 , 1 , { NULL , NULL , NULL , } , } ,
	{ VISCII , 1 , { "viscii-1" , NULL , NULL , } , } ,
	{ KOI8_R , 1 , { "koi8-r" , NULL , NULL , } , } ,
	{ KOI8_U , 1 , { "koi8-u" , NULL , NULL , } , } ,
	{ JISX0201_KATA , 1 , { "jisx0201.1976-0" , NULL , NULL , } , } ,
	{ JISX0201_ROMAN , 1 , { "jisx0201.1976-0" , NULL , NULL , } , } ,
	{ JISC6226_1978 , 2 , { "jisx0208.1978-0" , NULL , NULL , } , } ,
	{ JISX0208_1983 , 2 , { "jisx0208.1983-0" , NULL , NULL , } , } ,
	{ JISX0208_1990 , 2 , { "jisx0208.1990-0" , NULL , NULL , } , } ,
	{ JISX0212_1990 , 2 , { "jisx0212.1990-0" , NULL , NULL , } , } ,
	{ JISX0213_2000_1 , 2 , { "jisx0213.2000-1" , NULL , NULL , } , } ,
	{ JISX0213_2000_2 , 2 , { "jisx0213.2000-2" , NULL , NULL , } , } ,
	{ KSC5601_1987 , 2 , { "ksc5601.1987-0" , "ksx1001.1997-0" , NULL , } , } ,
	{ UHC , 2 , { NULL , NULL , NULL , } , } ,
	{ JOHAB , 2 , { "johabsh-1" , /* "johabs-1" , */ "johab-1" , NULL , } , } ,
	{ GB2312_80 , 2 , { "gb2312.1980-0" , NULL , NULL , } , } ,
	{ GBK , 2 , { "gbk-0" , NULL , NULL , } , } ,
	{ BIG5 , 2 , { "big5.eten-0" , "big5.hku-0" , NULL , } , } ,
	{ BIG5HKSCS , 2 , { "big5hkscs-0" , "big5-0" , NULL , } , } ,
	{ CNS11643_1992_1 , 2 , { "cns11643.1992-1" , "cns11643.1992.1-0" , NULL , } , } ,
	{ CNS11643_1992_2 , 2 , { "cns11643.1992-2" , "cns11643.1992.2-0" , NULL , } , } ,
	{ CNS11643_1992_3 , 2 , { "cns11643.1992-3" , "cns11643.1992.3-0" , NULL , } , } ,
	{ CNS11643_1992_4 , 2 , { "cns11643.1992-4" , "cns11643.1992.4-0" , NULL , } , } ,
	{ CNS11643_1992_5 , 2 , { "cns11643.1992-5" , "cns11643.1992.5-0" , NULL , } , } ,
	{ CNS11643_1992_6 , 2 , { "cns11643.1992-6" , "cns11643.1992.6-0" , NULL , } , } ,
	{ CNS11643_1992_7 , 2 , { "cns11643.1992-7" , "cns11643.1992.7-0" , NULL , } , } ,
	{ ISO10646_UCS2_1 , 1 /* default value */ , { "iso10646-1" , NULL , NULL , } , } ,
	{ ISO10646_UCS4_1 , 1 /* default value */ , { "iso10646-1" , NULL , NULL , } , } ,
	
} ;


/* --- static functions --- */

static cs_info_t *
get_cs_info(
	mkf_charset_t  cs
	)
{
	int  counter ;

	for( counter = 0 ; counter < sizeof( cs_info_table) / sizeof( cs_info_t) ;
		counter ++)
	{
		if( cs_info_table[counter].cs == cs)
		{
			return  &cs_info_table[counter] ;
		}
	}
	
#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " not supported cs(%x).\n" , cs) ; 
#endif

	return  NULL ;
}

#ifdef  ANTI_ALIAS

static u_int
xft_calculate_char_width(
	Display *  display ,
	XftFont *  xfont ,
	u_char *  ch ,
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

	return  extents.width ;
}

#endif


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

#ifdef  ANTI_ALIAS

static u_int
get_xft_col_width(
	ml_font_t *  font ,
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
			XFT_SPACING , XftTypeInteger , XFT_CHARCELL , 0)))
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

#endif

static XFontStruct *
load_xfont(
	ml_font_t *  font ,
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
	
	/* "+ 16" means the num of '-' , '*'(15byte) and null chars. */
	if( ( fontname = alloca( sizeof( char) *
		(strlen(family) + strlen( weight) + strlen( slant) + strlen( width) +
		DIGIT_STR_LEN(fontsize) + strlen( spacing) + strlen( encoding) + 16))) == NULL)
	{
		return  0 ;
	}
	
	sprintf( fontname , "-*-%s-%s-%s-%s--%d-*-%s-*-%s" ,
		family , weight , slant , width , fontsize , spacing , encoding) ;
		
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " loading %s.\n" , fontname) ;
#endif

	if( ( xfont = XLoadQueryFont( font->display , fontname)))
	{
		return  xfont ;
	}

	if( strcmp( encoding , "iso10646-1") == 0 && strcmp( family , "biwidth") == 0)
	{
		/* XFree86 Unicode font */
		
		sprintf( fontname , "-*-*-%s-%s-%s-%s-%d-*-%s-*-%s" ,
			weight , slant , width , kik_get_lang() , fontsize , spacing , encoding) ;

	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " loading %s.\n" , fontname) ;
	#endif

		if( ( xfont = XLoadQueryFont( font->display , fontname)))
		{
			return  xfont ;
		}

		if( strcmp( kik_get_lang() , "ja") != 0)
		{
			sprintf( fontname , "-*-*-%s-%s-%s-ja-%d-*-%s-*-%s" ,
				weight , slant , width , fontsize , spacing , encoding) ;

		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " loading %s.\n" , fontname) ;
		#endif
		
			if( ( xfont = XLoadQueryFont( font->display , fontname)))
			{
				return  xfont ;
			}
		}

		/* GNU Unifont */
		
		sprintf( fontname , "-gnu-unifont-%s-%s-%s--%d-*-%s-*-%s" ,
			weight , slant , width , fontsize , spacing , encoding) ;

	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " loading %s.\n" , fontname) ;
	#endif
		
		return  XLoadQueryFont( font->display , fontname) ;
	}
	else
	{
		return  NULL ;
	}
}

static int
unset_xfont(
	ml_font_t *  font
	)
{
#ifdef  ANTI_ALIAS
	if( font->xft_font)
	{
		XftFontClose( font->display , font->xft_font) ;
		font->xft_font = NULL ;
	}
	else
#endif
	if( font->xfont)
	{
		XFreeFont( font->display , font->xfont) ;
		font->xfont = NULL ;
	}

	return  1 ;
}


/* --- global functions --- */

ml_font_t *
ml_font_new(
	Display *  display ,
	ml_font_attr_t  attr
	)
{
	ml_font_t *  font ;

	if( ( font = malloc( sizeof( ml_font_t))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif

		return  NULL ;
	}

	memset( font , 0 , sizeof( ml_font_t)) ;

	font->attr = attr ;
	font->display = display ;

	return  font ;
}

int
ml_font_delete(
	ml_font_t *  font
	)
{
	unset_xfont( font) ;

	free( font) ;

	return  1 ;
}

#ifdef  ANTI_ALIAS

int
ml_font_set_xft_font(
	ml_font_t *  font ,
	char *  fontname ,
	u_int  fontsize ,
	u_int  col_width ,	/* if usascii font wants to be set , 0 will be set */
	int  use_medium_for_bold
	)
{
	int  weight ;
	cs_info_t *  csinfo ;
	u_int  cols ;
	u_int  ch_width ;
	XftFont *  xfont ;
	char *  font_encoding ;
	char **  font_encoding_p ;

	if( ( csinfo = get_cs_info( FONT_CS(font->attr))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " charset(0x%.2x) is not supported.\n" ,
			FONT_CS(font->attr)) ;
	#endif
		
		return  0 ;
	}

	/* font->is_double_drawing always 0 because Xft must show any bold font. */
	font->is_double_drawing = 0 ;

	if( font->attr & FONT_MEDIUM)
	{
		weight = XFT_WEIGHT_MEDIUM ;
	}
	else if( font->attr & FONT_BOLD)
	{
		weight = XFT_WEIGHT_BOLD ;
	}
	else
	{
		return  0 ;
	}

	if( font->attr & FONT_BIWIDTH)
	{
		cols = 2 ;
	}
	else
	{
		cols = csinfo->cols ;
	}
	
	if( fontname != NULL)
	{
		/*
		 * XftFont format.
		 * [Font Family]-[Font Encoding]
		 */
		 
		char *  p ;
		char *  font_family ;

		if( ( p = kik_str_alloca_dup( fontname)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
		#endif
		
			return  0 ;
		}
		
		if( ( font_family = kik_str_sep( &p , "-")) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " illegal true type font name(%s).\n" ,
				fontname) ;
		#endif
		
			return  0 ;
		}

		if( ( font_encoding = p) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " illegal true type font name(%s).\n" ,
				fontname) ;
		#endif

			return  0 ;
		}

		if( col_width == 0)
		{
			/* basic font (e.g. usascii) width */
			ch_width = get_xft_col_width( font , font_family , fontsize) * cols ;
		}
		else
		{
			ch_width = col_width * cols ;
		}

		if( ( xfont = XftFontOpen( font->display , DefaultScreen( font->display) ,
				XFT_FAMILY , XftTypeString , font_family ,
				XFT_PIXEL_SIZE , XftTypeDouble , (double)fontsize ,
				XFT_ENCODING , XftTypeString , font_encoding ,
				XFT_CHAR_WIDTH , XftTypeInteger , ch_width ,
				XFT_WEIGHT , XftTypeInteger , weight ,
				XFT_SPACING , XftTypeInteger , XFT_CHARCELL , 0)))
		{
			goto  font_found ;
		}

	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " %s font couln't be loaded.\n" , fontname) ;
	#endif
	}
	
	if( col_width == 0)
	{
		/* basic font (e.g. usascii) width */
		ch_width = get_xft_col_width( font , "" , fontsize) * cols ;
	}
	else
	{
		ch_width = col_width * cols ;
	}

	FOREACH_FONT_ENCODINGS(csinfo,font_encoding_p)
	{
		if( ( xfont = XftFontOpen( font->display , DefaultScreen( font->display) ,
				XFT_PIXEL_SIZE , XftTypeDouble , (double)fontsize ,
				XFT_ENCODING , XftTypeString , *font_encoding_p ,
				XFT_WEIGHT , XftTypeInteger , weight ,
				XFT_CHAR_WIDTH , XftTypeInteger , ch_width ,
				XFT_SPACING , XftTypeInteger , XFT_CHARCELL , 0)))
		{
			goto  font_found ;
		}
	}
	
#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " XftFontOpen(%s) failed.\n" , fontname) ;
#endif

	return  0 ;

font_found:

	unset_xfont( font) ;

	font->xft_font = xfont ;

	font->height = xfont->height ;
	font->height_to_baseline = xfont->ascent ;

	font->width = ch_width ;
	font->cols = cols ;

	font->is_proportional = 0 ;
	
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

	return  1 ;
}

#endif	/* ANTI_ALIAS */

int
ml_font_set_xfont(
	ml_font_t *  font ,
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

	if( ( csinfo = get_cs_info( FONT_CS(font->attr))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " get_cs_info(cs %x(attr %x)) failed.\n" ,
			FONT_CS(font->attr) , font->attr) ;
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

	if( fontname != NULL)
	{
		if( ( xfont = XLoadQueryFont( font->display , fontname)))
		{
			goto  font_found ;
		}

	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " %s font couln't be loaded.\n" , fontname) ;
	#endif
	}
	
	/*
	 * searching apropriate font by using font_attr info.
	 */

#ifdef  __DEBUG	 
	kik_debug_printf( "font for attr %x will be loaded.\n" , font->attr) ;
#endif

	if( font->attr & FONT_BOLD)
	{
		weight = "bold" ;
	}
	else if( font->attr & FONT_MEDIUM)
	{
		weight = "medium" ;
	}
	else
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " not supported font weight.\n") ; 
	#endif

		return  0 ;
	}

	if( font->attr & FONT_ITALIC)
	{
		slant = "i" ;
	}
	else if( font->attr & FONT_ROMAN)
	{
		slant = "r" ;
	}
	else
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " not supported font weight.\n") ; 
	#endif

		return  0 ;
	}

	if( font->attr & FONT_NORMAL)
	{
		width = "normal" ;
	}
	else
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " not supported font weight.\n") ; 
	#endif

		return  0 ;
	}

	if( font->attr & FONT_BIWIDTH)
	{
		family = "biwidth" ;
	}
	else
	{
		family = "*" ;
	}

	FOREACH_FONT_ENCODINGS(csinfo,font_encoding_p)
	{
		if( ( xfont = load_xfont( font , family , weight , slant ,
			width , fontsize , "c" , *font_encoding_p)))
		{
			goto  font_found ;
		}
	}
	
	if( font->attr & FONT_BOLD)
	{
		FOREACH_FONT_ENCODINGS(csinfo,font_encoding_p)
		{
			if( ( xfont = load_xfont( font , family , weight , "*" , "*" ,
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
			if( ( xfont = load_xfont( font , family , weight , "*" , "*" ,
				fontsize , "m" , *font_encoding_p)))
			{
				goto   font_found ;
			}
		}
		
		FOREACH_FONT_ENCODINGS(csinfo,font_encoding_p)
		{
			if( ( xfont = load_xfont( font , family , weight , "*" , "*" ,
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
		if( ( xfont = load_xfont( font , family , "*" , "*" , "*" , fontsize ,
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
		if( ( xfont = load_xfont( font , family , "*" , "*" , "*" , fontsize ,
			"m" , *font_encoding_p)))
		{
			goto   font_found ;
		}
	}
	
	FOREACH_FONT_ENCODINGS(csinfo,font_encoding_p)
	{
		if( ( xfont = load_xfont( font , family , "*" , "*" , "*" , fontsize ,
			"p" , *font_encoding_p)))
		{
			goto   font_found ;
		}
	}

	return  0 ;

font_found:

	unset_xfont( font) ;

	font->xfont = xfont ;
	
	font->height = xfont->ascent + xfont->descent ;
	font->height_to_baseline = xfont->ascent ;

	if( font->attr & FONT_BIWIDTH)
	{
		font->cols = 2 ;
	}
	else
	{
		font->cols = csinfo->cols ;
	}
	
	/*
	 * calculating actual font glyph width.
	 */
	if( (( FONT_CS(font->attr) == ISO10646_UCS4_1 || FONT_CS(font->attr) == ISO10646_UCS2_1) &&
		! (font->attr & FONT_BIWIDTH)) ||
		FONT_CS(font->attr) == TIS620_2533)
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
	else if( ( FONT_CS(font->attr) == ISO10646_UCS4_1 || FONT_CS(font->attr) == ISO10646_UCS2_1) &&
			font->attr & FONT_BIWIDTH)
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
			font->is_proportional = 1 ;
		}
		else
		{
			font->is_proportional = 0 ;
		}
		
		font->width = xfont->max_bounds.width ;
	}

	/*
	 * XXX hack
	 * forcibly conforming non standard font width to standard font width.
	 */
	if( col_width != 0)
	{
		/* not a standard(usascii) font */

		if( font->width != col_width * font->cols)
		{
			font->is_proportional = 1 ;
			font->width = col_width * font->cols ;
		}
	}


	/*
	 * checking if font width/height/height_to_baseline member is sane.
	 */
	 
	if( font->width == 0)
	{
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

	return  1 ;
}

mkf_charset_t
ml_font_cs(
	ml_font_t *  font
	)
{
	return  FONT_CS(font->attr) ;
}

int
ml_change_font_cs(
	ml_font_t *  font ,
	mkf_charset_t  cs
	)
{
	font->attr &= ~MAX_FONT_CS ;
	font->attr |= cs ;

	return  1 ;
}
