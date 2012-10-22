/*
 *	$Id$
 */

#ifndef  ___X_DISPLAY_H__
#define  ___X_DISPLAY_H__


#include  "../x_display.h"


Cursor  x_display_get_cursor( x_display_t *  disp , u_int  shape) ;

XVisualInfo *  x_display_get_visual_info( x_display_t *  disp) ;


#endif
