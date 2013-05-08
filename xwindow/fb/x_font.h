/*
 *	$Id$
 */

#ifndef  ___X_FONT_H__
#define  ___X_FONT_H__


#include  "../x_font.h"


u_char *  x_get_bitmap( XFontStruct *  xfont , u_char *  ch , size_t  len) ;


#ifdef  USE_X_GET_BITMAP_LINE

#include  <kiklib/kik_types.h>	/* inline */

static inline u_char *
x_get_bitmap_line(
	XFontStruct *  xfont ,
	u_char *  bitmap ,
	int  y		/* y < xfont->height must be checked by the caller. */
	)
{
	u_char *  line ;

	if( bitmap &&
	    /* xfont->glyph_width_bytes is 1, 2 or 4 bytes */
	    memcmp( ( line = bitmap + y * xfont->glyph_width_bytes) ,
			"\x0\x0\x0" ,	/* 4 bytes */
			xfont->glyph_width_bytes) != 0)
	{
		return  line ;
	}
	else
	{
		return  NULL ;
	}
}

#endif	/* USE_X_GET_BITMAP_LINE */

#ifdef  USE_X_GET_BITMAP_CELL

#include  <kiklib/kik_types.h>	/* inline */

static inline int
x_get_bitmap_cell(
	XFontStruct *  xfont ,
	u_char *  bitmap_line ,	/* bitmap_line != NULL must be checked by the caller. */
	int  x			/* x < xfont->width must be checked by the caller. */
	)
{
	if( xfont->glyphs_same_bitorder) /* XXX ? */
	{
		/* x & 7 == x % 8 */
		return  bitmap_line[x / 8] & (1 << (x & 7)) ;
	}
	else
	{
		/* x & 7 == x % 8 */
		return  bitmap_line[x / 8] & (1 << (8 - (x & 7) - 1)) ;
	}
}

#endif	/* USE_X_GET_BITMAP_CELL */

#endif
