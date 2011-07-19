/*
 *	$Id$
 */

#include  "x_font.h"

#include  <string.h>		/* memset/strncasecmp */
#ifdef  USE_TYPE_CAIRO
#include  <cairo/cairo-xlib.h>
#include  <cairo/cairo-ft.h>
#endif
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>	/* kik_snprintf */
#include  <kiklib/kik_mem.h>	/* alloca */
#include  <kiklib/kik_str.h>	/* kik_str_sep/kik_str_to_int */
#include  <kiklib/kik_util.h>	/* DIGIT_STR_LEN/K_MIN */
#include  <kiklib/kik_locale.h>	/* kik_get_lang() */
#include  <mkf/mkf_ucs4_map.h>
#include  <ml_char_encoding.h>	/* ml_is_msb_set */
#include  <ml_char.h>		/* UTF_MAX_SIZE */

#define  FOREACH_FONT_ENCODINGS(csinfo,font_encoding_p) \
	for( (font_encoding_p) = &csinfo->encoding_names[0] ; *(font_encoding_p) ; (font_encoding_p) ++)
#define  DIVIDE_ROUNDING(a,b)  ( ((int)((a)*10 + (b)*5)) / ((int)((b)*10)) )
#define  DIVIDE_ROUNDINGUP(a,b) ( ((int)((a)*10 + (b)*10 - 1)) / ((int)((b)*10)) )

/* Be careful not to round down 5.99999... to 5 */
#define  DOUBLE_ROUNDUP_TO_INT(a)  ((int)((a) + 0.9))

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

/*
 * If this table is changed, x_font_config.c:cs_table and mc_font.c:cs_info_table
 * shoule be also changed.
 */
static cs_info_t  cs_info_table[] =
{
	{ ISO10646_UCS4_1 , { "iso10646-1" , NULL , NULL , } , } ,
	{ ISO10646_UCS2_1 , { "iso10646-1" , NULL , NULL , } , } ,

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
	{ TIS620_2533 , { "tis620.2533-1" , "tis620.2529-1" , NULL , } , } ,
	{ ISO8859_13_R , { "iso8859-13" , NULL , NULL , } , } ,
	{ ISO8859_14_R , { "iso8859-14" , NULL , NULL , } , } ,
	{ ISO8859_15_R , { "iso8859-15" , NULL , NULL , } , } ,
	{ ISO8859_16_R , { "iso8859-16" , NULL , NULL , } , } ,

	/*
	 * XXX
	 * The encoding of TCVN font is iso8859-1 , and its font family is
	 * .VnTime or .VnTimeH ...
	 * How to deal with it ?
	 */
	{ TCVN5712_3_1993 , { NULL , NULL , NULL , } , } ,

	{ ISCII_ASSAMESE , { NULL , NULL , NULL , } , } ,
	{ ISCII_BENGALI , { NULL , NULL , NULL , } , } ,
	{ ISCII_GUJARATI , { NULL , NULL , NULL , } , } ,
	{ ISCII_HINDI , { NULL , NULL , NULL , } , } ,
	{ ISCII_KANNADA , { NULL , NULL , NULL , } , } ,
	{ ISCII_MALAYALAM , { NULL , NULL , NULL , } , } ,
	{ ISCII_ORIYA , { NULL , NULL , NULL , } , } ,
	{ ISCII_PUNJABI , { NULL , NULL , NULL , } , } ,
	{ ISCII_ROMAN , { NULL , NULL , NULL , } , } ,
	{ ISCII_TAMIL , { NULL , NULL , NULL , } , } ,
	{ ISCII_TELUGU , { NULL , NULL , NULL , } , } ,
	{ VISCII , { "viscii-1" , NULL , NULL , } , } ,
	{ KOI8_R , { "koi8-r" , NULL , NULL , } , } ,
	{ KOI8_U , { "koi8-u" , NULL , NULL , } , } ,

#if  0
	/*
	 * XXX
	 * KOI8_T, GEORGIAN_PS and CP125X charset can be shown by unicode font only.
	 */
	{ KOI8_T , { NULL , NULL , NULL , } , } ,
	{ GEORGIAN_PS , { NULL , NULL , NULL , } , } ,
	{ CP1250 , { NULL , NULL , NULL , } , } ,
	{ CP1251 , { NULL , NULL , NULL , } , } ,
	{ CP1252 , { NULL , NULL , NULL , } , } ,
	{ CP1253 , { NULL , NULL , NULL , } , } ,
	{ CP1254 , { NULL , NULL , NULL , } , } ,
	{ CP1255 , { NULL , NULL , NULL , } , } ,
	{ CP1256 , { NULL , NULL , NULL , } , } ,
	{ CP1257 , { NULL , NULL , NULL , } , } ,
	{ CP1258 , { NULL , NULL , NULL , } , } ,
	{ CP874 , { NULL , NULL , NULL , } , } ,
#endif

	{ JISX0201_KATA , { "jisx0201.1976-0" , NULL , NULL , } , } ,
	{ JISX0201_ROMAN , { "jisx0201.1976-0" , NULL , NULL , } , } ,
	{ JISC6226_1978 , { "jisx0208.1978-0" , "jisx0208.1983-0" , NULL , } , } ,
	{ JISX0208_1983 , { "jisx0208.1983-0" , "jisx0208.1990-0" , NULL , } , } ,
	{ JISX0208_1990 , { "jisx0208.1990-0" , "jisx0208.1983-0" , NULL , } , } ,
	{ JISX0212_1990 , { "jisx0212.1990-0" , NULL , NULL , } , } ,
	{ JISX0213_2000_1 , { "jisx0213.2000-1" , NULL , NULL , } , } ,
	{ JISX0213_2000_2 , { "jisx0213.2000-2" , NULL , NULL , } , } ,
	{ KSC5601_1987 , { "ksc5601.1987-0" , "ksx1001.1997-0" , NULL , } , } ,

#if  0
	/*
	 * XXX
	 * UHC and JOHAB fonts are not used at the present time.
	 * see ml_vt100_parser.c:ml_parse_vt100_sequence().
	 */
	{ UHC , { NULL , NULL , NULL , } , } ,
	{ JOHAB , { "johabsh-1" , /* "johabs-1" , */ "johab-1" , NULL , } , } ,
#endif

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

} ;

static int  compose_dec_special_font ;
#if  defined(USE_TYPE_XFT) || defined(USE_TYPE_CAIRO)
static char *  fc_size_type = FC_PIXEL_SIZE ;
static double  dpi_for_fc ;
#endif


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

#if  defined(USE_TYPE_XFT) || defined(USE_TYPE_CAIRO)

static int
parse_fc_font_name(
	char **  font_family ,
	int *  font_weight ,	/* if weight is not specified in font_name , not changed. */
	int *  font_slant ,	/* if slant is not specified in font_name , not changed. */
	double *  font_size ,	/* if size is not specified in font_name , not changed. */
	char **  font_encoding ,/* if encoding is not specified in font_name , not changed. */
	u_int *  percent ,	/* if percent is not specified in font_name , not changed. */
	char *  font_name	/* modified by this function. */
	)
{
	char *  p ;
	size_t  len ;
	
	/*
	 * XftFont format.
	 * [Family]( [WEIGHT] [SLANT] [SIZE]-[Encoding]:[Percentage])
	 */

	*font_family = font_name ;
	
	p = font_name ;
	while( 1)
	{
		if( *p == '\\' && *(p + 1))
		{
			/*
			 * It seems that XftFont format allows hyphens to be escaped.
			 * (e.g. Foo\-Bold-iso10646-1)
			 */

			/* skip backslash */
			p ++ ;
		}
		else if( *p == '\0')
		{
			/* encoding and percentage is not specified. */
			
			*font_name = '\0' ;
			
			break ;
		}
		else if( *p == '-')
		{
			/* Parsing "-[Encoding]:[Percentage]" */
			
			*font_name = '\0' ;

			*font_encoding = ++p ;
			
			kik_str_sep( &p , ":") ;
			if( p)
			{
				if( ! kik_str_to_uint( percent , p))
				{
				#ifdef  DEBUG
					kik_warn_printf( KIK_DEBUG_TAG
						" Percentage(%s) is illegal.\n" , p) ;
				#endif
				}
			}
			
			break ;
		}
		else if( *p == ':')
		{
			/* Parsing ":[Percentage]" */
			
			*font_name = '\0' ;
			
			if( ! kik_str_to_uint( percent , p + 1))
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG
					" Percentage(%s) is illegal.\n" , p + 1) ;
			#endif
			}
			
			break ;
		}

		*(font_name++) = *(p++) ;
	}

	/*
	 * Parsing "[Family] [WEIGHT] [SLANT] [SIZE]".
	 * Following is the same as x_font_win32.c:parse_font_name()
	 * except FC_*.
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
					int  slant ;

				} styles[] =
				{
					/*
					 * Portable styles.
					 */
					/* slant */
					{ "italic" , 0 , FC_SLANT_ITALIC , } ,
					/* weight */
					{ "bold" , FC_WEIGHT_BOLD , 0 , } ,

					/*
					 * Hack for styles which can be returned by
					 * gtk_font_selection_dialog_get_font_name().
					 */
					/* slant */
					{ "oblique" , 0 , FC_SLANT_OBLIQUE , } ,
					/* weight */
					{ "light" , /* e.g. "Bookman Old Style Light" */
						FC_WEIGHT_LIGHT , 0 , } ,
					{ "semi-bold" , FC_WEIGHT_DEMIBOLD , 0 , } ,
					{ "heavy" , /* e.g. "Arial Black Heavy" */
						FC_WEIGHT_BLACK , 0 , }	,
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
						else if( styles[count].slant)
						{
							*font_slant = styles[count].slant ;
						}

						goto  next_char ;
					}
				}
				
				if( *p != '0' &&	/* In case of "DevLys 010" font family. */
				    sscanf( p , "%f" , &size_f) == 1)
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

static u_int
get_fc_col_width(
	x_font_t *  font ,
	double  fontsize_d ,
	u_int  percent ,
	u_int  letter_space
	)
{
	if( percent > 0)
	{
		return  DIVIDE_ROUNDING(fontsize_d * font->cols * percent , 100 * 2) +
			letter_space ;
	}
	else if( font->is_var_col_width)
	{
		return  0 ;
	}
	else if( strcmp( fc_size_type , FC_SIZE) == 0)
	{
		double  widthpix ;
		double  widthmm ;
		double  dpi ;

		widthpix = DisplayWidth( font->display , DefaultScreen(font->display)) ;
		widthmm = DisplayWidthMM( font->display , DefaultScreen(font->display)) ;

		if( dpi_for_fc)
		{
			dpi = dpi_for_fc ;
		}
		else
		{
			dpi = (widthpix * 254) / (widthmm * 10) ;
		}

		return  DIVIDE_ROUNDINGUP(dpi * fontsize_d * font->cols , 72 * 2) + letter_space ;
	}
	else
	{
		return  DIVIDE_ROUNDINGUP(fontsize_d * font->cols , 2) + letter_space ;
	}
}

static FcPattern *
fc_pattern_create(
	char *  family ,	/* can be NULL */
	double  size ,
	char *  encoding ,	/* can be NULL */
	int  weight ,
	int  slant ,
	int  ch_width ,		/* can be 0 */
	int  aa_opt
	)
{
	FcPattern *  pattern ;

	if( ! ( pattern = FcPatternCreate()))
	{
		return  NULL ;
	}

	if( family)
	{
		FcPatternAddString( pattern , FC_FAMILY , family) ;
	}
	FcPatternAddDouble( pattern , fc_size_type , size) ;
	if( weight >= 0)
	{
		FcPatternAddInteger( pattern , FC_WEIGHT , weight) ;
	}
	if( slant >= 0)
	{
		FcPatternAddInteger( pattern , FC_SLANT , slant) ;
	}
	FcPatternAddInteger( pattern , FC_SPACING , ch_width > 0 ? FC_MONO : FC_PROPORTIONAL) ;
	if( ch_width > 0)
	{
		/* XXX FC_CHAR_WIDTH doesn't make effect in cairo ... */
		FcPatternAddInteger( pattern , FC_CHAR_WIDTH , ch_width) ;
	}
	if( aa_opt)
	{
		FcPatternAddBool( pattern , FC_ANTIALIAS , aa_opt == 1 ? True : False) ;
	}
	if( dpi_for_fc)
	{
		FcPatternAddDouble( pattern , FC_DPI , dpi_for_fc) ;
	}
#ifdef  USE_TYPE_XFT
	if( encoding)
	{
		FcPatternAddString( pattern , XFT_ENCODING , encoding) ;
	}
#endif
#if  0
	FcPatternAddBool( pattern , "embeddedbitmap" , True) ;
#endif

	return  pattern ;
}

#endif	/* USE_TYPE_XFT / USE_TYPE_CAIRO */

#ifdef  USE_TYPE_XFT

static u_int
xft_calculate_char_width(
	Display *  display ,
	XftFont *  xfont ,
	const u_char *  ch ,		/* US-ASCII or Unicode */
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

static XftFont *
xft_font_open(
	x_font_t *  font ,
	char *  family ,	/* can be NULL */
	double  size ,
	char *  encoding ,	/* can be NULL */
	int  weight ,
	int  slant ,
	int  ch_width ,
	int  aa_opt
	)
{
	FcPattern *  pattern ;
	FcPattern *  match ;
	FcResult  result ;
	XftFont *  xfont ;

	if( ! ( pattern = fc_pattern_create( family , size , encoding , weight , slant ,
				ch_width , aa_opt)))
	{
		return  NULL ;
	}

	match = XftFontMatch( font->display , DefaultScreen( font->display) , pattern ,
				&result) ;
	FcPatternDestroy( pattern) ;
	if( ! match)
	{
		return  NULL ;
	}
	
#if  0
	FcPatternPrint( match) ;
#endif

	if( ! ( xfont = XftFontOpenPattern( font->display , match)))
	{
		FcPatternDestroy( match) ;

		return  NULL ;
	}
	
	return  xfont ;
}

#endif

#ifdef  USE_TYPE_CAIRO

static u_int
cairo_calculate_char_width(
	cairo_scaled_font_t *  xfont ,
	const u_char *  ch ,
	size_t  len
	)
{
	u_char  utf8[UTF_MAX_SIZE + 1] ;
	cairo_text_extents_t  extents ;

	utf8[ x_convert_ucs_to_utf8( utf8 , mkf_bytes_to_int( ch , len))] = '\0' ;
	cairo_scaled_font_text_extents( xfont , utf8 , &extents) ;

#if  0
	kik_debug_printf( "%f %f %f\n" , extents.x_bearing , extents.width , extents.x_advance) ;
#endif

	return  DOUBLE_ROUNDUP_TO_INT(extents.x_advance) ;
}

static cairo_scaled_font_t *
cairo_font_open(
	x_font_t *  font ,
	char *  family ,	/* can be NULL */
	double  size ,
	char *  encoding ,	/* can be NULL */
	int  weight ,
	int  slant ,
	int  ch_width ,
	int  aa_opt
	)
{
	cairo_font_options_t *  options ;
	cairo_t *  cairo ;
	FcPattern *  pattern ;
	FcPattern *  match ;
	FcResult  result ;
	cairo_font_face_t *  font_face ;
	cairo_matrix_t  font_matrix ;
	cairo_matrix_t  ctm ;
	cairo_scaled_font_t *  xfont ;
	double  pixel_size ;

	if( ! ( pattern = fc_pattern_create( family , size , encoding , weight , slant ,
				ch_width , aa_opt)))
	{
		return  NULL ;
	}

	FcConfigSubstitute( NULL , pattern , FcMatchPattern) ;

	if( ! ( cairo = cairo_create( cairo_xlib_surface_create( font->display ,
				DefaultRootWindow( font->display) ,
				DefaultVisual( font->display , DefaultScreen( font->display)) ,
				DisplayWidth( font->display , DefaultScreen( font->display)) ,
				DisplayHeight( font->display , DefaultScreen( font->display))))))
	{
		FcPatternDestroy( pattern) ;

		return  NULL ;
	}

	options = cairo_font_options_create() ;
	cairo_get_font_options( cairo , options) ;
	/* cairo_font_options_set_antialias( options , CAIRO_ANTIALIAS_GRAY) ; */
	/* cairo_font_options_set_hint_style( options , CAIRO_HINT_STYLE_NONE) ; */
	/* CAIRO_HINT_METRICS_ON disarranges column width by boldening etc. */
	cairo_font_options_set_hint_metrics( options , CAIRO_HINT_METRICS_OFF) ;
	cairo_ft_font_options_substitute( options , pattern) ;

	FcDefaultSubstitute( pattern) ;

	if( ! ( match = FcFontMatch( NULL , pattern , &result)))
	{
		cairo_destroy( cairo) ;
		cairo_font_options_destroy( options) ;
		FcPatternDestroy( pattern) ;

		return  NULL ;
	}

#if  0
	FcPatternPrint( match) ;
#endif

	font_face = cairo_ft_font_face_create_for_pattern( match) ;

	FcPatternGetDouble( match , FC_PIXEL_SIZE , 0 , &pixel_size) ;
	
	cairo_matrix_init_identity( &font_matrix) ;
	cairo_matrix_scale( &font_matrix , pixel_size , pixel_size) ;
	cairo_get_matrix( cairo , &ctm) ;

	xfont = cairo_scaled_font_create( font_face , &font_matrix , &ctm , options) ;

	cairo_destroy( cairo) ;
	cairo_font_options_destroy( options) ;
	cairo_font_face_destroy( font_face) ;
	FcPatternDestroy( pattern) ;
	FcPatternDestroy( match) ;

	if( cairo_scaled_font_status( xfont))
	{
		cairo_scaled_font_destroy( xfont) ;

		return  NULL ;
	}

	return  xfont ;
}

#endif

#if  defined(USE_TYPE_XFT) || defined(USE_TYPE_CAIRO)

static void *
ft_font_open(
	x_font_t *  font ,
	char *  family ,
	double  size ,
	char *  encoding ,
	int  weight ,
	int  slant ,
	int  ch_width ,
	int  aa_opt ,
	int  use_xft
	)
{
	if( use_xft)
	{
	#ifdef  USE_TYPE_XFT
		return  xft_font_open( font , family , size , encoding , weight , slant ,
				ch_width , aa_opt) ;
	#endif
	}
	else
	{
	#ifdef  USE_TYPE_CAIRO
		return  cairo_font_open( font , family , size , encoding , weight , slant ,
				ch_width , aa_opt) ;
	#endif
	}

	return  NULL ;
}

static int
set_ft_font(
	x_font_t *  font ,
	const char *  fontname ,
	u_int  fontsize ,
	u_int  col_width ,	/* if usascii font wants to be set , 0 will be set. */
	int  use_medium_for_bold ,	/* Not used for now. */
	u_int  letter_space ,
	int  aa_opt ,		/* 0 = default , 1 = enable , -1 = disable */
	int  use_xft
	)
{
	char *  font_encoding ;
	int  weight ;
	int  slant ;
	u_int  ch_width ;
	void *  xfont ;

	/*
	 * encoding, weight and slant can be modified in parse_fc_font_name().
	 */
	
	font_encoding = NULL ;

	if( font->id & FONT_BOLD)
	{
		weight = FC_WEIGHT_BOLD ;
	}
	else
	{
		weight = -1 ;	/* use default value */
	}

	slant = -1 ;	/* use default value */

	if( fontname)
	{
		char *  p ;
		char *  font_family ;
		double  fontsize_d ;
		u_int  percent ;

		if( ( p = kik_str_alloca_dup( fontname)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
		#endif

			return  0 ;
		}

		fontsize_d = (double)fontsize ;
		percent = 0 ;
		if( parse_fc_font_name( &font_family , &weight , &slant , &fontsize_d ,
						&font_encoding , &percent , p))
		{
			if( col_width == 0)
			{
				/* basic font (e.g. usascii) width */

				ch_width = get_fc_col_width( font , fontsize_d , percent ,
								letter_space) ;

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
				if( font->is_var_col_width)
				{
					ch_width = 0 ;
				}
				else if( font->is_vertical)
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

		#ifdef  DEBUG
			kik_debug_printf( "Loading font %s%s%s %f %d\n" , font_family ,
				weight == XFT_WEIGHT_BOLD ? ":Bold" :
					weight == XFT_WEIGHT_LIGHT ? " Light" : "" ,
				slant == XFT_SLANT_ITALIC ? ":Italic" : "" ,
				fontsize_d , font->is_var_col_width) ;
		#endif

			if( ( xfont = ft_font_open( font , font_family , fontsize_d ,
					font_encoding , weight , slant , ch_width ,
					aa_opt , use_xft)))
			{
				goto  font_found ;
			}
		}

		kik_warn_printf( "Font %s (for size %f) couldn't be loaded.\n" ,
			fontname , fontsize_d) ;
	}

	if( col_width == 0)
	{
		/* basic font (e.g. usascii) width */

		ch_width = get_fc_col_width( font , (double)fontsize , 0 , letter_space) ;
	
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
		if( font->is_var_col_width)
		{
			ch_width = 0 ;
		}
		else if( font->is_vertical)
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

	if( ( xfont = ft_font_open( font , NULL , (double)fontsize , font_encoding ,
				weight , slant , ch_width , aa_opt , use_xft)))
	{
		goto  font_found ;
	}

#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " ft_font_open(%s) failed.\n" , fontname) ;
#endif

	return  0 ;

font_found:

#if  defined(FC_EMBOLDEN) /* Synthetic emboldening (fontconfig >= 2.3.0) */ \
	|| defined(USE_TYPE_CAIRO)
	font->is_double_drawing = 0 ;
#else
	if( weight == FC_WEIGHT_BOLD &&
	    XftPatternGetInteger( xfont->pattern , FC_WEIGHT , 0 , &weight) == XftResultMatch &&
	    weight != FC_WEIGHT_BOLD)
	{
		font->is_double_drawing = 1 ;
	}
	else
	{
		font->is_double_drawing = 0 ;
	}
#endif

	font->x_off = 0 ;

	font->is_proportional = font->is_var_col_width ;

	if( use_xft)
	{
	#ifdef  USE_TYPE_XFT
		font->xft_font = xfont ;

		font->height = font->xft_font->height ;
		font->height_to_baseline = font->xft_font->ascent ;

		if( ch_width == 0)
		{
			/*
			 * Proportional (font->is_var_col_width is true)
			 * letter_space is ignored in variable column width mode.
			 */
			font->width = xft_calculate_char_width( font->display , xfont , "W" , 1) ;
		}
		else
		{
			font->width = ch_width ;
		}
	#endif	/* USE_TYPE_XFT */
	}
	else
	{
	#ifdef  USE_TYPE_CAIRO
		cairo_font_extents_t  extents ;

		font->cairo_font = xfont ;

		cairo_scaled_font_extents( font->cairo_font , &extents) ;
		font->height = DOUBLE_ROUNDUP_TO_INT(extents.height) ;
		font->height_to_baseline = DOUBLE_ROUNDUP_TO_INT(extents.ascent) ;

		/* XXX letter_space is always ignored. */
		if( font->cols == 2)
		{
			font->width = DOUBLE_ROUNDUP_TO_INT(extents.max_x_advance) ;
		}
		else
		{
			font->width = cairo_calculate_char_width( font->cairo_font , "W" , 1) ;
		}

	#if  0
		if( col_width > 0 /* is not usascii */ && ! font->is_proportional &&
		    ch_width != font->width)
		{
			kik_warn_printf( "Font width(%d) is not matched with "
				"standard width(%d).\n" , font->width , ch_width) ;

			font->is_proportional = 1 ;

			if( font->width < ch_width)
			{
				font->x_off = (ch_width - font->width) / 2 ;
			}

			font->width = ch_width ;
		}
	#endif
	#endif	/* USE_TYPE_CAIRO */
	}

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
	const u_char *  ch ,
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

		/* XXX dealing as UCS2 */

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
	char **  percent ,	/* NULL can be returned. */
	char *  font_name	/* Don't specify NULL. Broken in this function */
	)
{
	/*
	 * XftFont format.
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
set_xfont(
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

			kik_warn_printf( "Font %s couldn't be loaded.\n" , font_xlfd) ;
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

	if( (font->id & FONT_BIWIDTH) && (FONT_CS(font->id) == ISO10646_UCS4_1) )
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
				ch_width = DIVIDE_ROUNDING( fontsize * percent , 100) ;
			}
			else
			{
				ch_width = DIVIDE_ROUNDING( fontsize * percent , 200) ;
			}

			if( font->width != ch_width)
			{
				font->is_proportional = 1 ;

				if( font->width < ch_width)
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
				kik_warn_printf( "Font width(%d) is not matched with "
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
		font->width = DIVIDE_ROUNDINGUP( fontsize * font->cols , 2) ;
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
	x_font_present_t  font_present ,
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

	if( font->id & FONT_BIWIDTH)
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

#ifdef  USE_TYPE_XCORE
	font->xfont = NULL ;
#endif
#ifdef  USE_TYPE_XFT
	font->xft_font = NULL ;
#endif
#ifdef  USE_TYPE_CAIRO
	font->cairo_font = NULL ;
#endif
	font->decsp_font = NULL ;

	switch( type_engine)
	{
	default:
		return  NULL ;

#if  defined(USE_TYPE_XFT) || defined(USE_TYPE_CAIRO)
	case  TYPE_XFT:
	case  TYPE_CAIRO:
		if( ! set_ft_font( font , fontname , fontsize , col_width , use_medium_for_bold ,
				letter_space ,
				(font_present & FONT_AA) == FONT_AA ?
					1 : ((font_present & FONT_NOAA) == FONT_NOAA ? -1 : 0) ,
				type_engine == TYPE_XFT))
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
		else if( ! set_xfont( font , fontname , fontsize , col_width ,
				use_medium_for_bold , letter_space))
		{
			free( font) ;

			return  NULL ;
		}

		break ;
#endif
	}

	if( font->is_proportional && ! font->is_var_col_width)
	{
		kik_warn_printf(
			"Characters (cs %d) are drawn *one by one* to arrange column width.\n" ,
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
#ifdef  USE_TYPE_XFT
	if( font->xft_font)
	{
		XftFontClose( font->display , font->xft_font) ;
		font->xft_font = NULL ;
	}
#endif
#ifdef  USE_TYPE_CAIRO
	if( font->cairo_font)
	{
		cairo_scaled_font_destroy( font->cairo_font) ;
		font->cairo_font = NULL ;
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
	if( font->is_var_col_width && font->is_proportional && ! font->decsp_font)
	{
	#ifdef  USE_TYPE_XFT
		if( font->xft_font)
		{
			u_char  ucs4[4] ;

			if( cs != US_ASCII && ! IS_ISCII(cs))
			{
				if( ! x_convert_to_xft_ucs4( ucs4 , ch , len , cs))
				{
					return  0 ;
				}

				ch = ucs4 ;
				len = 4 ;
			}

			return  xft_calculate_char_width( font->display , font->xft_font ,
								ch , len) ;
		}
	#endif
	#ifdef  USE_TYPE_CAIRO
		if( font->cairo_font)
		{
			u_char  ucs4[4] ;

			if( cs != US_ASCII && ! IS_ISCII(cs))
			{
				if( ! x_convert_to_xft_ucs4( ucs4 , ch , len , cs))
				{
					return  0 ;
				}

				ch = ucs4 ;
				len = 4 ;
			}

			return  cairo_calculate_char_width( font->cairo_font , ch , len) ;
		}
	#endif
	#ifdef  USE_TYPE_XCORE
		if( font->xfont)
		{
			return  calculate_char_width( font->display , font->xfont , ch , len) ;
		}
	#endif

		kik_error_printf( KIK_DEBUG_TAG " couldn't calculate correct font width.\n") ;
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

/* For mlterm-libvte */
void
x_font_use_point_size_for_fc(
	int  bool
	)
{
#if  defined(USE_TYPE_XFT) || defined(USE_TYPE_CAIRO)
	if( bool)
	{
		fc_size_type = FC_SIZE ;
	}
	else
	{
		fc_size_type = FC_PIXEL_SIZE ;
	}
#endif
}

void
x_font_set_dpi_for_fc(
	double  dpi
	)
{
#if  defined(USE_TYPE_XFT) || defined(USE_TYPE_CAIRO)
	dpi_for_fc = dpi ;
#endif
}

#if  defined(USE_TYPE_XFT) || defined(USE_TYPE_CAIRO)

static int  use_cp932_ucs_for_xft = 0 ;

static int
convert_to_ucs4(
	u_char *  ucs4_bytes ,
	const u_char *  src_bytes ,
	size_t  src_size ,
	mkf_charset_t  cs
	)
{
	if( cs == ISO10646_UCS4_1)
	{
		memcpy( ucs4_bytes , src_bytes , 4) ;
	}
	else
	{
		mkf_char_t  non_ucs ;
		mkf_char_t  ucs4 ;

		if( ml_is_msb_set( cs))
		{
			int  count ;

			for( count = 0 ; count < src_size ; count ++)
			{
				non_ucs.ch[count] = src_bytes[count] & 0x7f ;
			}
		}
		else
		{
			memcpy( non_ucs.ch , src_bytes , src_size) ;
		}
		
		non_ucs.size = src_size ;
		non_ucs.property = 0 ;
		non_ucs.cs = cs ;

		if( mkf_map_to_ucs4( &ucs4 , &non_ucs))
		{
			memcpy( ucs4_bytes , ucs4.ch , 4) ;
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
int
x_convert_to_xft_ucs4(
	u_char *  ucs4_bytes ,
	const u_char *  src_bytes ,
	size_t  src_size ,
	mkf_charset_t  cs	/* US_ASCII and ISO8859_1_R is not accepted */
	)
{
	if( cs == US_ASCII || cs == ISO8859_1_R)
	{
		return  0 ;
	}
	else if( use_cp932_ucs_for_xft && cs == JISX0208_1983)
	{
		u_int16_t  code ;

		code = mkf_bytes_to_int( src_bytes , src_size) ;

		if( code == 0x2140)
		{
			ucs4_bytes[2] = 0xff ;
			ucs4_bytes[3] = 0x3c ;
		}
		else if( code == 0x2141)
		{
			ucs4_bytes[2] = 0xff ;
			ucs4_bytes[3] = 0x5e ;
		}
		else if( code == 0x2142)
		{
			ucs4_bytes[2] = 0x22 ;
			ucs4_bytes[3] = 0x25 ;
		}
		else if( code == 0x215d)
		{
			ucs4_bytes[2] = 0xff ;
			ucs4_bytes[3] = 0x0d ;
		}
		else if( code == 0x2171)
		{
			ucs4_bytes[2] = 0xff ;
			ucs4_bytes[3] = 0xe0 ;
		}
		else if( code == 0x2172)
		{
			ucs4_bytes[2] = 0xff ;
			ucs4_bytes[3] = 0xe1 ;
		}
		else if( code == 0x224c)
		{
			ucs4_bytes[2] = 0xff ;
			ucs4_bytes[3] = 0xe2 ;
		}
		else
		{
			goto  no_diff ;
		}
		
		ucs4_bytes[0] = 0x0 ;
		ucs4_bytes[1] = 0x0 ;
		
		return  1 ;
	}

no_diff:
	return  convert_to_ucs4( ucs4_bytes , src_bytes , src_size , cs) ;
}

size_t
x_convert_ucs_to_utf8(
	u_char *  utf8 ,	/* size of utf8 should be greater than 7. */
	u_int32_t  ucs
	)
{
	/* ucs is unsigned */
	if( /* 0x00 <= ucs && */ ucs <= 0x7f)
	{
		*utf8 = ucs ;

		return  1 ;
	}
	else if( ucs <= 0x07ff)
	{
		*(utf8 ++) = ((ucs >> 6) & 0xff) | 0xc0 ;
		*utf8 = (ucs & 0x3f) | 0x80 ;

		return  2 ;
	}
	else if( ucs <= 0xffff)
	{
		*(utf8 ++) = ((ucs >> 12) & 0x0f) | 0xe0 ;
		*(utf8 ++) = ((ucs >> 6) & 0x3f) | 0x80 ;
		*utf8 = (ucs & 0x3f) | 0x80 ;

		return  3 ;
	}
	else if( ucs <= 0x1fffff)
	{
		*(utf8 ++) = ((ucs >> 18) & 0x07) | 0xf0 ;
		*(utf8 ++) = ((ucs >> 12) & 0x3f) | 0x80 ;
		*(utf8 ++) = ((ucs >> 6) & 0x3f) | 0x80 ;
		*utf8 = (ucs & 0x3f) | 0x80 ;

		return  4 ;
	}
	else if( ucs <= 0x03ffffff)
	{
		*(utf8 ++) = ((ucs >> 24) & 0x03) | 0xf8 ;
		*(utf8 ++) = ((ucs >> 18) & 0x3f) | 0x80 ;
		*(utf8 ++) = ((ucs >> 12) & 0x3f) | 0x80 ;
		*(utf8 ++) = ((ucs >> 6) & 0x3f) | 0x80 ;
		*utf8 = (ucs & 0x3f) | 0x80 ;

		return  5 ;
	}
	else if( ucs <= 0x7fffffff)
	{
		*(utf8 ++) = ((ucs >> 30) & 0x01) | 0xfc ;
		*(utf8 ++) = ((ucs >> 24) & 0x3f) | 0x80 ;
		*(utf8 ++) = ((ucs >> 18) & 0x3f) | 0x80 ;
		*(utf8 ++) = ((ucs >> 12) & 0x3f) | 0x80 ;
		*(utf8 ++) = ((ucs >> 6) & 0x3f) | 0x80 ;
		*utf8 = (ucs & 0x3f) | 0x80 ;

		return  6 ;
	}
	else
	{
		return  0 ;
	}
}

#endif	/* USE_TYPE_XFT / USE_TYPE_CAIRO */


#ifdef  DEBUG

int
x_font_dump(
	x_font_t *  font
	)
{
#ifdef  USE_TYPE_XCORE
	kik_msg_printf( "Font id %x: XFont %p" , font->id , font->xfont) ;
#endif
#ifdef  USE_TYPE_XFT
	kik_msg_printf( "Font id %x: XftFont %p" , font->id , font->xft_font) ;
#endif
#ifdef  USE_TYPE_CAIRO
	kik_msg_printf( "Font id %x: CairoFont %p" , font->id , font->cairo_font) ;
#endif

	kik_msg_printf( " (width %d, height %d, height_to_baseline %d, x_off %d)" ,
		font->width , font->height , font->height_to_baseline , font->x_off) ;

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
	
	if( font->is_double_drawing)
	{
		kik_msg_printf( " (double drawing)") ;
	}

	kik_msg_printf( "\n") ;

	return  1 ;
}

#endif
