/*
 *	$Id$
 */

#ifndef  __ML_XIM_H__
#define  __ML_XIM_H__


#include  <mkf/mkf_parser.h>

#include  "ml_char_encoding.h"
#include  "ml_window.h"


typedef struct  ml_xim
{
	XIM  im ;
	
	char *  name ;
	char *  locale ;
	
	mkf_parser_t *  parser ;
	ml_char_encoding_t  encoding ;

	ml_window_t **  xic_wins ;
	u_int  num_of_xic_wins ;
	
} ml_xim_t ;


int  ml_xim_init( int  use_xim) ;

int  ml_xim_final(void) ;

int  ml_xim_display_opened( Display *  display) ;

int  ml_xim_display_closed( Display *  display) ;

int  ml_add_xim_listener( ml_window_t *  win , char *  xim_name , char *  xim_locale) ;

int  ml_remove_xim_listener( ml_window_t *  win) ;

XIMStyle  ml_xim_get_style( ml_window_t *  win) ;

XIC  ml_xim_create_ic( ml_window_t *  win , XIMStyle  selected_style , XVaNestedList  preedit_attr) ;

char *  ml_get_xim_name( ml_window_t *  win) ;

char *  ml_get_xim_locale( ml_window_t *  win) ;


#endif
