/*
 *	$Id$
 */

#ifndef  __X_VIRTUAL_KBD_H__
#define  __X_VIRTUAL_KBD_H__


#include  "x_display.h"


int  x_virtual_kbd_start( x_display_t *  disp) ;

int  x_virtual_kbd_stop(void) ;

int  x_virtual_kbd_read( XKeyEvent *  kev , XButtonEvent *  bev) ;


#endif
