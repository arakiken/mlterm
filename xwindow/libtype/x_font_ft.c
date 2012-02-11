/*
 *	$Id$
 */

#include  "../x_font.h"

#ifdef  USE_TYPE_XFT
#include  <X11/Xft/Xft.h>
#endif
#ifdef  USE_TYPE_CAIRO
#include  <cairo/cairo.h>
#include  <cairo/cairo-ft.h>	/* FcChar32 */
#include  <cairo/cairo-xlib.h>
#endif
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>	/* alloca */
#include  <kiklib/kik_str.h>	/* kik_str_sep/kik_str_to_int/memset/strncasecmp */
#include  <mkf/mkf_char.h>	/* mkf_bytes_to_int */
#include  <ml_char.h>		/* UTF_MAX_SIZE */


#define  DIVIDE_ROUNDING(a,b)  ( ((int)((a)*10 + (b)*5)) / ((int)((b)*10)) )
#define  DIVIDE_ROUNDINGUP(a,b) ( ((int)((a)*10 + (b)*10 - 1)) / ((int)((b)*10)) )

/* Be careful not to round down 5.99999... to 5 */
#define  DOUBLE_ROUNDUP_TO_INT(a)  ((int)((a) + 0.9))

/*
 * XXX
 * cairo always uses double drawing fow now, because width of normal font is not
 * always the same as that of bold font in cairo.
 */
#if  1
#define  CAIRO_FORCE_DOUBLE_DRAWING
#endif

#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static const char *  fc_size_type = FC_PIXEL_SIZE ;
static double  dpi_for_fc ;


/* --- static functions --- */

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
		double  dpi ;

		if( dpi_for_fc)
		{
			dpi = dpi_for_fc ;
		}
		else
		{
			double  widthpix ;
			double  widthmm ;

			widthpix = DisplayWidth( font->display , DefaultScreen(font->display)) ;
			widthmm = DisplayWidthMM( font->display , DefaultScreen(font->display)) ;

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
#ifdef  USE_TYPE_XFT
	if( ch_width > 0)
	{
		/* XXX FC_CHAR_WIDTH doesn't make effect in cairo ... */
		FcPatternAddInteger( pattern , FC_CHAR_WIDTH , ch_width) ;
	}
#endif
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


#ifdef  USE_TYPE_XFT

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
	int  pixel_size2 ;

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
#ifndef  CAIRO_FORCE_DOUBLE_DRAWING
	/*
	 * XXX
	 * CAIRO_HINT_METRICS_OFF has bad effect, but CAIRO_HINT_METRICS_ON disarranges
	 * column width by boldening etc.
	 */
	cairo_font_options_set_hint_metrics( options , CAIRO_HINT_METRICS_OFF) ;
#else
	/* For performance */
	cairo_font_options_set_hint_style( options , CAIRO_HINT_STYLE_NONE) ;
#endif
#if  0
	cairo_font_options_set_antialias( options , CAIRO_ANTIALIAS_SUBPIXEL) ;
	cairo_font_options_set_subpixel_order( options , CAIRO_SUBPIXEL_ORDER_RGB) ;
#endif
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
	/*
	 * 10.5 / 2.0 = 5.25 ->(roundup) 6 -> 6 * 2 = 12
	 * 11.5 / 2.0 = 5.75 ->(roundup) 6 -> 6 * 2 = 12
	 *
	 * If half width is 5.25 -> 6 and full width is 5.25 * 2 = 10.5 -> 11,
	 * half width char -> x_bearing = 1 / width 5
	 * full width char -> x_bearing = 1 / width 10.
	 * This results in gap between chars.
	 */
	pixel_size2 = DIVIDE_ROUNDINGUP(pixel_size,2.0) * 2 ;

	cairo_matrix_init_identity( &font_matrix) ;
	cairo_matrix_scale( &font_matrix , pixel_size2 , pixel_size2) ;
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

u_int  xft_calculate_char_width( x_font_t *  font , const u_char *  ch , size_t  len) ;
u_int  cairo_calculate_char_width( x_font_t *  font , const u_char *  ch , size_t  len) ;

static int
fc_set_font(
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

	if(
	#ifdef  CAIRO_FORCE_DOUBLE_DRAWING
	    use_xft &&
	#endif
	    (font->id & FONT_BOLD))
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
				weight == FC_WEIGHT_BOLD ? ":Bold" :
					weight == FC_WEIGHT_LIGHT ? " Light" : "" ,
				slant == FC_SLANT_ITALIC ? ":Italic" : "" ,
				fontsize_d , font->is_var_col_width) ;
		#endif

			if( ( xfont = ft_font_open( font , font_family , fontsize_d ,
					font_encoding , weight , slant , ch_width ,
					aa_opt , use_xft)))
			{
				goto  font_found ;
			}
		}

		kik_msg_printf( "Font %s (for size %f) couldn't be loaded.\n" ,
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

	font->x_off = 0 ;
	font->is_proportional = font->is_var_col_width ;

	if( use_xft)
	{
	#ifdef  USE_TYPE_XFT
	#if  defined(FC_EMBOLDEN) /* Synthetic emboldening (fontconfig >= 2.3.0) */
		font->is_double_drawing = 0 ;
	#else	/* FC_EMBOLDEN */
		if( weight == FC_WEIGHT_BOLD &&
		    XftPatternGetInteger( xfont->pattern , FC_WEIGHT , 0 , &weight) ==
			XftResultMatch &&
		    weight != FC_WEIGHT_BOLD)
		{
			font->is_double_drawing = 1 ;
		}
		else
		{
			font->is_double_drawing = 0 ;
		}
	#endif	/* FC_EMBOLDEN */

		font->xft_font = xfont ;

		font->height = font->xft_font->height ;
		font->height_to_baseline = font->xft_font->ascent ;

		if( ch_width == 0)
		{
			/*
			 * Proportional (font->is_var_col_width is true)
			 * letter_space is ignored in variable column width mode.
			 */
			font->width = xft_calculate_char_width( font , "W" , 1) ;
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

	#ifdef  CAIRO_FORCE_DOUBLE_DRAWING
		if( font->id & FONT_BOLD)
		{
			font->is_double_drawing = 1 ;
		}
		else
	#endif
		{
			font->is_double_drawing = 0 ;
		}

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
			font->width = cairo_calculate_char_width( font , "W" , 1) ;
		}

		if( col_width > 0 /* is not usascii */ && ! font->is_proportional &&
		    ch_width != font->width)
		{
			kik_msg_printf( "Font(id %x) width(%d) is not matched with "
				"standard width(%d).\n" , font->id , font->width , ch_width) ;

			/*
			 * XXX
			 * Surrounded by #if 0 ... #endif,
			 * because the case like ch_width = 12 and
			 * extents.max_x_advance = 12.28(dealt as 13 though should be dealt as 12)
			 * happens.
			 */
		#if  0
			font->is_proportional = 1 ;

			if( font->width < ch_width)
			{
				font->x_off = (ch_width - font->width) / 2 ;
			}
		#endif

			font->width = ch_width ;
		}
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

	return  1 ;
}


/* --- global functions --- */

#ifdef  USE_TYPE_XFT

int
xft_set_font(
	x_font_t *  font ,
	const char *  fontname ,
	u_int  fontsize ,
	u_int  col_width ,		/* if usascii font wants to be set , 0 will be set. */
	int  use_medium_for_bold ,	/* Not used for now. */
	u_int  letter_space ,
	int  aa_opt ,			/* 0 = default , 1 = enable , -1 = disable */
	int  use_point_size ,
	double  dpi
	)
{
	if( use_point_size)
	{
		fc_size_type = FC_SIZE ;
	}
	else
	{
		fc_size_type = FC_PIXEL_SIZE ;
	}
	
	dpi_for_fc = dpi ;

	return  fc_set_font( font , fontname , fontsize , col_width , use_medium_for_bold ,
			letter_space , aa_opt , 1) ;
}

int
xft_unset_font(
	x_font_t *  font
	)
{
	XftFontClose( font->display , font->xft_font) ;
	font->xft_font = NULL ;

	return  1 ;
}

u_int
xft_calculate_char_width(
	x_font_t *  font ,
	const u_char *  ch ,		/* US-ASCII or Unicode */
	size_t  len
	)
{
	XGlyphInfo  extents ;

	if( len == 1)
	{
		XftTextExtents8( font->display , font->xft_font , ch , 1 , &extents) ;
	}
#if  0
	else if( len == 2)
	{
		FcChar16  xch ;

		xch = ((ch[0] << 8) & 0xff00) | (ch[1] & 0xff) ;

		XftTextExtents16( font->display , font->xft_font , &xch , 1 , &extents) ;
	}
#endif
	else if( len == 4)
	{
		FcChar32  xch ;

		xch = ((ch[0] << 24) & 0xff000000) | ((ch[1] << 16) & 0xff0000) |
			((ch[2] << 8) & 0xff00) | (ch[3] & 0xff) ;

		XftTextExtents32( font->display , font->xft_font , &xch , 1 , &extents) ;
	}
	else
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " char size %d is too large.\n" , len) ;
	#endif

		return  0 ;
	}

	if( extents.xOff < 0)
	{
		/* Some (indic) fonts could return minus value as text width. */
		return  0 ;
	}
	else
	{
		return  extents.xOff ;
	}
}

#endif

#ifdef  USE_TYPE_CAIRO

int
cairo_set_font(
	x_font_t *  font ,
	const char *  fontname ,
	u_int  fontsize ,
	u_int  col_width ,		/* if usascii font wants to be set , 0 will be set. */
	int  use_medium_for_bold ,	/* Not used for now. */
	u_int  letter_space ,
	int  aa_opt ,			/* 0 = default , 1 = enable , -1 = disable */
	int  use_point_size ,
	double  dpi
	)
{
	if( use_point_size)
	{
		fc_size_type = FC_SIZE ;
	}
	else
	{
		fc_size_type = FC_PIXEL_SIZE ;
	}
	dpi_for_fc = dpi ;

	return  fc_set_font( font , fontname , fontsize , col_width , use_medium_for_bold ,
			letter_space , aa_opt , 0) ;
}

int
cairo_unset_font(
	x_font_t *  font
	)
{
	cairo_scaled_font_destroy( font->cairo_font) ;
	font->cairo_font = NULL ;

	return  1 ;
}

size_t
x_convert_ucs4_to_utf8(
	u_char *  utf8 ,	/* size of utf8 should be greater than 5. */
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

u_int
cairo_calculate_char_width(
	x_font_t *  font ,
	const u_char *  ch ,
	size_t  len
	)
{
	u_char  utf8[UTF_MAX_SIZE + 1] ;
	cairo_text_extents_t  extents ;
	int  width ;

	utf8[ x_convert_ucs4_to_utf8( utf8 , mkf_bytes_to_int( ch , len))] = '\0' ;
	cairo_scaled_font_text_extents( font->cairo_font , utf8 , &extents) ;

#if  0
	kik_debug_printf( KIK_DEBUG_TAG " CHAR(%x) x_bearing %f width %f x_advance %f\n" ,
		ch[0] , extents.x_bearing , extents.width , extents.x_advance) ;
#endif

	if( ( width = DOUBLE_ROUNDUP_TO_INT(extents.x_advance)) < 0)
	{
		return  0 ;
	}
	else
	{
		/* Some (indic) fonts could return minus value as text width. */
		return  width ;
	}
}

#endif
