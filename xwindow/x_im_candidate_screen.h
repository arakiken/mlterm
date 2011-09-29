/*
 *	$Id$
 */

#ifndef  __X_IM_CANDIDATE_SCREEN_H__
#define  __X_IM_CANDIDATE_SCREEN_H__

#include  <ml_char.h>
#include  "x_window.h"
#include  "x_display.h"
#include  "x_font_manager.h"
#include  "x_color_manager.h"

typedef struct x_im_candidate
{
	u_short  info ; /* to store misc. info from IM plugins */

	ml_char_t *  chars ;
	u_int  num_of_chars ; /* == array size */
	u_int  filled_len ;

} x_im_candidate_t ;

typedef struct x_im_candidate_event_listener
{
	void *  self ;
	void (*selected)( void *  p , u_int  index) ;

}  x_im_candidate_event_listener_t ;

typedef struct x_im_candidate_screen
{
	x_window_t  window ;

	x_font_manager_t *  font_man ;		/* same as attaced screen */

	x_color_manager_t *  color_man ;	/* same as attaced screen */

	x_im_candidate_t *  candidates ;
	u_int  num_of_candidates ;		/* == array size          */

	u_int  num_per_window ;

	u_int  index ;		/* current selected index of candidates   */

	u_int  is_focused ;

	u_int  line_height ;	/* line height of attaced screen          */

	int  is_vertical_term ;
	int  is_vertical_direction ;

	/* x_im_candidate_screen.c -> im plugins */
	x_im_candidate_event_listener_t  listener ;

	/*
	 * methods for x_im_candidate_screen_t which is called from im
	 */
	int (*delete)( struct x_im_candidate_screen *) ;
	int (*show)( struct x_im_candidate_screen *) ;
	int (*hide)( struct x_im_candidate_screen *) ;
	int (*set_spot)( struct x_im_candidate_screen * , int  , int) ;
	int (*init)( struct x_im_candidate_screen * , u_int , u_int) ;
	int (*set)( struct x_im_candidate_screen * ,
		    mkf_parser_t * , u_char * , u_int) ;
	int (*select)( struct x_im_candidate_screen *  cand_screen , u_int) ;

} x_im_candidate_screen_t ;

x_im_candidate_screen_t * x_im_candidate_screen_new(
					x_display_t *  disp ,
					x_font_manager_t *  font_man ,
					x_color_manager_t *  color_man ,
					int  is_vertical_term ,
					int  is_vertical_direction ,
					u_int  line_height_of_screen ,
					int  x , int  y) ;

#endif

