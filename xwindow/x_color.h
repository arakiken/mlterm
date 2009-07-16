/*
 *	$Id$
 */

#ifndef  __X_COLOR_H__
#define  __X_COLOR_H__


#include  "x.h"

#ifdef  USE_TYPE_XFT
#include  <X11/Xft/Xft.h>
#endif

#define  RGB_WHITE  0xffffff
#define  RGB_BLACK  0x0

#ifdef  USE_TYPE_XFT
typedef XftColor  x_color_t ;
#else
typedef XColor  x_color_t ;
#endif


int  x_load_named_xcolor( Display *  display , int  screen , x_color_t *  xcolor , char *  name) ;

int  x_load_rgb_xcolor( Display *  display , int  screen , x_color_t *  xcolor ,
	u_int8_t  red , u_int8_t  green , u_int8_t  blue) ;

int  x_unload_xcolor( Display *  display , int  screen , x_color_t *  xcolor) ;

int  x_get_xcolor_rgb( u_int8_t *  red , u_int8_t *  green , u_int8_t *  blue ,
	x_color_t *  xcolor) ;

int  x_xcolor_fade( Display *  display , int  screen , x_color_t *  xcolor , u_int  fade_ratio) ;


#endif
