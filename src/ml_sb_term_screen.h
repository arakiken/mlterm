/*
 *	$Id$
 */

#ifndef  __ML_SB_TERM_SCREEN_H__
#define  __ML_SB_TERM_SCREEN_H__


#include  <kiklib/kik_types.h>		/* int8_t */

#include  "ml_term_screen.h"
#include  "ml_scrollbar.h"
#include  "ml_color_manager.h"


typedef struct  ml_sb_term_screen
{
	ml_window_t  window ;

	ml_term_screen_t *  termscr ;
	ml_scrollbar_t  scrollbar ;

	ml_scrollbar_event_listener_t  sb_listener ;
	ml_screen_scroll_event_listener_t  screen_scroll_listener ;

	ml_sb_mode_t  sb_mode ;
	
	void (*receive_upward_scrolled_out_line)( void * , ml_image_line_t *) ;
	
} ml_sb_term_screen_t ;


ml_sb_term_screen_t *  ml_sb_term_screen_new( ml_term_screen_t *  termscr ,
	char *  view_name , ml_color_manager_t *  color_man ,
	ml_color_t  fg_color , ml_color_t  bg_color , ml_sb_mode_t  mode) ;

int  ml_sb_term_screen_delete( ml_sb_term_screen_t *  sb_termscr) ;


#endif
