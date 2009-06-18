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

/* RGB are 8bits, not 16 bits. */
int  x_load_rgb_xcolor( Display *  display , int  screen , x_color_t *  xcolor ,
	u_short  red , u_short  green , u_short  blue) ;

int  x_unload_xcolor( Display *  display , int  screen , x_color_t *  xcolor) ;

/* RGB are 8bits, not 16 bits. */
int  x_get_xcolor_rgb( u_short *  red , u_short *  green , u_short *  blue , x_color_t *  xcolor) ;

int  x_xcolor_fade( Display *  display , int  screen , x_color_t *  xcolor , u_int  fade_ratio) ;


#endif
