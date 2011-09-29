/*
 *	$Id$
 */

#ifndef  __X_DRAW_STR_H__
#define  __X_DRAW_STR_H__

#include  <ml_char.h>
#include  "x_window.h"
#include  "x_font_manager.h"
#include  "x_color_manager.h"


int x_draw_str( x_window_t *  window , x_font_manager_t *  font_man ,
	x_color_manager_t *  color_man , ml_char_t *  chars ,
	u_int	num_of_chars , int  x , int  y , u_int  height ,
	u_int  height_to_baseline , u_int  top_margin , u_int  bottom_margin) ;

int x_draw_str_to_eol( x_window_t *  window , x_font_manager_t *  font_man ,
	x_color_manager_t *  color_man , ml_char_t *  chars ,
	u_int  num_of_chars , int  x , int  y , u_int  height ,
	u_int  height_to_baseline , u_int  top_margin , u_int  bottom_margin) ;


#endif
