/*
 *	update: <2001/10/31(16:25:26)>
 *	$Id$
 */

#ifndef  __ML_SB_TERM_SCREEN_H__
#define  __ML_SB_TERM_SCREEN_H__


#include  "ml_term_screen.h"
#include  "ml_scrollbar.h"


typedef struct  ml_sb_term_screen
{
	ml_window_t  window ;

	ml_term_screen_t *  termscr ;
	ml_scrollbar_t  scrollbar ;

	ml_scrollbar_event_listener_t  sb_listener ;
	ml_screen_scroll_event_listener_t  screen_scroll_listener ;
	
	void (*receive_upward_scrolled_out_line)( void * , ml_image_line_t *) ;
	
} ml_sb_term_screen_t ;


ml_sb_term_screen_t *  ml_sb_term_screen_new( ml_term_screen_t *  termscr ,
	char *  view_name , ml_color_table_t  color_table , int  use_transbg) ;

int  ml_sb_term_screen_delete( ml_sb_term_screen_t *  sb_termscr) ;


#endif
