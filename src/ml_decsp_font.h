/*
 *	$Id$
 */

#ifndef  __ML_VTGRAPHICS_H__
#define  __ML_VTGRAPHICS_H__


#include  <X11/Xlib.h>


typedef struct ml_decsp_font
{
	Pixmap  glyphs[0x20] ;
	u_int  width ;
	u_int  height ;
	u_int  height_to_baseline ;

} ml_decsp_font_t ;


ml_decsp_font_t *  ml_decsp_font_new( Display *  display , u_int  width ,
	u_int  height , u_int  height_to_baseline) ;

int  ml_decsp_font_delete( ml_decsp_font_t *  vtgr , Display *  display) ;

int  ml_decsp_font_draw_string( ml_decsp_font_t *  vtgr , Display *  display ,
	Drawable  drawable , GC  gc , int  x , int  y , u_char *  str , u_int  len) ;

int  ml_decsp_font_draw_image_string( ml_decsp_font_t *  vtgr , Display *  display ,
	Drawable  drawable , GC  gc , int  x , int  y , u_char *  str , u_int  len) ;


#endif
