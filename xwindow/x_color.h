/*
 *	$Id$
 */

#ifndef  __X_COLOR_H__
#define  __X_COLOR_H__


#include  <kiklib/kik_types.h>
#ifdef  USE_TYPE_XFT
#include  <X11/Xft/Xft.h>
#endif

#include  "x.h"


#ifdef  USE_WIN32GUI

typedef struct x_color
{
	/* Public */
	u_long  pixel ;

	/* Private except x_color_cache.c */
	u_int8_t  is_loaded ;

} x_color_t ;

#else

typedef struct x_color
{
	/* Public */
	u_long  pixel ;

	/* Private except x_color_cache.c */
	u_int8_t  red ;
	u_int8_t  green ;
	u_int8_t  blue ;
	
	u_int8_t  is_loaded ;

} x_color_t ;

#endif


int  x_load_named_xcolor( Display *  display , int  screen , x_color_t *  xcolor , char *  name) ;

int  x_load_rgb_xcolor( Display *  display , int  screen , x_color_t *  xcolor ,
	u_int8_t  red , u_int8_t  green , u_int8_t  blue) ;

int  x_unload_xcolor( Display *  display , int  screen , x_color_t *  xcolor) ;

#ifdef  USE_TYPE_XFT
void  x_color_to_xft( XftColor *  xftcolor , x_color_t *  xcolor) ;
#endif

int  x_get_xcolor_rgb( u_int8_t *  red , u_int8_t *  green , u_int8_t *  blue ,
	x_color_t *  xcolor) ;

int  x_xcolor_fade( Display *  display , int  screen , x_color_t *  xcolor , u_int  fade_ratio) ;


#endif
