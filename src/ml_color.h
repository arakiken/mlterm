/*
 *	$Id$
 */

/*
 * ANTI_ALIAS macro should be concerned.
 */

#ifndef  __ML_COLOR_H__
#define  __ML_COLOR_H__


/* this must be included before including Xft.h(before XFree86-4.0.x) */
#include  <X11/Xlib.h>

#ifdef  ANTI_ALIAS
#include  <X11/Xft/Xft.h>
#endif


/* used in ml_color_manager.c/ml_window.c */
#define  MAX_ACTUAL_COLORS ML_FG_COLOR


#ifdef  ANTI_ALIAS
typedef XftColor  x_color_t ;
#else
typedef XColor  x_color_t ;
#endif

typedef x_color_t **  ml_color_table_t ;


typedef enum  ml_color
{
	ML_UNKNOWN_COLOR = -1 ,

	/* 0x0 - 0xd are color handles */

	ML_FG_COLOR = 0xe ,
	ML_BG_COLOR = 0xf ,

	MAX_COLORS ,

} ml_color_t ;


ml_color_table_t  ml_color_table_dup( ml_color_table_t  color_table) ;

int  ml_load_named_xcolor( Display *  display , int  screen , x_color_t *  xcolor , char *  name) ;

int  ml_load_rgb_xcolor( Display *  display , int  screen , x_color_t *  xcolor ,
	u_short  red , u_short  green , u_short  blue) ;

int  ml_unload_xcolor( Display *  display , int  screen , x_color_t *  xcolor) ;

int  ml_get_xcolor_rgb( u_short *  red , u_short *  green , u_short *  blue , x_color_t *  xcolor) ;


#endif
