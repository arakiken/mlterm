/*
 *	$Id$
 */

#include  <stdio.h>
#include  <stdlib.h>
#include  <X11/cursorfont.h>
#include  <ml_sb_view.h>

#include  "ml_sample_sb_view_lib.h"
#include  "ml_arrow_data.h"


#define  TOP_MARGIN     14
#define  BOTTOM_MARGIN  14
#define  HEIGHT_MARGIN  (TOP_MARGIN + BOTTOM_MARGIN)
#define  WIDTH          13


/* --- static variables --- */

static GC  gc_intern ;

static Pixmap  arrow_up ;
static Pixmap  arrow_up_dent ;
static Pixmap  arrow_down ;
static Pixmap  arrow_down_dent ;

static XColor  gray_color ;
static XColor  lightgray_color ;
static int  color_init = 0 ;


/* --- static functions --- */

static void
get_geometry_hints(
	ml_sb_view_t *  view ,
	unsigned int *  width ,
	unsigned int *  top_margin ,
	unsigned int *  bottom_margin ,
	int *  up_button_y ,
	unsigned int *  up_button_height ,
	int *  down_button_y ,
	unsigned int *  down_button_height
	)
{
	*width = WIDTH ;
	*top_margin = TOP_MARGIN ;
	*bottom_margin = BOTTOM_MARGIN ;
	*up_button_y = 0 ;
	*up_button_height = TOP_MARGIN ;
	*down_button_y = -BOTTOM_MARGIN ;
	*down_button_height = BOTTOM_MARGIN ;
}

static void
get_default_color(
	ml_sb_view_t *  view ,
	char **  fg_color ,
	char **  bg_color
	)
{
	*fg_color = "gray" ;
	*bg_color = "lightgray" ;
}

static void
realized(
	ml_sb_view_t *  view ,
	Display *  display ,
	int  screen ,
	Window  window ,
	GC  gc ,
	unsigned int  height
	)
{
	view->display = display ;
	view->screen = screen ;
	view->window = window ;
	view->gc = gc ;
	view->height = height ;

	XDefineCursor( view->display , view->window , XCreateFontCursor( view->display , XC_left_ptr)) ;

	if( ! color_init)
	{
		XColor  exact ;
		
		if( XAllocNamedColor( view->display , DefaultColormap( view->display , view->screen) ,
			"gray" , &gray_color , &exact) == False)
		{
			fprintf( stderr , "gray failed.\n") ;
		}

		if( XAllocNamedColor( view->display , DefaultColormap( view->display , view->screen) ,
			"lightgray" , &lightgray_color , &exact) == False)
		{
			fprintf( stderr , "lightgray failed.\n") ;
		}

		color_init = 1 ;
	}
	
	if( ! gc_intern)
	{
		XGCValues  gc_value ;

		gc_value.foreground = BlackPixel( view->display , view->screen) ;
		gc_value.background = WhitePixel( view->display , view->screen) ;
		gc_value.graphics_exposures = 0 ;

		gc_intern = XCreateGC( view->display , view->window ,
			GCForeground | GCBackground | GCGraphicsExposures , &gc_value) ;
	}

	if( ! arrow_up)
	{
		arrow_up = ml_get_icon_pixmap( view , gc_intern , arrow_up_src , WIDTH , TOP_MARGIN) ;
	}

	if( ! arrow_down)
	{
		arrow_down = ml_get_icon_pixmap( view , gc_intern , arrow_down_src , WIDTH , BOTTOM_MARGIN) ;
	}

	if( ! arrow_up_dent)
	{
		arrow_up_dent = ml_get_icon_pixmap( view , gc_intern , arrow_up_dent_src ,
			WIDTH , TOP_MARGIN) ;
	}
	
	if( ! arrow_down_dent)
	{
		arrow_down_dent = ml_get_icon_pixmap( view , gc_intern , arrow_down_dent_src ,
			WIDTH , BOTTOM_MARGIN) ;
	}
}

static void
resized(
	ml_sb_view_t *  view ,
	Window  window ,
	unsigned int  height
	)
{
	view->window = window ;
	view->height = height ;
}

static void
delete(
	ml_sb_view_t *  view
	)
{
	free( view) ;
}

static void
draw_arrow_up_icon(
	ml_sb_view_t *  view ,
	int  is_dent
	)
{
	Pixmap  arrow ;

	if( is_dent)
	{
		arrow = arrow_up_dent ;
	}
	else
	{
		arrow = arrow_up ;
	}
	
	XCopyArea( view->display , arrow , view->window , view->gc ,
		0 , 0 , WIDTH , TOP_MARGIN , 0 , 0) ;
}

static void
draw_arrow_down_icon(
	ml_sb_view_t *  view ,
	int  is_dent
	)
{
	Pixmap  arrow ;

	if( is_dent)
	{
		arrow = arrow_down_dent ;
	}
	else
	{
		arrow = arrow_down ;
	}
	
	XCopyArea( view->display , arrow , view->window , view->gc ,
		0 , 0 , WIDTH , BOTTOM_MARGIN , 0 , view->height - BOTTOM_MARGIN) ;
}

static void
draw_decoration(
	ml_sb_view_t *  view
	)
{
	draw_arrow_up_icon( view , 0) ;
	draw_arrow_down_icon( view , 0) ;
}

static void
draw_scrollbar(
	ml_sb_view_t *  view ,
	int  bar_top_y ,
	unsigned int  bar_height
	)
{
	XClearArea( view->display , view->window , 0 , TOP_MARGIN , WIDTH ,
		view->height - HEIGHT_MARGIN , 0) ;

	/* drawing bar */
	XFillRectangle( view->display , view->window , view->gc ,
		1 , bar_top_y , WIDTH - 1 , bar_height) ;
		
	/* left side shade */
	XSetForeground( view->display , gc_intern , WhitePixel( view->display , view->screen)) ;
	XDrawLine( view->display , view->window , gc_intern ,
		0 , bar_top_y , 0 , bar_top_y + bar_height - 1) ;

	/* up side shade */
	XDrawLine( view->display , view->window , gc_intern ,
		0 , bar_top_y , WIDTH - 1 , bar_top_y) ;

	/* right side shade */
	XSetForeground( view->display , gc_intern , BlackPixel( view->display , view->screen)) ;
	XDrawLine( view->display , view->window , gc_intern ,
		WIDTH - 1 , bar_top_y , WIDTH - 1 , bar_top_y + bar_height - 1) ;

	/* down side shade */
	XDrawLine( view->display , view->window , gc_intern ,
		1 , bar_top_y + bar_height - 1 , WIDTH , bar_top_y + bar_height - 1) ;
}

static void
up_button_pressed(
	ml_sb_view_t *  view
	)
{
	draw_arrow_up_icon( view , 1) ;
}

static void
down_button_pressed(
	ml_sb_view_t *  view
	)
{
	draw_arrow_down_icon( view , 1) ;
}

static void
up_button_released(
	ml_sb_view_t *  view
	)
{
	draw_arrow_up_icon( view , 0) ;
}

static void
down_button_released(
	ml_sb_view_t *  view
	)
{
	draw_arrow_down_icon( view , 0) ;
}


/* --- global functions --- */

ml_sb_view_t *
ml_sample_sb_view_new(void)
{
	ml_sb_view_t *  view ;
	
	if( ( view = malloc( sizeof( ml_sb_view_t))) == NULL)
	{
		return  NULL ;
	}

	view->get_geometry_hints = get_geometry_hints ;
	view->get_default_color = get_default_color ;
	view->realized = realized ;
	view->resized = resized ;
	view->delete = delete ;
	
	view->draw_decoration = draw_decoration ;
	view->draw_scrollbar = draw_scrollbar ;

	view->up_button_pressed = up_button_pressed ;
	view->down_button_pressed = down_button_pressed ;
	view->up_button_released = up_button_released ;
	view->down_button_released = down_button_released ;

	return  view ;
}
