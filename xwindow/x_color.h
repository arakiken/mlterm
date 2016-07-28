/*
 *	$Id$
 */

#ifndef  __X_COLOR_H__
#define  __X_COLOR_H__


#include  <kiklib/kik_types.h>
#include  <kiklib/kik_mem.h>	/* alloca */

#include  "x_display.h"


typedef struct x_color
{
	/* Public */
	u_int32_t  pixel ;

#ifdef  X_COLOR_HAS_RGB
	/* Private except x_color_cache.c */
	u_int8_t  red ;
	u_int8_t  green ;
	u_int8_t  blue ;
	u_int8_t  alpha ;
#endif

} x_color_t ;


int  x_load_named_xcolor( x_display_t *  disp , x_color_t *  xcolor , char *  name) ;

int  x_load_rgb_xcolor( x_display_t *  disp , x_color_t *  xcolor ,
	u_int8_t  red , u_int8_t  green , u_int8_t  blue , u_int8_t  alpha) ;

int  x_unload_xcolor( x_display_t *  disp , x_color_t *  xcolor) ;

int  x_get_xcolor_rgba( u_int8_t *  red , u_int8_t *  green , u_int8_t *  blue ,
	u_int8_t *  alpha , x_color_t *  xcolor) ;

int  x_xcolor_fade( x_display_t * , x_color_t *  xcolor , u_int  fade_ratio) ;


#endif
