/*
 *	$Id$
 */

#ifndef  __X_IM_STATUS_SCREEN_H__
#define  __X_IM_STATUS_SCREEN_H__

#include  <ml_char.h>
#include  "x_window.h"
#include  "x_display.h"
#include  "x_font_manager.h"
#include  "x_color_manager.h"

typedef struct x_im_status_screen
{
	x_window_t  window ;

	x_font_manager_t *  font_man ;	/* is the same as attaced screen */

	x_color_manager_t *  color_man ;/* is the same as attaced screen */

	ml_char_t *  chars ;
	u_int  num_of_chars ; /* == array size */
	u_int  filled_len ;

	u_int  is_focused ;

	int  is_vertical ;

	/*
	 * methods of x_im_status_screen_t which is called from im
	 */
	int (*delete)( struct x_im_status_screen *) ;
	int (*show)( struct x_im_status_screen *) ;
	int (*hide)( struct x_im_status_screen *) ;
	int (*set_spot)( struct x_im_status_screen * , int  , int) ;
	int (*set)( struct x_im_status_screen * , mkf_parser_t * , u_char *) ;

} x_im_status_screen_t ;

x_im_status_screen_t * x_im_status_screen_new( x_display_t *  disp ,
					       x_font_manager_t *  font_man ,
					       x_color_manager_t *  color_man ,
					       int  is_vertical ,
					       int  x , int  y) ;

#endif

