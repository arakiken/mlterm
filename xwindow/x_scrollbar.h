/*
 *	$Id$
 */

#ifndef  __X_SCROLLBAR_H__
#define  __X_SCROLLBAR_H__


#include  <kiklib/kik_types.h>		/* u_int */
#include  <ml_term.h>

#include  "x_window.h"
#include  "x_sb_view.h"
#include  "x_sb_mode.h"
#include  "x_color_manager.h"


typedef struct  x_scrollbar_event_listener
{
	void *  self ;
	int  (*screen_scroll_to)( void * , int) ;
	int  (*screen_scroll_upward)( void * , u_int) ;
	int  (*screen_scroll_downward)( void * , u_int) ;
	int  (*screen_is_static)( void *) ;

} x_scrollbar_event_listener_t ;

typedef struct  x_scrollbar
{
	x_window_t  window ;

	char *  view_name ;
	x_sb_view_t *  view ;

	char *  fg_color ;
	char *  bg_color ;
	x_color_t  fg_xcolor ;
	x_color_t  bg_xcolor ;
	
	x_scrollbar_event_listener_t *  sb_listener ;

	u_int  bar_height ;
	u_int  top_margin ;
	u_int  bottom_margin ;
	u_int  line_height ;
	u_int  num_of_scr_lines ;
	u_int  num_of_log_lines ;
	u_int  num_of_filled_log_lines ;
	int  bar_top_y ;
	int  y_on_bar ;
	int  current_row ;

	int  up_button_y ;
	u_int  up_button_height ;
	int  down_button_y ;
	u_int  down_button_height ;
	int8_t  is_pressing_up_button ;
	int8_t  is_pressing_down_button ;

	int8_t  is_motion ;

} x_scrollbar_t ;


int  x_scrollbar_init( x_scrollbar_t *  sb , x_scrollbar_event_listener_t *  sb_listener ,
	char *  view_name , char *  fg_color , char *  bg_color ,
	u_int  height , u_int  line_height , u_int  num_of_log_lines ,
	int  use_transbg , x_picture_modifier_t *  pic_mod) ;

int  x_scrollbar_final( x_scrollbar_t *  sb) ;

int  x_scrollbar_set_logged_lines( x_scrollbar_t *  sb , u_int  lines) ;

int  x_scrollbar_line_is_added( x_scrollbar_t *  sb) ;

int  x_scrollbar_reset( x_scrollbar_t *  sb) ;

int  x_scrollbar_move_upward( x_scrollbar_t *  sb , u_int  size) ;

int  x_scrollbar_move_downward( x_scrollbar_t *  sb , u_int  size) ;

int  x_scrollbar_set_num_of_log_lines( x_scrollbar_t * sb , u_int  num_of_log_lines) ;

int  x_scrollbar_set_line_height( x_scrollbar_t *  sb , u_int  line_height) ;

int  x_scrollbar_set_fg_color( x_scrollbar_t *  sb , char *  fg_color) ;

int  x_scrollbar_set_bg_color( x_scrollbar_t *  sb , char *  bg_color) ;

int  x_scrollbar_change_view( x_scrollbar_t *  sb , char *  name) ;

int  x_scrollbar_set_transparent( x_scrollbar_t *  sb , x_picture_modifier_t *  pic_mod ,
	int  force) ;

int  x_scrollbar_unset_transparent( x_scrollbar_t *  sb) ;
	

#endif
