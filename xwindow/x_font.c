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


typedef struct  cs_info
{
	mkf_charset_t  cs ;
	
	/* 
	 * default encoding.
	 *
	 * !! Notice !!
	 * the last element must be NULL.
	 */
	char *   encoding_names[3] ;
	
} cs_info_t ;


/* --- static variables --- */

static cs_info_t  cs_info_table[] =
{
	{ DEC_SPECIAL , { "iso8859-1" , NULL , NULL , } , } ,
	{ ISO8859_1_R , { "iso8859-1" , NULL , NULL , } , } ,
	{ ISO8859_2_R , { "iso8859-2" , NULL , NULL , } , } ,
	{ ISO8859_3_R , { "iso8859-3" , NULL , NULL , } , } ,
	{ ISO8859_4_R , { "iso8859-4" , NULL , NULL , } , } ,
	{ ISO8859_5_R , { "iso8859-5" , NULL , NULL , } , } ,
	{ ISO8859_6_R , { "iso8859-6" , NULL , NULL , } , } ,
	{ ISO8859_7_R , { "iso8859-7" , NULL , NULL , } , } ,
	{ ISO8859_8_R , { "iso8859-8" , NULL , NULL , } , } ,
	{ ISO8859_9_R , { "iso8859-9" , NULL , NULL , } , } ,
	{ ISO8859_10_R , { "iso8859-10" , NULL , NULL , } , } ,
	{ ISCII , { NULL , NULL , NULL , } , } ,
	{ TIS620_2533 , { "tis620.2533-1" , "tis620.2529-1" , NULL , } , } ,
	{ ISO8859_13_R , { "iso8859-13" , NULL , NULL , } , } ,
	{ ISO8859_14_R , { "iso8859-14" , NULL , NULL , } , } ,
	{ ISO8859_15_R , { "iso8859-15" , NULL , NULL , } , } ,
	
	/*
	 * XXX
	 * The encoding of TCVN font is iso8859-1 , and its font family is
	 * .VnTime or .VnTimeH ...
	 * How to deal with it ?
	 */
	{ TCVN5712_3_1993 , { NULL , NULL , NULL , } , } ,

	{ VISCII , { "viscii-1" , NULL , NULL , } , } ,
	{ KOI8_R , { "koi8-r" , NULL , NULL , } , } ,
	{ KOI8_U , { "koi8-u" , NULL , NULL , } , } ,
	{ JISX0201_KATA , { "jisx0201.1976-0" , NULL , NULL , } , } ,
	{ JISX0201_ROMAN , { "jisx0201.1976-0" , NULL , NULL , } , } ,
	{ JISC6226_1978 , { "jisx0208.1978-0" , "jisx0208.1983-0" , NULL , } , } ,
	{ JISX0208_1983 , { "jisx0208.1983-0" , "jisx0208.1990-0" , NULL , } , } ,
	{ JISX0208_1990 , { "jisx0208.1990-0" , "jisx0208.1983-0" , NULL , } , } ,
	{ JISX0212_1990 , { "jisx0212.1990-0" , NULL , NULL , } , } ,
	{ JISX0213_2000_1 , { "jisx0213.2000-1" , NULL , NULL , } , } ,
	{ JISX0213_2000_2 , { "jisx0213.2000-2" , NULL , NULL , } , } ,
	{ KSC5601_1987 , { "ksc5601.1987-0" , "ksx1001.1997-0" , NULL , } , } ,
	
	/*
	 * XXX
	 * UHC and JOHAB fonts are not used at the present time.
	 * see ml_term_parser.c:x_parse_vt100_sequence().
	 */
	{ UHC , { NULL , NULL , NULL , } , } ,
	{ JOHAB , { "johabsh-1" , /* "johabs-1" , */ "johab-1" , NULL , } , } ,

	{ GB2312_80 , { "gb2312.1980-0" , NULL , NULL , } , } ,
	{ GBK , { "gbk-0" , NULL , NULL , } , } ,
	{ BIG5 , { "big5.eten-0" , "big5.hku-0" , NULL , } , } ,
	{ HKSCS , { "big5hkscs-0" , "big5-0" , NULL , } , } ,
	{ CNS11643_1992_1 , { "cns11643.1992-1" , "cns11643.1992.1-0" , NULL , } , } ,
	{ CNS11643_1992_2 , { "cns11643.1992-2" , "cns11643.1992.2-0" , NULL , } , } ,
	{ CNS11643_1992_3 , { "cns11643.1992-3" , "cns11643.1992.3-0" , NULL , } , } ,
	{ CNS11643_1992_4 , { "cns11643.1992-4" , "cns11643.1992.4-0" , NULL , } , } ,
	{ CNS11643_1992_5 , { "cns11643.1992-5" , "cns11643.1992.5-0" , NULL , } , } ,
	{ CNS11643_1992_6 , { "cns11643.1992-6" , "cns11643.1992.6-0" , NULL , } , } ,
	{ CNS11643_1992_7 , { "cns11643.1992-7" , "cns11643.1992.7-0" , NULL , } , } ,
	{ ISO10646_UCS4_1 , { "iso10646-1" , NULL , NULL , } , } ,
	
} ;

static int  compose_dec_special_font ;


/* --- static functions --- */

static cs_info_t *
get_cs_info(
	mkf_charset_t  cs
	)
{
	int  count ;

	for( count = 0 ; count < sizeof( cs_info_table) / sizeof( cs_info_t) ;
		count ++)
	{
		if( cs_info_table[count].cs == cs)
		{
			return  &cs_info_table[count] ;
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

static XFontStruct *
load_xfont(
	x_font_t *  font ,
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
		return  0 ;
	}

	kik_snprintf( fontname , max_len , "-*-%s-%s-%s-%s--%d-*-*-*-%s-*-%s" ,
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
		
		kik_snprintf( fontname , max_len , "-*-*-%s-%s-%s-%s-%d-*-*-*-%s-*-%s" ,
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
			kik_snprintf( fontname , max_len , "-*-*-%s-%s-%s-ja-%d-*-*-*-%s-*-%s" ,
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
		
		kik_snprintf( fontname , max_len , "-gnu-unifont-%s-%s-%s--%d-*-*-*-%s-*-%s" ,
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
	x_font_t *  font
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

	if( font->decsp_font)
	{
		x_decsp_font_delete( font->decsp_font , font->display) ;
		font->decsp_font = NULL ;
	}

	return  1 ;
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

#ifdef  ANTI_ALIAS

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

	if( ( *font_family = kik_str_sep( &font_name , "-")) == NULL ||
		font_name == NULL)
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
		XFT_SPACING , XftTypeInteger , XFT_PROPORTIONAL , 0)))
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

static int
set_decsp_font(
	x_font_t *  font
	)
{
	/*
	 * freeing font->xfont or font->xft_font
	 */
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


/* --- global functions --- */

int
x_compose_dec_special_font(void)
{
	compose_dec_special_font = 1 ;

	return  1 ;
}


x_font_t *
x_font_new(
	Display *  display ,
	ml_font_t  id
	)
{
	x_font_t *  font ;

	if( ( font = malloc( sizeof( x_font_t))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif

		return  NULL ;
	}

	memset( font , 0 , sizeof( x_font_t)) ;

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

	return  font ;
}

int
x_font_delete(
	x_font_t *  font
	)
{
	unset_xfont( font) ;

	free( font) ;

	return  1 ;
}

int
x_font_set_font_present(
	x_font_t *  font ,
	x_font_present_t  font_present
	)
{
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

	return  1 ;
}


#ifdef  ANTI_ALIAS

int
x_font_set_xft_font(
	x_font_t *  font ,
	char *  fontname ,
	u_int  fontsize ,
	u_int  col_width ,	/* if usascii font wants to be set , 0 will be set. */
	int  use_medium_for_bold
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

	/* font->is_double_drawing always 0 because Xft must show any bold font. */
	font->is_double_drawing = 0 ;

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
						XFT_SPACING , XftTypeInteger , XFT_PROPORTIONAL , 0)))
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
						XFT_SPACING , XftTypeInteger , XFT_CHARCELL , 0)))
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
					XFT_SPACING , XftTypeInteger , XFT_PROPORTIONAL , 0)))
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
					XFT_SPACING , XftTypeInteger , XFT_CHARCELL , 0)))
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

	unset_xfont( font) ;

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
	if( compose_dec_special_font && FONT_CS(font->id) == DEC_SPECIAL)
	{
		return  set_decsp_font( font) ;
	}

	return  1 ;
}

#else

int
x_font_set_xft_font(
	x_font_t *  font ,
	char *  fontname ,
	u_int  fontsize ,
	u_int  col_width ,	/* if usascii font wants to be set , 0 will be set */
	int  use_medium_for_bold
	)
{
	return  x_font_set_xfont( font , fontname , fontsize , col_width , use_medium_for_bold) ;
}

#endif

int
x_font_set_xfont(
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
		if( ( xfont = load_xfont( font , family , weight , slant ,
			width , fontsize , "c" , *font_encoding_p)))
		{
			goto  font_found ;
		}
	}
	
	if( font->id & FONT_BOLD)
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
	if( ! font->is_var_col_width || font->decsp_font)
	{
		return  font->width ;
	}
	else
	{
	#ifdef  ANTI_ALIAS
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
		else
	#endif
		{
			return  calculate_char_width( font->display , font->xfont , ch , len) ;
		}
	}
}

#ifdef  DEBUG

int
x_font_dump(
	x_font_t *  font
	)
{
	kik_msg_printf( "  id %x: XFont %p" , font->id , font->xfont) ;
#ifdef  ANTI_ALIAS
	kik_msg_printf( " XftFont %p" , font->id , font->xft_font) ;
#endif
	if( font->is_proportional)
	{
		kik_msg_printf( " (proportional)") ;
	}
	kik_msg_printf( "\n") ;

	return  1 ;
}

#endif
