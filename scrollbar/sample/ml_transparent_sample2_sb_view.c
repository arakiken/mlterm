/*
 *	$Id$
 */

#include  <stdio.h>
#include  <stdlib.h>
#include  <X11/cursorfont.h>
#include  <ml_sb_view.h>

#include  "ml_sample_sb_view_lib.h"
#include  "ml_transparent_arrow_data.h"


#define  TOP_MARGIN     0
#define  BOTTOM_MARGIN  28
#define  HEIGHT_MARGIN  (TOP_MARGIN + BOTTOM_MARGIN)
#define  WIDTH          13


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
	*up_button_y = -BOTTOM_MARGIN ;
	*up_button_height = BOTTOM_MARGIN / 2 ;
	*down_button_y = -(BOTTOM_MARGIN / 2) ;
	*down_button_height = BOTTOM_MARGIN / 2 ;
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
	sample_sb_view_t *  sample ;
	XGCValues  gc_value ;

	sample = (sample_sb_view_t*) view ;
	
	view->display = display ;
	view->screen = screen ;
	view->window = window ;
	view->gc = gc ;
	view->height = height ;

	XDefineCursor( view->display , view->window , XCreateFontCursor( view->display , XC_left_ptr)) ;

	gc_value.foreground = BlackPixel( view->display , view->screen) ;
	gc_value.background = WhitePixel( view->display , view->screen) ;
	gc_value.graphics_exposures = 0 ;

	sample->gc = XCreateGC( view->display , view->window ,
			GCForeground | GCBackground | GCGraphicsExposures , &gc_value) ;
	
	sample->arrow_up = ml_get_icon_pixmap( view , sample->gc , arrow_up_src ,
				WIDTH , BOTTOM_MARGIN / 2) ;
	sample->arrow_down = ml_get_icon_pixmap( view , sample->gc , arrow_down_src ,
				WIDTH , BOTTOM_MARGIN / 2) ;
	sample->arrow_up_dent = ml_get_icon_pixmap( view , sample->gc , arrow_up_dent_src ,
					WIDTH , BOTTOM_MARGIN / 2) ;
	sample->arrow_down_dent = ml_get_icon_pixmap( view , sample->gc , arrow_down_dent_src ,
					WIDTH , BOTTOM_MARGIN / 2) ;
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
	sample_sb_view_t *  sample ;

	sample = (sample_sb_view_t*) view ;

	XFreePixmap( view->display , sample->arrow_up) ;
	XFreePixmap( view->display , sample->arrow_up_dent) ;
	XFreePixmap( view->display , sample->arrow_down) ;
	XFreePixmap( view->display , sample->arrow_down_dent) ;
	
	XFreeGC( view->display , sample->gc) ;
	
	free( sample) ;
}

static void
draw_arrow_up_icon(
	ml_sb_view_t *  view ,
	int  is_dent
	)
{
	sample_sb_view_t *  sample ;
	Pixmap  arrow ;
	int  x ;
	int  y ;
	char **  src ;

	sample = (sample_sb_view_t*) view ;
	
	if( is_dent)
	{
		arrow = sample->arrow_up_dent ;
		src = arrow_up_dent_src ;
	}
	else
	{
		arrow = sample->arrow_up ;
		src = arrow_up_src ;
	}
	
	for( y = 0 ; y < BOTTOM_MARGIN / 2 ; y ++)
	{
		for( x = 0 ; x < WIDTH ; x ++)
		{
			if( src[y][x] == '-')
			{
				XCopyArea( view->display , view->window , arrow ,
					view->gc , x , y + (view->height - BOTTOM_MARGIN) , 
					1 , 1 , x , y) ;
			}
		}

		x = 0 ;
	}

	XCopyArea( view->display , arrow , view->window , view->gc ,
		0 , 0 , WIDTH , BOTTOM_MARGIN / 2 , 0 , view->height - BOTTOM_MARGIN) ;
}

static void
draw_arrow_down_icon(
	ml_sb_view_t *  view ,
	int  is_dent
	)
{
	sample_sb_view_t *  sample ;
	Pixmap  arrow ;
	int  x ;
	int  y ;
	char **  src ;

	sample = (sample_sb_view_t*) view ;
	
	if( is_dent)
	{
		arrow = sample->arrow_down_dent ;
		src = arrow_down_dent_src ;
	}
	else
	{
		arrow = sample->arrow_down ;
		src = arrow_down_src ;
	}
	
	for( y = 0 ; y < BOTTOM_MARGIN / 2 ; y ++)
	{
		for( x = 0 ; x < WIDTH ; x ++)
		{
			if( src[y][x] == '-')
			{
				XCopyArea( view->display , view->window , arrow ,
					view->gc , x , y + (view->height - BOTTOM_MARGIN / 2) ,
					1 , 1 , x , y) ;
			}
		}

		x = 0 ;
	}

	XCopyArea( view->display , arrow , view->window , view->gc ,
		0 , 0 , WIDTH , BOTTOM_MARGIN / 2 , 0 , view->height - BOTTOM_MARGIN / 2) ;
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
	sample_sb_view_t *  sample ;

	sample = (sample_sb_view_t*) view ;
	
	XClearArea( view->display , view->window , 0 , TOP_MARGIN , WIDTH ,
		view->height - HEIGHT_MARGIN , 0) ;

	/* drawing bar */
	
	/* left side shade */
	XSetForeground( view->display , sample->gc , WhitePixel( view->display , view->screen)) ;
	XDrawLine( view->display , view->window , sample->gc ,
		0 , bar_top_y , 0 , bar_top_y + bar_height - 1) ;

	/* up side shade */
	XDrawLine( view->display , view->window , sample->gc ,
		0 , bar_top_y , WIDTH - 1 , bar_top_y) ;

	/* right side shade */
	XSetForeground( view->display , sample->gc , BlackPixel( view->display , view->screen)) ;
	XDrawLine( view->display , view->window , sample->gc ,
		WIDTH - 1 , bar_top_y , WIDTH - 1 , bar_top_y + bar_height - 1) ;

	/* down side shade */
	XDrawLine( view->display , view->window , sample->gc ,
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
ml_sample2_transparent_sb_view_new(void)
{
	sample_sb_view_t *  sample ;
	
	if( ( sample = malloc( sizeof( sample_sb_view_t))) == NULL)
	{
		return  NULL ;
	}

	sample->view.get_geometry_hints = get_geometry_hints ;
	sample->view.get_default_color = get_default_color ;
	sample->view.realized = realized ;
	sample->view.resized = resized ;
	sample->view.delete = delete ;
	
	sample->view.draw_decoration = draw_decoration ;
	sample->view.draw_scrollbar = draw_scrollbar ;

	sample->view.up_button_pressed = up_button_pressed ;
	sample->view.down_button_pressed = down_button_pressed ;
	sample->view.up_button_released = up_button_released ;
	sample->view.down_button_released = down_button_released ;

	sample->arrow_up = None ;
	sample->arrow_up_dent = None ;
	sample->arrow_down = None ;
	sample->arrow_down_dent = None ;

	return  (ml_sb_view_t*) sample ;
}

