/*
 *	$Id$
 */

#ifndef  __X_DECSP_FONT_H__
#define  __X_DECSP_FONT_H__


#include  "../x.h"


typedef struct x_decsp_font
{
	Pixmap  glyphs[0x20] ;
	u_int  width ;
	u_int  height ;
	u_int  ascent ;

} x_decsp_font_t ;


x_decsp_font_t *  x_decsp_font_new( Display *  display , u_int  width ,
	u_int  height , u_int  ascent) ;

int  x_decsp_font_delete( x_decsp_font_t *  vtgr , Display *  display) ;

int  x_decsp_font_draw_string( x_decsp_font_t *  vtgr , Display *  display ,
	Drawable  drawable , GC  gc , int  x , int  y , u_char *  str , u_int  len) ;

int  x_decsp_font_draw_image_string( x_decsp_font_t *  font , Display *  display ,
	Drawable  drawable , GC  gc , int  x , int  y , u_char *  str , u_int  len) ;


#endif
