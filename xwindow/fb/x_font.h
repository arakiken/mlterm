/*
 *	$Id$
 */

#ifndef  ___X_FONT_H__
#define  ___X_FONT_H__


#include  "../x_font.h"


u_char *  x_get_bitmap( XFontStruct *  xfont , u_char *  ch , size_t  len) ;

u_char *  x_get_bitmap_line( XFontStruct *  xfont , u_char *  bitmap , int  y) ;

int  x_get_bitmap_cell( XFontStruct *  xfont , u_char *  bitmap_line , int  x) ;


#endif
