/*
 *	$Id$
 */

#include  "x_font.h"

#include  <stdio.h>
#include  <fcntl.h>	/* open */
#include  <unistd.h>	/* close */
#include  <sys/mman.h>	/* mmap */
#include  <string.h>	/* memcmp */
#include  <sys/stat.h>	/* fstat */
#include  <utime.h>	/* utime */

#include  <kiklib/kik_def.h>	/* WORDS_BIGENDIAN */
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_path.h>	/* kik_basename */
#include  <kiklib/kik_conf_io.h>/* kik_get_user_rc_path */
#include  <kiklib/kik_util.h>	/* TOINT32 */
#include  <mkf/mkf_char.h>
#ifdef  __ANDROID__
#include  <dirent.h>
#endif

#ifdef  USE_OT_LAYOUT
#include  <otl.h>
#endif

#include  "x_display.h"

#define  DIVIDE_ROUNDING(a,b)  ( ((int)((a)*10 + (b)*5)) / ((int)((b)*10)) )
#define  DIVIDE_ROUNDINGUP(a,b) ( ((int)((a)*10 + (b)*10 - 1)) / ((int)((b)*10)) )

#ifdef  WORDS_BIGENDIAN
#define  _TOINT32(p,is_be) ((is_be) ? TOINT32(p) : LE32DEC(p))
#define  _TOINT16(p,is_be) ((is_be) ? TOINT16(p) : LE16DEC(p))
#else
#define  _TOINT32(p,is_be) ((is_be) ? BE32DEC(p) : TOINT32(p))
#define  _TOINT16(p,is_be) ((is_be) ? BE16DEC(p) : TOINT16(p))
#endif

#define  PCF_PROPERTIES		(1<<0)
#define  PCF_ACCELERATORS	(1<<1)
#define  PCF_METRICS		(1<<2)
#define  PCF_BITMAPS		(1<<3)
#define  PCF_INK_METRICS	(1<<4)
#define  PCF_BDF_ENCODINGS	(1<<5)
#define  PCF_SWIDTHS		(1<<6)
#define  PCF_GLYPH_NAMES	(1<<7)
#define  PCF_BDF_ACCELERATORS	(1<<8)

#define  MAX_GLYPH_TABLES  512
#define  GLYPH_TABLE_SIZE  128
#define  INITIAL_GLYPH_INDEX_TABLE_SIZE  0x1000


#if  0
#define  __DEBUG
#endif


/* --- global functions --- */

int
x_compose_dec_special_font(void)
{
	/* Do nothing for now in fb. */
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

 	if( type_engine != TYPE_XCORE || ! ( font = calloc( 1 , sizeof(x_font_t))))
	{
		return  NULL ;
	}

	if( size_attr >= DOUBLE_HEIGHT_TOP)
	{
		fontsize *= 2 ;
		col_width *= 2 ;
	}

	if( ! ( font->xfont = calloc( 1 , sizeof(XFontStruct))))
	{
		free( font) ;

		return  NULL ;
	}

	font->display = display ;
	font->xfont = NULL ;

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
		/*
		 * If you use fixed-width fonts whose width is differnet from
		 * each other.
		 */
		font->is_var_col_width = 1 ;
	}

	if( font_present & FONT_VERTICAL)
	{
		font->is_vertical = 1 ;
	}

	if( use_medium_for_bold)
	{
		font->double_draw_gap = 1 ;
	}

	font->width = font->display->col_width * font->cols ;
	font->height = font->display->line_height ;
	font->ascent = 0 ;

	if( col_width == 0)
	{
		/* standard(usascii) font */

		if( font->is_vertical)
		{
			/*
			 * !! Notice !!
			 * The width of full and half character font is the same.
			 */

			font->x_off = font->width / 2 ;
			font->width *= 2 ;
		}

		if( letter_space > 0)
		{
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

				/* is_var_col_width is always false if is_vertical is true. */
			#if  0
				if( ! font->is_var_col_width)
			#endif
				{
					if( font->width < col_width)
					{
						font->x_off = (col_width - font->width) / 2 ;
					}

					font->width = col_width ;
				}
			}
		}
		else
		{
			if( font->width != col_width * font->cols)
			{
				kik_msg_printf( "Font(id %x) width(%d) is not matched with "
					"standard width(%d).\n" ,
					font->id , font->width , col_width * font->cols) ;

				if( ! font->is_var_col_width)
				{
					if( font->width < col_width * font->cols)
					{
						font->x_off = (col_width * font->cols -
								font->width) / 2 ;
					}

					font->width = col_width * font->cols ;
				}
			}
		}
	}


	if( size_attr == DOUBLE_WIDTH)
	{
		font->x_off += (font->width / 2) ;
		font->width *= 2 ;
	}

	font->size_attr = size_attr ;


	/*
	 * checking if font width/height/ascent member is sane.
	 */

	if( font->width == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " font width is 0.\n") ;
	#endif

		/* XXX this may be inaccurate. */
		font->width = DIVIDE_ROUNDINGUP( fontsize * font->cols , 2) ;
	}

	if( font->height == 0)
	{
		/* XXX this may be inaccurate. */
		font->height = fontsize ;
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

#if  defined(USE_FREETYPE)
	if( font->xfont->is_aa && font->is_proportional)
	{
		u_char *  glyph ;

		if( ( glyph = get_ft_bitmap( font->xfont , ch ,
			#ifdef  USE_OT_LAYOUT
				(font->use_ot_layout /* && font->ot_font */)
			#else
				0
			#endif
				)))
		{
			return  glyph[font->xfont->glyph_size - 3] ;
		}
	}
#endif

	return  font->width ;
}

/* Return written size */
size_t
x_convert_ucs4_to_utf16(
	u_char *  dst ,	/* 4 bytes. Big endian. */
	u_int32_t  src
	)
{
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
	kik_msg_printf( "Font id %x: XFont %p (width %d, height %d, ascent %d, x_off %d)" ,
		font->id , font->xfont , font->width , font->height , font->ascent , font->x_off) ;

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
