/*
 *	$Id$
 */

#include  <ApplicationServices/ApplicationServices.h>

#include  "../x_font.h"

#include  <stdio.h>
#include  <string.h>		/* memset/strncasecmp */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>	/* kik_snprintf */
#include  <kiklib/kik_mem.h>	/* alloca */
#include  <kiklib/kik_str.h>	/* kik_str_to_int */
#include  <mkf/mkf_ucs4_map.h>
#include  <mkf/mkf_ucs_property.h>
#include  <ml_char_encoding.h>	/* x_convert_to_xft_ucs4 */

#include  "cocoa.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static int  use_point_size ;


/* --- static functions --- */

static int
parse_font_name(
	char **  font_family ,
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
	char *  font_family ;
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

	if( font_present & FONT_VAR_WIDTH || IS_ISCII(FONT_CS(font->id)))
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

		parse_font_name( &font_family , &fontsize_d , &percent , p) ;
	}
	else
	{
		/* Default font */
		font_family = "Menlo" ;
	}

	char *  orig_font_family = font_family ;

	if( ( font->id & FONT_ITALIC) && ! strcasestr( font_family , "italic"))
	{
		char *  p = alloca( strlen( font_family) + 8) ;

		sprintf( p , "%s Italic" , font_family) ;
		font_family = p ;
	}

	if( font->id & FONT_BOLD)
	{
	#if  1
		if( ! strcasestr( font_family , "Bold"))
		{
			char *  p = alloca( strlen( font_family) + 6) ;

			sprintf( p , "%s Bold" , font_family) ;
			font_family = p ;
		}
	#else
		font->double_draw_gap = 1 ;
	#endif
	}

	while( ! ( font->cg_font = cocoa_create_font( font_family)))
	{
		if( orig_font_family && (font->id & (FONT_ITALIC | FONT_BOLD)))
		{
			font_family = orig_font_family ;

			if( font->id & FONT_BOLD)
			{
				font->double_draw_gap = 1 ;
			}

			orig_font_family = NULL ;
		}
		else
		{
			free( font) ;

			return  NULL ;
		}
	}

#if  0
	kik_debug_printf( "%d %d %d %d %d\n" , CGFontGetAscent( font->cg_font) ,
		CGFontGetDescent( font->cg_font) ,
		CGFontGetLeading( font->cg_font) ,
		CGFontGetCapHeight( font->cg_font) ,
		CGFontGetXHeight( font->cg_font)) ;
#endif

	font->pointsize = fontsize ;

	float  scale = screen_get_user_space_scale_factor() ;
	if( scale > 0.0)
	{
		fontsize = fontsize * scale + 0.5 ;
	}

#if  0
	kik_debug_printf( "pont size %d -> SCALE %f -> pixel size %d\n" ,
		font->pointsize , scale , fontsize) ;
#endif

	int  ascent = CGFontGetAscent( font->cg_font) ;
	int  descent = CGFontGetDescent( font->cg_font) ;	/* minus value */
	font->height = fontsize ;
	font->ascent = (fontsize * ascent) / (ascent - descent) ;

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

			font->width = ch_width ;
		}
		else if( font->is_vertical)
		{
			/*
			 * !! Notice !!
			 * The width of full and half character font is the same.
			 */

			font->width = fontsize ;
		}
		else
		{
			font->width = fontsize * font->cols / 2 ;
		}

		font->width += letter_space ;
	}
	else
	{
		/* not a standard(usascii) font */

		if( font->is_vertical)
		{
			font->width = col_width ;
		}
		else
		{
			font->width = col_width * font->cols ;
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
#if  0
	if( font->fid)
	{
		DeleteObject(font->fid) ;
	}

	if( font->conv)
	{
		font->conv->delete( font->conv) ;
	}
#endif

	free( font) ;

#if  0
	if( display_gc)
	{
		DeleteDC( display_gc) ;
		display_gc = None ;
	}
#endif

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
