/*
 *	$Id$
 */

#ifndef  __X_SB_TERM_SCREEN_H__
#define  __X_SB_TERM_SCREEN_H__


#include  <kiklib/kik_types.h>		/* int8_t */

#include  "x_screen.h"
#include  "x_scrollbar.h"
#include  "x_color_manager.h"


typedef struct  x_sb_screen
{
	x_window_t  window ;

	x_screen_t *  screen ;
	x_scrollbar_t  scrollbar ;

	x_scrollbar_event_listener_t  sb_listener ;
	x_screen_scroll_event_listener_t  screen_scroll_listener ;

	x_sb_mode_t  sb_mode ;
	
} x_sb_screen_t ;


x_sb_screen_t *  x_sb_screen_new( x_screen_t *  screen ,
	char *  view_name , char *  fg_color , char *  bg_color , x_sb_mode_t  mode) ;

int  x_sb_screen_delete( x_sb_screen_t *  sb_screen) ;


#endif
