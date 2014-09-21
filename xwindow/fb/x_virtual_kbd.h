/*
 *	$Id$
 */

#ifndef  __X_VIRTUAL_KBD_H__
#define  __X_VIRTUAL_KBD_H__


#include  "x_display.h"
#include  "../x_window.h"


int  x_virtual_kbd_hide(void) ;

int  x_is_virtual_kbd_event( x_display_t *  disp , XButtonEvent *  bev) ;

int  x_virtual_kbd_read( XKeyEvent *  kev , XButtonEvent *  bev) ;

x_window_t *  x_is_virtual_kbd_area( int  y) ;


#endif
