/*
 *	$Id$
 */

#ifndef  __X_DISPLAY_H__
#define  __X_DISPLAY_H__


#include  <X11/Xlib.h>

#include  "x_window_manager.h"


typedef struct x_display
{
	char *  name ;
	Display *  display ;
	
	x_window_manager_t  win_man ;

} x_display_t ;


x_display_t *  x_display_open( char *  name) ;

int  x_display_close( x_display_t *  disp) ;

int  x_display_close_2( Display *  display) ;

int  x_display_close_all(void) ;

x_display_t **  x_get_opened_displays( u_int *  num) ;

int  x_display_fd( x_display_t *  disp) ;


#endif
