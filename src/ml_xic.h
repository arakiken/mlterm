/*
 *	$Id$
 */

#ifndef  __ML_XIC_H__
#define  __ML_XIC_H__


#include  <X11/Xlib.h>

#include  "ml_window.h"


typedef struct  ml_xic
{
	XIC   ic ;
	XFontSet  fontset ;
	XIMStyle  style ;
	
} ml_xic_t ;


int  ml_use_xim( ml_window_t *  win , ml_xim_event_listener_t *  xim_listener) ;

int  ml_xic_activate( ml_window_t *  win , char *  name , char *  locale) ;

int  ml_xic_deactivate( ml_window_t *  win) ;

int  ml_xim_activated( ml_window_t *  win) ;

int  ml_xim_destroyed( ml_window_t *  win) ;
	
char *  ml_xic_get_xim_name( ml_window_t *  win) ;

int  ml_xic_fg_color_changed( ml_window_t *  win) ;

int  ml_xic_bg_color_changed( ml_window_t *  win) ;

int  ml_xic_font_set_changed( ml_window_t *  win) ;

int  ml_xic_resized( ml_window_t *  win) ;

int  ml_xic_set_spot( ml_window_t *  win) ;

size_t  ml_xic_get_str(	ml_window_t *  win , u_char *  seq , size_t  seq_len ,
	mkf_parser_t **  parser , KeySym *  keysym , XKeyEvent *  event) ;

int  ml_xic_set_focus( ml_window_t *  win) ;

int  ml_xic_unset_focus( ml_window_t *  win) ;


#endif
