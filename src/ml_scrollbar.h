/*
 *	$Id$
 */

#ifndef  __ML_SCROLLBAR_H__
#define  __ML_SCROLLBAR_H__


#include  "ml_window.h"
#include  "ml_sb_view.h"


typedef struct  ml_scrollbar_event_listener
{
	void *  self ;
	int  (*screen_scroll_to)( void * , int) ;
	int  (*screen_scroll_upward)( void * , u_int) ;
	int  (*screen_scroll_downward)( void * , u_int) ;

} ml_scrollbar_event_listener_t ;

typedef struct  ml_scrollbar
{
	ml_window_t  window ;

	char *  view_name ;
	ml_sb_view_t *  view ;
	
	ml_scrollbar_event_listener_t *  sb_listener ;

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

} ml_scrollbar_t ;


int  ml_scrollbar_init( ml_scrollbar_t *  sb , ml_scrollbar_event_listener_t *  sb_listener ,
	char *  view_name , ml_color_table_t  color_table , u_int  height , u_int  line_height ,
	u_int  num_of_log_lines , int  use_transbg) ;

int  ml_scrollbar_final( ml_scrollbar_t *  sb) ;

int  ml_scrollbar_line_is_added( ml_scrollbar_t *  sb) ;

int  ml_scrollbar_reset( ml_scrollbar_t *  sb) ;

int  ml_scrollbar_move_upward( ml_scrollbar_t *  sb , u_int  size) ;

int  ml_scrollbar_move_downward( ml_scrollbar_t *  sb , u_int  size) ;

int  ml_scrollbar_set_num_of_log_lines( ml_scrollbar_t * sb , u_int  num_of_log_lines) ;

int  ml_scrollbar_set_line_height( ml_scrollbar_t *  sb , u_int  line_height) ;

int  ml_scrollbar_set_transparent( ml_scrollbar_t *  sb) ;

int  ml_scrollbar_unset_transparent( ml_scrollbar_t *  sb) ;
	

#endif
