/*
 *	$Id$
 */

#include  "x_term.h"


/* --- global functions --- */

int
x_term_init(
	x_term_t *  xterm ,
	x_display_t *  display ,
	x_window_t *  root ,
	x_font_manager_t *  font_man ,
	x_color_manager_t *  color_man ,
	ml_term_t *  term
	)
{
	xterm->display = display ;
	xterm->root_window = root ;
	xterm->font_man = font_man ;
	xterm->color_man = color_man ;
	xterm->term = term ;

	return  1 ;
}

int
x_term_final(
	x_term_t *  xterm
	)
{
	x_window_manager_remove_root( &xterm->display->win_man , xterm->root_window) ;
	x_font_manager_delete( xterm->font_man) ;
	x_color_manager_delete( xterm->color_man) ;

	if( xterm->display->win_man.num_of_roots == 0)
	{
		x_display_close( xterm->display) ;
	}

	if( xterm->term)
	{
		ml_put_back_term( xterm->term) ;
	}

	return  1 ;
}
