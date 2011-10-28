/*
 *	$Id$
 */

#include  <stdio.h>
#include  <stdlib.h>
#include  <x_sb_view.h>

#include  "x_sample_sb_view_lib.h"
#include  "x_arrow_data.h"


#define  TOP_MARGIN     14
#define  BOTTOM_MARGIN  14
#define  HEIGHT_MARGIN  (TOP_MARGIN + BOTTOM_MARGIN)
#define  WIDTH          13


/* --- static functions --- */

static void
get_geometry_hints(
	x_sb_view_t *  view ,
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
	x_sb_view_t *  view ,
	char **  fg_color ,
	char **  bg_color
	)
{
	*fg_color = "gray" ;
	*bg_color = "lightgray" ;
}

static void
realized(
	x_sb_view_t *  view ,
	Display *  display ,
	int  screen ,
	Window  window ,
	GC  gc ,
	unsigned int  height
	)
{
	sample_sb_view_t *  sample ;
	XGCValues  gc_value ;
	XWindowAttributes  attr ;
	XColor  black = { 0 , 0 , 0 , 0 , 0 , 0 , } ;
	XColor  white = { 0 , 0xffff , 0xffff , 0xffff , 0 , 0 , } ;

	sample = (sample_sb_view_t*) view ;
	
	view->display = display ;
	view->screen = screen ;
	view->window = window ;
	view->gc = gc ;
	view->height = height ;

	XGetWindowAttributes( view->display , view->window , &attr) ;
	XAllocColor( view->display , attr.colormap , &black) ;
	XAllocColor( view->display , attr.colormap , &white) ;

	sample->black_pixel = gc_value.foreground = black.pixel ;
	sample->white_pixel = gc_value.background = white.pixel ;
	gc_value.graphics_exposures = 0 ;

	sample->gc = XCreateGC( view->display , view->window ,
			GCForeground | GCBackground | GCGraphicsExposures , &gc_value) ;

	sample->arrow_up = x_get_icon_pixmap( view , sample->gc , arrow_up_src ,
				WIDTH , TOP_MARGIN , attr.depth , black.pixel , white.pixel) ;
	sample->arrow_down = x_get_icon_pixmap( view , sample->gc , arrow_down_src ,
				WIDTH , BOTTOM_MARGIN , attr.depth , black.pixel , white.pixel) ;
	sample->arrow_up_dent = x_get_icon_pixmap( view , sample->gc , arrow_up_dent_src ,
				WIDTH , TOP_MARGIN , attr.depth , black.pixel , white.pixel) ;
	sample->arrow_down_dent = x_get_icon_pixmap( view , sample->gc , arrow_down_dent_src ,
				WIDTH , BOTTOM_MARGIN , attr.depth , black.pixel , white.pixel) ;
}

static void
resized(
	x_sb_view_t *  view ,
	Window  window ,
	unsigned int  height
	)
{
	view->window = window ;
	view->height = height ;
}

static void
color_changed(
	x_sb_view_t *  view ,
	int  is_fg		/* 1=fg , 0=bg */
	)
{
	if( is_fg)
	{
		sample_sb_view_t *  sample ;

		sample = (sample_sb_view_t*) view ;

		x_draw_icon_pixmap_fg( view , sample->arrow_up , arrow_up_src ,
			WIDTH , TOP_MARGIN) ;
		x_draw_icon_pixmap_fg( view , sample->arrow_down , arrow_down_src ,
			WIDTH , BOTTOM_MARGIN) ;
		x_draw_icon_pixmap_fg( view , sample->arrow_up_dent , arrow_up_dent_src ,
			WIDTH , TOP_MARGIN) ;
		x_draw_icon_pixmap_fg( view , sample->arrow_down_dent , arrow_down_dent_src ,
			WIDTH , BOTTOM_MARGIN) ;
	}
}

static void
delete(
	x_sb_view_t *  view
	)
{
	sample_sb_view_t *  sample ;

	sample = (sample_sb_view_t*) view ;

	if( sample)
	{
		XFreePixmap( view->display , sample->arrow_up) ;
		XFreePixmap( view->display , sample->arrow_up_dent) ;
		XFreePixmap( view->display , sample->arrow_down) ;
		XFreePixmap( view->display , sample->arrow_down_dent) ;

		XFreeGC( view->display , sample->gc) ;

		free( sample) ;
	}
}

static void
draw_arrow_up_icon(
	x_sb_view_t *  view ,
	int  is_dent
	)
{
	sample_sb_view_t *  sample ;
	Pixmap  arrow ;

	sample = (sample_sb_view_t*) view ;
	
	if( is_dent)
	{
		arrow = sample->arrow_up_dent ;
	}
	else
	{
		arrow = sample->arrow_up ;
	}
	
	XCopyArea( view->display , arrow , view->window , view->gc ,
		0 , 0 , WIDTH , TOP_MARGIN , 0 , 0) ;
}

static void
draw_arrow_down_icon(
	x_sb_view_t *  view ,
	int  is_dent
	)
{
	sample_sb_view_t *  sample ;
	Pixmap  arrow ;

	sample = (sample_sb_view_t*) view ;

	if( is_dent)
	{
		arrow = sample->arrow_down_dent ;
	}
	else
	{
		arrow = sample->arrow_down ;
	}
	
	XCopyArea( view->display , arrow , view->window , view->gc ,
		0 , 0 , WIDTH , BOTTOM_MARGIN , 0 , view->height - BOTTOM_MARGIN) ;
}

static void
draw_scrollbar(
	x_sb_view_t *  view ,
	int  bar_top_y ,
	unsigned int  bar_height
	)
{
	sample_sb_view_t *  sample ;

	sample = (sample_sb_view_t*) view ;
	
	/* drawing bar */
	XFillRectangle( view->display , view->window , view->gc ,
		1 , bar_top_y , WIDTH - 1 , bar_height) ;
		
	/* left side shade */
	XSetForeground( view->display , sample->gc , sample->white_pixel) ;
	XDrawLine( view->display , view->window , sample->gc ,
		0 , bar_top_y , 0 , bar_top_y + bar_height - 1) ;

	/* up side shade */
	XDrawLine( view->display , view->window , sample->gc ,
		0 , bar_top_y , WIDTH - 1 , bar_top_y) ;

	/* right side shade */
	XSetForeground( view->display , sample->gc , sample->black_pixel) ;
	XDrawLine( view->display , view->window , sample->gc ,
		WIDTH - 1 , bar_top_y , WIDTH - 1 , bar_top_y + bar_height - 1) ;

	/* down side shade */
	XDrawLine( view->display , view->window , sample->gc ,
		1 , bar_top_y + bar_height - 1 , WIDTH , bar_top_y + bar_height - 1) ;
}

static void
draw_background(
	x_sb_view_t *  view ,
	int  y ,
	unsigned int  height
	)
{
	XClearArea( view->display , view->window , 0 , y , WIDTH , height , 0) ;
}

static void
draw_up_button(
	x_sb_view_t *  view ,
	int  is_pressed
	)
{
	draw_arrow_up_icon( view , is_pressed) ;
}

static void
draw_down_button(
	x_sb_view_t *  view ,
	int  is_pressed
	)
{
	draw_arrow_down_icon( view , is_pressed) ;
}


/* --- global functions --- */

x_sb_view_t *
x_sample_sb_view_new(void)
{
	sample_sb_view_t *  sample ;
	
	if( ( sample = malloc( sizeof( sample_sb_view_t))) == NULL)
	{
		return  NULL ;
	}

	sample->view.version = 1 ;

	sample->view.get_geometry_hints = get_geometry_hints ;
	sample->view.get_default_color = get_default_color ;
	sample->view.realized = realized ;
	sample->view.resized = resized ;
	sample->view.color_changed = color_changed ;
	sample->view.delete = delete ;

	sample->view.draw_scrollbar = draw_scrollbar ;
	sample->view.draw_background = draw_background ;
	sample->view.draw_up_button = draw_up_button ;
	sample->view.draw_down_button = draw_down_button ;

	sample->gc = None ;
	sample->arrow_up = None ;
	sample->arrow_up_dent = None ;
	sample->arrow_down = None ;
	sample->arrow_down_dent = None ;

	return  (x_sb_view_t*) sample ;
}
