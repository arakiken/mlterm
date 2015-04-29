/*
 *	$Id$
 */

#ifndef  __X_LAYOUT_H__
#define  __X_LAYOUT_H__


#include  <kiklib/kik_types.h>		/* int8_t */

#include  "x_screen.h"
#include  "x_scrollbar.h"
#include  "x_color_manager.h"


typedef struct  x_layout
{
	x_window_t  window ;

	struct terminal
	{
		x_screen_t *  screen ;
		x_scrollbar_t  scrollbar ;
		x_sb_mode_t  sb_mode ;
		x_scrollbar_event_listener_t  sb_listener ;
		x_screen_scroll_event_listener_t  screen_scroll_listener ;

		u_int16_t  separator_x ;
		u_int16_t  separator_y ;
		int  yfirst ;

		struct terminal *  next[2] ;

	} term ;

	void (*line_scrolled_out)( void *) ;
	
} x_layout_t ;


x_layout_t *  x_layout_new( x_screen_t *  screen ,
	char *  view_name , char *  fg_color , char *  bg_color , x_sb_mode_t  mode) ;

int  x_layout_delete( x_layout_t *  layout) ;

int  x_layout_add_child( x_layout_t *  layout , x_screen_t *  screen , int  vertical) ;

int  x_layout_remove_child( x_layout_t *  layout , x_screen_t *  screen) ;


#endif
