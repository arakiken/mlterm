/*
 *	$Id$
 */

#ifndef  __X_TERM_H__
#define  __X_TERM_H__


#include  <ml_term_manager.h>

#include  "x_display.h"
#include  "x_font_manager.h"
#include  "x_color_manager.h"
#include  "x_window_manager.h"


typedef struct x_term
{
	x_display_t *  display ;
	
	x_window_t *  root_window ;
	x_font_manager_t *  font_man ;
	x_color_manager_t *  color_man ;

	ml_term_t *  term ;

} x_term_t ;


int  x_term_init( x_term_t *  xterm , x_display_t *  display , x_window_t *  root ,
	x_font_manager_t *  font_man , x_color_manager_t *  color_man , ml_term_t *  term) ;

int  x_term_final( x_term_t *  xterm) ;


#endif
