/*
 *	$Id$
 */

#include  "ml_simple_sb_view.h"

#include  <stdlib.h>
#include  <kiklib/kik_mem.h>		/* malloc */
#include  <kiklib/kik_types.h>


#define  WIDTH          10


typedef struct  ml_simple_sb_view
{
	ml_sb_view_t  view ;
	
	int8_t  is_transparent ;
	
} ml_simple_sb_view_t ;


/* --- static functions --- */

static void
get_geometry_hints(
	ml_sb_view_t *  view ,
	u_int *  width ,
	u_int *  top_margin ,
	u_int *  bottom_margin ,
	int *  up_button_y ,
	u_int *  up_button_height ,
	int *  down_button_y ,
	u_int *  down_button_height
	)
{
	*width = WIDTH ;
	*top_margin = 0 ;
	*bottom_margin = 0 ;
	*up_button_y = 0 ;
	*up_button_height = 0 ;
	*down_button_y = 0 ;
	*down_button_height = 0 ;
}

static void
get_default_color(
	ml_sb_view_t *  view ,
	char **  fg_color ,
	char **  bg_color
	)
{
	*fg_color = "black" ;
	*bg_color = "white" ;
}

static void
realized(
	ml_sb_view_t *  view ,
	Display *  display ,
	int  screen ,
	Window  window ,
	GC  gc ,
	u_int  height
	)
{
	view->display = display ;
	view->screen = screen ;
	view->window = window ;
	view->gc = gc ;
	view->height = height ;
}

static void
resized(
	ml_sb_view_t *  view ,
	Window  window ,
	u_int  height
	)
{
	view->window = window ;
	view->height = height ;
}

static void
draw_decoration(
	ml_sb_view_t *  view
	)
{
	/* do nothing */
}

static void
draw_scrollbar(
	ml_sb_view_t *  view ,
	int  bar_top_y ,
	u_int  bar_height
	)
{
	ml_simple_sb_view_t *  simple_view ;

	simple_view = (ml_simple_sb_view_t*) view ;
	
	XClearArea( view->display , view->window , 0 , 0 , WIDTH , view->height , 0) ;

	if( ! simple_view->is_transparent)
	{
		XFillRectangle( view->display , view->window , view->gc , 0 , bar_top_y ,
			WIDTH , bar_height) ;
	}
	else
	{
		XFillRectangle( view->display , view->window , view->gc , 0 , bar_top_y , 1 , bar_height) ;
		XFillRectangle( view->display , view->window , view->gc , WIDTH ,
			bar_top_y , 1 , bar_height) ;
		XFillRectangle( view->display , view->window , view->gc , 0 , bar_top_y , WIDTH , 1) ;
		XFillRectangle( view->display , view->window , view->gc , 0 ,
			bar_top_y + bar_height - 1 , WIDTH , 1) ;
	}
}

static void
delete(
	ml_sb_view_t *  view
	)
{
	free( view) ;
}


/* --- global functions --- */

ml_sb_view_t *
ml_simple_sb_view_new(void)
{
	ml_simple_sb_view_t *  view ;

	if( ( view = malloc( sizeof( ml_simple_sb_view_t))) == NULL)
	{
		return  NULL ;
	}

	view->view.get_geometry_hints = get_geometry_hints ;
	view->view.get_default_color = get_default_color ;
	view->view.realized = realized ;
	view->view.resized = resized ;
	view->view.delete = delete ;
	
	view->view.draw_decoration = draw_decoration ;
	view->view.draw_scrollbar = draw_scrollbar ;
	
	view->view.up_button_pressed = NULL ;
	view->view.down_button_pressed = NULL ;
	view->view.up_button_released = NULL ;
	view->view.down_button_released = NULL ;

	view->is_transparent = 0 ;

	return  (ml_sb_view_t*)view ;
}

ml_sb_view_t *
ml_simple_transparent_sb_view_new(void)
{
	ml_simple_sb_view_t *  view ;

	if( ( view = malloc( sizeof( ml_simple_sb_view_t))) == NULL)
	{
		return  NULL ;
	}

	view->view.get_geometry_hints = get_geometry_hints ;
	view->view.get_default_color = get_default_color ;
	view->view.realized = realized ;
	view->view.resized = resized ;
	view->view.delete = delete ;
	
	view->view.draw_decoration = draw_decoration ;
	view->view.draw_scrollbar = draw_scrollbar ;
	
	view->view.up_button_pressed = NULL ;
	view->view.down_button_pressed = NULL ;
	view->view.up_button_released = NULL ;
	view->view.down_button_released = NULL ;

	view->is_transparent = 1 ;

	return  (ml_sb_view_t*)view ;
}
