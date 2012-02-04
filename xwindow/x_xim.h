/*
 *	$Id$
 */

#ifndef  __X_XIM_H__
#define  __X_XIM_H__


#include  <mkf/mkf_parser.h>
#include  <ml_char_encoding.h>

#include  "x_window.h"


typedef struct  x_xim
{
	XIM  im ;
	
	char *  name ;
	char *  locale ;
	
	mkf_parser_t *  parser ;
	ml_char_encoding_t  encoding ;

	x_window_t **  xic_wins ;
	u_int  num_of_xic_wins ;
	
} x_xim_t ;


int  x_xim_init( int  use_xim) ;

int  x_xim_final(void) ;

int  x_xim_display_opened( Display *  display) ;

int  x_xim_display_closed( Display *  display) ;

int  x_add_xim_listener( x_window_t *  win , char *  xim_name , char *  xim_locale) ;

int  x_remove_xim_listener( x_window_t *  win) ;

XIMStyle  x_xim_get_style( x_window_t *  win) ;

XIC  x_xim_create_ic( x_window_t *  win , XIMStyle  selected_style , XVaNestedList  preedit_attr) ;

char *  x_get_xim_name( x_window_t *  win) ;

char *  x_get_xim_locale( x_window_t *  win) ;

char *  x_get_default_xim_name(void) ;


#endif
