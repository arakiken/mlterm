/*
 *	$Id$
 */

#include  "../x_font.h"

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>	/* kik_snprintf */
#include  <kiklib/kik_mem.h>	/* alloca */
#include  <kiklib/kik_str.h>	/* kik_str_sep/kik_str_to_int/memset/strncasecmp */
#include  <kiklib/kik_util.h>	/* DIGIT_STR_LEN/K_MIN */
#include  <kiklib/kik_locale.h>	/* kik_get_lang() */
#include  <mkf/mkf_ucs4_map.h>
#include  <mkf/mkf_ucs_property.h>
#include  <ml_char_encoding.h>	/* ml_is_msb_set */

#include  "../x_type_loader.h"

#include  "x_decsp_font.h"


#define  FOREACH_FONT_ENCODINGS(csinfo,font_encoding_p) \
	for( (font_encoding_p) = (csinfo)->encoding_names ; \
	     (font_encoding_p) < (csinfo)->encoding_names + \
	         sizeof((csinfo)->encoding_names) / sizeof((csinfo)->encoding_names[0]) && \
	     *(font_encoding_p) ; \
	     (font_encoding_p) ++)

#define  DIVIDE_ROUNDING(a,b)  ( ((int)((a)*10 + (b)*5)) / ((int)((b)*10)) )
#define  DIVIDE_ROUNDINGUP(a,b) ( ((int)((a)*10 + (b)*10 - 1)) / ((int)((b)*10)) )

#if  0
#define  __DEBUG
#endif


typedef struct  cs_info
{
	mkf_charset_t  cs ;

	/* default encodings. */
	char *   encoding_names[2] ;

} cs_info_t ;


/* --- static variables --- */

/*
 * If this table is changed, x_font_config.c:cs_table and mc_font.c:cs_info_table
 * shoule be also changed.
 */
static cs_info_t  cs_info_table[] =
{
	{ ISO10646_UCS4_1 , { "iso10646-1" , NULL , } , } ,

	{ DEC_SPECIAL , { "iso8859-1" , NULL , } , } ,
	{ ISO8859_1_R , { "iso8859-1" , NULL , } , } ,
	{ ISO8859_2_R , { "iso8859-2" , NULL , } , } ,
	{ ISO8859_3_R , { "iso8859-3" , NULL , } , } ,
	{ ISO8859_4_R , { "iso8859-4" , NULL , } , } ,
	{ ISO8859_5_R , { "iso8859-5" , NULL , } , } ,
	{ ISO8859_6_R , { "iso8859-6" , NULL , } , } ,
	{ ISO8859_7_R , { "iso8859-7" , NULL , } , } ,
	{ ISO8859_8_R , { "iso8859-8" , NULL , } , } ,
	{ ISO8859_9_R , { "iso8859-9" , NULL , } , } ,
	{ ISO8859_10_R , { "iso8859-10" , NULL , } , } ,
	{ TIS620_2533 , { "tis620.2533-1" , "tis620.2529-1" , } , } ,
	{ ISO8859_13_R , { "iso8859-13" , NULL , } , } ,
	{ ISO8859_14_R , { "iso8859-14" , NULL , } , } ,
	{ ISO8859_15_R , { "iso8859-15" , NULL , } , } ,
	{ ISO8859_16_R , { "iso8859-16" , NULL , } , } ,

	/*
	 * XXX
	 * The encoding of TCVN font is iso8859-1 , and its font family is
	 * .VnTime or .VnTimeH ...
	 * How to deal with it ?
	 */
	{ TCVN5712_3_1993 , { NULL , NULL , } , } ,

	{ ISCII_ASSAMESE , { NULL , NULL , } , } ,
	{ ISCII_BENGALI , { NULL , NULL , } , } ,
	{ ISCII_GUJARATI , { NULL , NULL , } , } ,
	{ ISCII_HINDI , { NULL , NULL , } , } ,
	{ ISCII_KANNADA , { NULL , NULL , } , } ,
	{ ISCII_MALAYALAM , { NULL , NULL , } , } ,
	{ ISCII_ORIYA , { NULL , NULL , } , } ,
	{ ISCII_PUNJABI , { NULL , NULL , } , } ,
	{ ISCII_ROMAN , { NULL , NULL , } , } ,
	{ ISCII_TAMIL , { NULL , NULL , } , } ,
	{ ISCII_TELUGU , { NULL , NULL , } , } ,
	{ VISCII , { "viscii-1" , NULL , } , } ,
	{ KOI8_R , { "koi8-r" , NULL , } , } ,
	{ KOI8_U , { "koi8-u" , NULL , } , } ,

#if  0
	/*
	 * XXX
	 * KOI8_T, GEORGIAN_PS and CP125X charset can be shown by unicode font only.
	 */
	{ KOI8_T , { NULL , NULL , } , } ,
	{ GEORGIAN_PS , { NULL , NULL , } , } ,
	{ CP1250 , { NULL , NULL , } , } ,
	{ CP1251 , { NULL , NULL , } , } ,
	{ CP1252 , { NULL , NULL , } , } ,
	{ CP1253 , { NULL , NULL , } , } ,
	{ CP1254 , { NULL , NULL , } , } ,
	{ CP1255 , { NULL , NULL , } , } ,
	{ CP1256 , { NULL , NULL , } , } ,
	{ CP1257 , { NULL , NULL , } , } ,
	{ CP1258 , { NULL , NULL , } , } ,
	{ CP874 , { NULL , NULL , } , } ,
#endif

	{ JISX0201_KATA , { "jisx0201.1976-0" , NULL , } , } ,
	{ JISX0201_ROMAN , { "jisx0201.1976-0" , NULL , } , } ,
	{ JISC6226_1978 , { "jisx0208.1978-0" , "jisx0208.1983-0" , } , } ,
	{ JISX0208_1983 , { "jisx0208.1983-0" , "jisx0208.1990-0" , } , } ,
	{ JISX0208_1990 , { "jisx0208.1990-0" , "jisx0208.1983-0" , } , } ,
	{ JISX0212_1990 , { "jisx0212.1990-0" , NULL , } , } ,
	{ JISX0213_2000_1 , { "jisx0213.2000-1" , "jisx0208.1983-0" , } , } ,
	{ JISX0213_2000_2 , { "jisx0213.2000-2" , NULL , } , } ,
	{ KSC5601_1987 , { "ksc5601.1987-0" , "ksx1001.1997-0" , } , } ,

#if  0
	/*
	 * XXX
	 * UHC and JOHAB fonts are not used at the present time.
	 * see ml_vt100_parser.c:ml_parse_vt100_sequence().
	 */
	{ UHC , { NULL , NULL , } , } ,
	{ JOHAB , { "johabsh-1" , /* "johabs-1" , */ "johab-1" , } , } ,
#endif

	{ GB2312_80 , { "gb2312.1980-0" , NULL , } , } ,
	{ GBK , { "gbk-0" , NULL , } , } ,
	{ BIG5 , { "big5.eten-0" , "big5.hku-0" , } , } ,
	{ HKSCS , { "big5hkscs-0" , "big5-0" , } , } ,
	{ CNS11643_1992_1 , { "cns11643.1992-1" , "cns11643.1992.1-0" , } , } ,
	{ CNS11643_1992_2 , { "cns11643.1992-2" , "cns11643.1992.2-0" , } , } ,
	{ CNS11643_1992_3 , { "cns11643.1992-3" , "cns11643.1992.3-0" , } , } ,
	{ CNS11643_1992_4 , { "cns11643.1992-4" , "cns11643.1992.4-0" , } , } ,
	{ CNS11643_1992_5 , { "cns11643.1992-5" , "cns11643.1992.5-0" , } , } ,
	{ CNS11643_1992_6 , { "cns11643.1992-6" , "cns11643.1992.6-0" , } , } ,
	{ CNS11643_1992_7 , { "cns11643.1992-7" , "cns11643.1992.7-0" , } , } ,

} ;

static int  compose_dec_special_font ;
static int  use_point_size_for_fc ;
static double  dpi_for_fc ;


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

#if  ! defined(NO_DYNAMIC_LOAD_TYPE)
static int
xft_unset_font(
	x_font_t *  font
	)
{
	int (*func)( x_font_t *) ;

	if( ! ( func = x_load_type_xft_func( X_UNSET_FONT)))
	{
		return  0 ;
	}

	return  (*func)( font) ;
}
#elif  defined(USE_TYPE_XFT)
int  xft_unset_font( x_font_t *  font) ;
#endif

#if  ! defined(NO_DYNAMIC_LOAD_TYPE)
static int
cairo_unset_font(
	x_font_t *  font
	)
{
	int (*func)( x_font_t *) ;

	if( ! ( func = x_load_type_cairo_func( X_UNSET_FONT)))
	{
		return  0 ;
	}

	return  (*func)( font) ;
}
#elif  defined(USE_TYPE_CAIRO)
int  cairo_unset_font( x_font_t *  font) ;
#endif


static int
set_decsp_font(
	x_font_t *  font
	)
{
	/*
	 * freeing font->xfont or font->xft_font
	 */
#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT)
	if( font->xft_font)
	{
		xft_unset_font( font) ;
	}
#endif

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_CAIRO)
	if( font->cairo_font)
	{
		cairo_unset_font( font) ;
	}
#endif

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XCORE)
	if( font->xfont)
	{
		XFreeFont( font->display , font->xfont) ;
		font->xfont = NULL ;
	}
#endif

	if( ( font->decsp_font = x_decsp_font_new( font->display , font->width , font->height ,
					font->ascent)) == NULL)
	{
		return  0 ;
	}

	/* decsp_font is impossible to draw double with. */
	font->double_draw_gap = 0 ;

	/* decsp_font is always fixed pitch. */
	font->is_proportional = 0 ;

	return  1 ;
}


#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XCORE)

static u_int
xcore_calculate_char_width(
	Display *  display ,
	XFontStruct *  xfont ,
	u_int32_t  ch
	)
{
	int  width ;

	if( ch < 0x100)
	{
		u_char  c ;

		c = ch ;

		width = XTextWidth( xfont , &c , 1) ;
	}
	else
	{
		XChar2b  c[2] ;

		width = XTextWidth16( xfont , c , x_convert_ucs4_to_utf16( c , ch) / 2) ;
	}

	if( width < 0)
	{
		/* Some (indic) fonts could return minus value as text width. */
		return  0 ;
	}
	else
	{
		return  width ;
	}
}

static int
parse_xfont_name(
	char **  font_xlfd ,
	char **  percent ,	/* NULL can be returned. */
	char *  font_name	/* Don't specify NULL. Broken in this function */
	)
{
	/*
	 * XFont format.
	 * [Font XLFD](:[Percentage])
	 */

	/* kik_str_sep() never returns NULL because font_name isn't NULL. */
	*font_xlfd = kik_str_sep( &font_name , ":") ;
	
	/* may be NULL */
	*percent = font_name ;

	return  1 ;
}

static XFontStruct *
load_xfont(
	Display *  display ,
	const char *  family ,
	const char *  weight ,
	const char *  slant ,
	const char *  width ,
	u_int  fontsize ,
	const char *  spacing ,
	const char *  encoding
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

#ifdef  DEBUG
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

	#ifdef  DEBUG
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

		#ifdef  DEBUG
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

	#ifdef  DEBUG
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
xcore_set_font(
	x_font_t *  font ,
	const char *  fontname ,
	u_int  fontsize ,
	u_int  col_width ,	/* if usascii font wants to be set , 0 will be set */
	int  use_medium_for_bold ,
	u_int  letter_space
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
	int  count ;
	int  num_of_spacings ;
	char *  spacings[] = { "c" , "m" , "p" } ;

	if( ( csinfo = get_cs_info( FONT_CS(font->id))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " get_cs_info(cs %x(id %x)) failed.\n" ,
			FONT_CS(font->id) , font->id) ;
	#endif

		return  0 ;
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
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " loading %s font (%s percent).\n" ,
				font_xlfd , percent_str) ;
		#endif

			while( 1)
			{
				if( ! ( xfont = XLoadQueryFont( font->display , font_xlfd)))
				{
					char *  xlfd ;

					if( ( xlfd = kik_str_replace( font_xlfd ,
							"-bold-" , "-medium-")))
					{
						xfont = XLoadQueryFont( font->display , xlfd) ;
						free( xlfd) ;
					}

					if( ! xfont)
					{
						kik_msg_printf( "Font %s couldn't be loaded.\n" ,
							font_xlfd) ;

						break ;
					}

					font->double_draw_gap = 1 ;
				}
				else
				{
					font->double_draw_gap = use_medium_for_bold ;
				}

				if( percent_str == NULL ||
				    ! kik_str_to_uint( &percent , percent_str))
				{
					percent = 0 ;
				}

				goto  font_found ;
			}
		}
	}

	/*
	 * searching apropriate font by using font info.
	 */

#ifdef  __DEBUG
	kik_debug_printf( "font for id %x will be loaded.\n" , font->id) ;
#endif

	font->double_draw_gap = 0 ;

	percent = 0 ;

	if( font->id & FONT_BOLD)
	{
		weight = "bold" ;
	}
	else
	{
		weight = "medium" ;
	}

	if( font->id & FONT_ITALIC)
	{
		slant = "i" ;
	}
	else
	{
		slant = "r" ;
	}

	width = "normal" ;

	if( (font->id & FONT_FULLWIDTH) && (FONT_CS(font->id) == ISO10646_UCS4_1) )
	{
		family = "biwidth" ;
		num_of_spacings = sizeof(spacings) / sizeof(spacings[0]) ;
	}
	else
	{
		family = "fixed" ;
		num_of_spacings = 1 ;
	}

	for( count = 0 ; ; count ++)
	{
		FOREACH_FONT_ENCODINGS(csinfo,font_encoding_p)
		{
			int  idx ;

			for( idx = 0 ; idx < num_of_spacings ; idx++)
			{
				if( ( xfont = load_xfont( font->display , family ,
						weight , slant , width , fontsize ,
						spacings[idx] , *font_encoding_p)))
				{
					goto  font_found ;
				}
			}
		}

		if( count == 0)
		{
			width = "*" ;
			family = "*" ;
			num_of_spacings = sizeof(spacings) / sizeof(spacings[0]) ;
		}
		else if( count == 1)
		{
			slant = "*" ;
		}
		else if( count == 2)
		{
			weight = "*" ;

			if( font->id & FONT_BOLD)
			{
				/* no bold font is found. */
				font->double_draw_gap = 1 ;
			}
		}
		else
		{
			break ;
		}
	}

	return  0 ;

font_found:

	font->xfont = xfont ;

	font->height = xfont->ascent + xfont->descent ;
	font->ascent = xfont->ascent ;

	/*
	 * calculating actual font glyph width.
	 */
	font->is_proportional = 0 ;
	font->width = xfont->max_bounds.width ;

	if( xfont->max_bounds.width != xfont->min_bounds.width)
	{
		if( FONT_CS(font->id) == ISO10646_UCS4_1 ||
		    FONT_CS(font->id) == TIS620_2533)
		{
			if( font->id & FONT_FULLWIDTH)
			{
				/*
				 * XXX
				 * At the present time , all full width unicode fonts
				 * (which may include both half width and full width
				 * glyphs) are regarded as fixed.
				 * Since I don't know what chars to be compared to
				 * determine font proportion and width.
				 */
			}
			else
			{
				/*
				 * XXX
				 * A font including combining (0-width) glyphs or both half and
				 * full width glyphs.
				 * In this case , whether the font is proportional or not
				 * cannot be determined by comparing min_bounds and max_bounds,
				 * so if `i' and `W' chars have different width , the font is
				 * regarded as proportional (and `W' width is used as font->width).
				 */

				u_int  w_width ;
				u_int  i_width ;

				if( ( w_width = xcore_calculate_char_width( font->display ,
							font->xfont , 'W')) == 0)
				{
					font->is_proportional = 1 ;
				}
				else if( ( i_width = xcore_calculate_char_width( font->display ,
								font->xfont , 'i')) == 0 ||
					 w_width != i_width)
				{
					font->is_proportional = 1 ;
					font->width = w_width ;
				}
				else
				{
					font->width = w_width ;
				}
			}
		}
		else
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG
				" max font width(%d) and min one(%d) are mismatched.\n" ,
				xfont->max_bounds.width , xfont->min_bounds.width) ;
		#endif

			font->is_proportional = 1 ;
		}
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
				ch_width = DIVIDE_ROUNDING( fontsize * percent , 100) ;
			}
			else
			{
				ch_width = DIVIDE_ROUNDING( fontsize * percent , 200) ;
			}

			if( font->width != ch_width)
			{
				font->is_proportional = 1 ;

				if( ! font->is_var_col_width && font->width < ch_width)
				{
					/*
					 * If width(2) of '1' doesn't match ch_width(4)
					 * x_off = (4-2)/2 = 1.
					 * It means that starting position of drawing '1' is 1
					 * as follows.
					 *
					 *  0123
					 * +----+
					 * | ** |
					 * |  * |
					 * |  * |
					 * +----+
					 */
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
				kik_msg_printf( "Font(id %x) width(%d) is not matched with "
					"standard width(%d).\n" ,
					font->id , font->width , col_width) ;

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
				kik_msg_printf( "Font(id %x) width(%d) is not matched with "
					"standard width(%d).\n" ,
					font->id , font->width , col_width * font->cols) ;

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


	/*
	 * checking if font width/height/ascent member is sane.
	 */

	if( font->width == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " font width is 0.\n") ;
	#endif

		font->is_proportional = 1 ;

		/* XXX this may be inaccurate. */
		font->width = DIVIDE_ROUNDINGUP( fontsize * font->cols , 2) ;
	}

	if( font->height == 0)
	{
		/* XXX this may be inaccurate. */
		font->height = fontsize ;
	}

	if( font->ascent == 0)
	{
		/* XXX this may be inaccurate. */
		font->ascent = fontsize ;
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

#endif


#if  ! defined(NO_DYNAMIC_LOAD_TYPE)

static u_int
xft_calculate_char_width(
	x_font_t *  font ,
	u_int32_t  ch			/* US-ASCII or Unicode */
	)
{
	int (*func)( x_font_t * , u_int32_t) ;

	if( ! ( func = x_load_type_xft_func( X_CALCULATE_CHAR_WIDTH)))
	{
		return  0 ;
	}

	return  (*func)( font , ch) ;
}

static int
xft_set_font(
	x_font_t *  font ,
	const char *  fontname ,
	u_int  fontsize ,
	u_int  col_width ,	/* if usascii font wants to be set , 0 will be set. */
	u_int  letter_space ,
	int  aa_opt ,		/* 0 = default , 1 = enable , -1 = disable */
	int  use_point_size_for_fc ,
	double  dpi_for_fc
	)
{
	int (*func)( x_font_t * , const char * , u_int , u_int , u_int , int , int , double) ;

	if( ! ( func = x_load_type_xft_func( X_SET_FONT)))
	{
		return  0 ;
	}

	return  (*func)( font , fontname , fontsize , col_width , letter_space ,
			aa_opt , use_point_size_for_fc , dpi_for_fc) ;
}

#elif  defined(USE_TYPE_XFT)
u_int  xft_calculate_char_width( x_font_t *  font , u_int32_t  ch) ;
int  xft_set_font( x_font_t *  font , const char *  fontname , u_int  fontsize ,
	u_int  col_width , u_int  letter_space , int  aa_opt ,
	int  use_point_size_for_fc , double  dpi_for_fc) ;
#endif


#if  ! defined(NO_DYNAMIC_LOAD_TYPE)

static u_int
cairo_calculate_char_width(
	x_font_t *  font ,
	u_int32_t  ch		/* US-ASCII or Unicode */
	)
{
	int (*func)( x_font_t * , u_int32_t) ;

	if( ! ( func = x_load_type_cairo_func( X_CALCULATE_CHAR_WIDTH)))
	{
		return  0 ;
	}

	return  (*func)( font , ch) ;
}

static int
cairo_set_font(
	x_font_t *  font ,
	const char *  fontname ,
	u_int  fontsize ,
	u_int  col_width ,	/* if usascii font wants to be set , 0 will be set. */
	u_int  letter_space ,
	int  aa_opt ,		/* 0 = default , 1 = enable , -1 = disable */
	int  use_point_size_for_fc ,
	double  dpi_for_fc
	)
{
	int (*func)( x_font_t * , const char * , u_int , u_int , u_int , int , int , double) ;

	if( ! ( func = x_load_type_cairo_func( X_SET_FONT)))
	{
		return  0 ;
	}

	return  (*func)( font , fontname , fontsize , col_width , letter_space ,
			aa_opt , use_point_size_for_fc , dpi_for_fc) ;
}

#elif  defined(USE_TYPE_CAIRO)
u_int  cairo_calculate_char_width( x_font_t *  font , u_int32_t  ch) ;
int  cairo_set_font( x_font_t *  font , const char *  fontname , u_int  fontsize ,
	u_int  col_width , u_int  letter_space , int  aa_opt , int  use_point_size_for_fc ,
	double  dpi_for_fc) ;
#endif

static u_int
calculate_char_width(
	x_font_t *  font ,
	u_int32_t  ch ,
	mkf_charset_t  cs
	)
{
#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT)
	if( font->xft_font)
	{
		if( cs != US_ASCII && ! IS_ISCII(cs))
		{
			if( ! ( ch = x_convert_to_xft_ucs4( ch , cs)))
			{
				return  0 ;
			}
		}

		return  xft_calculate_char_width( font , ch) ;
	}
#endif

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_CAIRO)
	if( font->cairo_font)
	{
		if( cs != US_ASCII && ! IS_ISCII(cs))
		{
			if( ! ( ch = x_convert_to_xft_ucs4( ch , cs)))
			{
				return  0 ;
			}
		}

		return  cairo_calculate_char_width( font , ch) ;
	}
#endif

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XCORE)
	if( font->xfont)
	{
		return  xcore_calculate_char_width( font->display , font->xfont , ch) ;
	}
#endif

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " couldn't calculate correct font width.\n") ;
#endif

	return  0 ;
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
	ml_font_t  id ,
	x_type_engine_t  type_engine ,
	x_font_present_t  font_present , /* FONT_VAR_WIDTH is never set if FONT_VERTICAL is set. */
	const char *  fontname ,
	u_int  fontsize ,
	u_int  col_width ,
	int  use_medium_for_bold ,
	u_int  letter_space		/* ignored in variable column width mode. */
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

	if( ( font_present & FONT_VAR_WIDTH) || IS_ISCII(FONT_CS(font->id)))
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

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XCORE)
	font->xfont = NULL ;
#endif
#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT)
	font->xft_font = NULL ;
#endif
#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_CAIRO)
	font->cairo_font = NULL ;
#endif
	font->decsp_font = NULL ;

	switch( type_engine)
	{
	default:
		return  NULL ;

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT)
	case  TYPE_XFT:
		if( ! xft_set_font( font , fontname , fontsize , col_width , letter_space ,
				(font_present & FONT_AA) == FONT_AA ?
					1 : ((font_present & FONT_NOAA) == FONT_NOAA ? -1 : 0) ,
				use_point_size_for_fc , dpi_for_fc))
		{
			free( font) ;

			return  NULL ;
		}

		break ;
#endif

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_CAIRO)
	case  TYPE_CAIRO:
		if( ! cairo_set_font( font , fontname , fontsize , col_width , letter_space ,
				(font_present & FONT_AA) == FONT_AA ?
					1 : ((font_present & FONT_NOAA) == FONT_NOAA ? -1 : 0) ,
				use_point_size_for_fc , dpi_for_fc))
		{
			free( font) ;

			return  NULL ;
		}

		break ;
#endif

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XCORE)
	case  TYPE_XCORE:
		if( font_present & FONT_AA)
		{
			return  NULL ;
		}
		else if( ! xcore_set_font( font , fontname , fontsize , col_width ,
				use_medium_for_bold , letter_space))
		{
			free( font) ;

			return  NULL ;
		}

		goto  end ;
#endif
	}

	/*
	 * set_decsp_font() is called after dummy xft/cairo font is loaded to get font metrics.
	 * Since dummy font encoding is "iso8859-1", loading rarely fails.
	 */
	/* XXX dec specials must always be composed for now */
	if( /* compose_dec_special_font && */ FONT_CS(font->id) == DEC_SPECIAL)
	{
		set_decsp_font( font) ;
	}

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XCORE)
end:
#endif
	if( font->is_proportional && ! font->is_var_col_width)
	{
		kik_msg_printf(
			"Characters (cs %x) are drawn *one by one* to arrange column width.\n" ,
			FONT_CS(font->id)) ;
	}

#ifdef  DEBUG
	x_font_dump( font) ;
#endif

	return  font ;
}

int
x_font_delete(
	x_font_t *  font
	)
{
#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT)
	if( font->xft_font)
	{
		xft_unset_font( font) ;
	}
#endif

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_CAIRO)
	if( font->cairo_font)
	{
		cairo_unset_font( font) ;
	}
#endif

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XCORE)
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
			/* Returned value can be 0 if iscii font is used. */
			return  calculate_char_width( font , ch , cs) ;
		}

		if( draw_alone)
		{
			*draw_alone = 1 ;
		}
	}
	else if( draw_alone && cs == ISO10646_UCS4_1)
	{
		if( (( mkf_get_ucs_property( ch) & MKF_AWIDTH) ||
		    /*
		     * The width of U+2590 and U+2591 is narrow in EastAsianWidth-6.3.0
		     * but the glyphs in GNU Unifont are full-width unexpectedly.
		     */
		    ch == 0x2590 || ch == 0x2591))
		{
			if( calculate_char_width( font , ch , cs) != font->width)
			{
				*draw_alone = 1 ;
			}
		}
	}

	return  font->width ;
}

char **
x_font_get_encoding_names(
	mkf_charset_t  cs
	)
{
	cs_info_t *  info ;

	if( ( info = get_cs_info( cs)))
	{
		return  info->encoding_names ;
	}
	else
	{
		return  NULL ;
	}
}

void
x_font_use_point_size(
	int  use
	)
{
	use_point_size_for_fc = use ;
}

/* For mlterm-libvte */
void
x_font_set_dpi_for_fc(
	double  dpi
	)
{
	dpi_for_fc = dpi ;
}

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT) || defined(USE_TYPE_CAIRO)

static int  use_cp932_ucs_for_xft = 0 ;

static u_int32_t
convert_to_ucs4(
	u_int32_t  ch ,
	mkf_charset_t  cs
	)
{
	if( cs == ISO10646_UCS4_1 /* || cs == ISO10646_UCS2_1 */)
	{
		return  ch ;
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
			return  mkf_char_to_int( &ucs4) ;
		}
		else
		{
			return  0 ;
		}
	}

	return  1 ;
}

int
x_use_cp932_ucs_for_xft(void)
{
	use_cp932_ucs_for_xft = 1 ;

	return  1 ;
}

/*
 * used only for xft or cairo.
 */
u_int32_t
x_convert_to_xft_ucs4(
	u_int32_t  ch ,
	mkf_charset_t  cs	/* US_ASCII and ISO8859_1_R is not accepted */
	)
{
	if( cs == US_ASCII || cs == ISO8859_1_R)
	{
		return  0 ;
	}
	else if( use_cp932_ucs_for_xft && cs == JISX0208_1983)
	{
		if( ch == 0x2140)
		{
			return  0xff3c ;
		}
		else if( ch == 0x2141)
		{
			return  0xff5e ;
		}
		else if( ch == 0x2142)
		{
			return  0x2225 ;
		}
		else if( ch == 0x215d)
		{
			return  0xff0d ;
		}
		else if( ch == 0x2171)
		{
			return  0xffe0 ;
		}
		else if( ch == 0x2172)
		{
			return  0xffe1 ;
		}
		else if( ch == 0x224c)
		{
			return  0xffe2 ;
		}
	}

	return  convert_to_ucs4( ch , cs) ;
}

#endif

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XCORE)

/* Return written size */
size_t
x_convert_ucs4_to_utf16(
	u_char *  dst ,	/* 4 bytes. Big endian. */
	u_int32_t  src
	)
{
#if  0
	kik_debug_printf( KIK_DEBUG_TAG "%.8x => " , src) ;
#endif

	if( src < 0x10000)
	{
		dst[0] = (src >> 8) & 0xff ;
		dst[1] = src & 0xff ;
	
		return  2 ;
	}
	else if( src < 0x110000)
	{
		/* surrogate pair */

		u_char  c ;

		src -= 0x10000 ;
		c = (u_char)( src / (0x100 * 0x400)) ;
		src -= (c * 0x100 * 0x400) ;
		dst[0] = c + 0xd8 ;

		c = (u_char)( src / 0x400) ;
		src -= (c * 0x400) ;
		dst[1] = c ;

		c = (u_char)( src / 0x100) ;
		src -= (c * 0x100) ;
		dst[2] = c + 0xdc ;
		dst[3] = (u_char)src ;

	#if  0
		kik_msg_printf( "%.2x%.2x%.2x%.2x\n" , dst[0] , dst[1] , dst[2] , dst[3]) ;
	#endif

		return  4 ;
	}

	return  0 ;
}

#endif


#ifdef  DEBUG

int
x_font_dump(
	x_font_t *  font
	)
{
#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XCORE)
	kik_msg_printf( "Font id %x: XFont %p " , font->id , font->xfont) ;
#endif
#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT)
	kik_msg_printf( "Font id %x: XftFont %p " , font->id , font->xft_font) ;
#endif
#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_CAIRO)
	kik_msg_printf( "Font id %x: CairoFont %p " , font->id , font->cairo_font) ;
#endif

	kik_msg_printf( "(width %d, height %d, ascent %d, x_off %d)" ,
		font->width , font->height , font->ascent , font->x_off) ;

	if( font->is_proportional)
	{
		kik_msg_printf( " (proportional)") ;
	}

	if( font->is_var_col_width)
	{
		kik_msg_printf( " (var col width)") ;
	}

	if( font->is_vertical)
	{
		kik_msg_printf( " (vertical)") ;
	}
	
	if( font->double_draw_gap)
	{
		kik_msg_printf( " (double drawing)") ;
	}

	kik_msg_printf( "\n") ;

	return  1 ;
}

#endif
