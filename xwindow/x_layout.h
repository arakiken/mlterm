/*
 *	$Id$
 */

#ifndef  __X_LAYOUT_H__
#define  __X_LAYOUT_H__


#include  <kiklib/kik_types.h>		/* int8_t */

#include  "x_screen.h"
#include  "x_scrollbar.h"
#include  "x_color_manager.h"


#define  X_SCREEN_TO_LAYOUT(screen)  ((x_layout_t*)(screen)->window.parent)


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

		/* 0: right, 1: down */
		struct terminal *  next[2] ;

	} term ;

	char *  pic_file_path ;
	x_picture_modifier_t  pic_mod ;
	x_picture_t *  bg_pic ;

	void (*line_scrolled_out)( void *) ;
	
} x_layout_t ;


x_layout_t *  x_layout_new( x_screen_t *  screen ,
	char *  view_name , char *  fg_color , char *  bg_color , x_sb_mode_t  mode) ;

int  x_layout_delete( x_layout_t *  layout) ;

int  x_layout_add_child( x_layout_t *  layout , x_screen_t *  screen ,
	int  horizontal , const char *  percent) ;

int  x_layout_remove_child( x_layout_t *  layout , x_screen_t *  screen) ;

int  x_layout_switch_screen( x_layout_t *  layout , int  prev) ;

int  x_layout_resize( x_layout_t *  layout , x_screen_t *  screen ,
	int  horizontal , int  step) ;

#define  x_layout_has_one_child( layout) \
	((layout)->term.next[0] == NULL && ((layout)->term.next[1]) == NULL)


#endif
