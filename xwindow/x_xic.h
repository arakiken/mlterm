/*
 *	$Id$
 */

#ifndef  __X_XIC_H__
#define  __X_XIC_H__


#include  <X11/Xlib.h>
#include  <kiklib/kik_types.h>		/* size_t */

#include  "x_window.h"


typedef struct  x_xic
{
	XIC   ic ;
	XFontSet  fontset ;
	XIMStyle  style ;
	
} x_xic_t ;


int  x_xic_activate( x_window_t *  win , char *  name , char *  locale) ;

int  x_xic_deactivate( x_window_t *  win) ;

char *  x_xic_get_xim_name( x_window_t *  win) ;

int  x_xic_fg_color_changed( x_window_t *  win) ;

int  x_xic_bg_color_changed( x_window_t *  win) ;

int  x_xic_font_set_changed( x_window_t *  win) ;

int  x_xic_resized( x_window_t *  win) ;

int  x_xic_set_spot( x_window_t *  win) ;

size_t  x_xic_get_str( x_window_t *  win , u_char *  seq , size_t  seq_len ,
	mkf_parser_t **  parser , KeySym *  keysym , XKeyEvent *  event) ;

size_t  x_xic_get_utf8_str( x_window_t *  win , u_char *  seq , size_t  seq_len ,
	mkf_parser_t **  parser , KeySym *  keysym , XKeyEvent *  event) ;

int  x_xic_set_focus( x_window_t *  win) ;

int  x_xic_unset_focus( x_window_t *  win) ;


int  x_xim_activated( x_window_t *  win) ;

int  x_xim_destroyed( x_window_t *  win) ;


#endif
