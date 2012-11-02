/*
 *	$Id$
 */

#include  <stdio.h>
#include  <stdlib.h>
#include  <x_sb_view.h>

#include  "x_arrow_data.h"


#define  TOP_MARGIN     14
#define  BOTTOM_MARGIN  14
#define  HEIGHT_MARGIN  (TOP_MARGIN + BOTTOM_MARGIN)
#define  WIDTH          13


typedef struct x_color
{
	/* Public */
	u_long  pixel ;

	/* Private except x_color_cache.c */
	u_int8_t  red ;
	u_int8_t  green ;
	u_int8_t  blue ;
	u_int8_t  alpha ;

} x_color_t ;

int  x_window_clear( Window  win , int  x , int  y , u_int  width , u_int  height) ;
int  x_window_fill( Window  win , int  x , int  y , u_int  width , u_int  height) ;
int  x_window_fill_with( Window  win , x_color_t *  color ,
	int  x , int  y , u_int  width , u_int  height) ;


/* --- static variables --- */

x_color_t  black = { 0x0 , 0x0 , 0x0 , 0x0 , 0xff } ;
x_color_t  white = { 0x0 , 0xff , 0xff , 0xff , 0xff } ;


/* --- static functions --- */

static int
draw_icon(
	x_sb_view_t *  view ,
	int  x ,
	int  y ,
	char **  data ,
	unsigned int  width ,
	unsigned int  height
	)
{
	int  x_off ;
	int  y_off ;

	for( y_off = 0 ; y_off < height ; y_off ++)
	{
		for( x_off = 0 ; x_off < width ; x_off ++)
		{
			if( data[y_off][x_off] == '-')
			{
				x_window_fill( view->window , x + x_off , y + y_off , 1 , 1) ;
			}
			else if( data[y_off][x_off] == '#')
			{
				x_window_fill_with( view->window , &black ,
					x + x_off , y + y_off , 1 , 1) ;
			}
			else
			{
				x_window_fill_with( view->window , &white ,
					x + x_off , y + y_off , 1 , 1) ;
			}
		}
	}

	return  1 ;
}

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
	if( display->cmap)
	{
		/* 8 bpp */
		black.pixel = 0 ;
		white.pixel = 7 ;
	}
	else
	{
		/* 15 or more bpp */
		white.pixel = RGB_TO_PIXEL(0xff,0xff,0xff,display->rgbinfo) ;
	}

	view->display = display ;
	view->screen = screen ;
	view->window = window ;
	view->gc = gc ;
	view->height = height ;
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
delete(
	x_sb_view_t *  view
	)
{
	free( view) ;
}

static void
draw_arrow_up_icon(
	x_sb_view_t *  view ,
	int  is_dent
	)
{
	draw_icon( view , 0 , 0 , is_dent ? arrow_up_dent_src : arrow_up_src ,
		WIDTH , TOP_MARGIN) ;
}

static void
draw_arrow_down_icon(
	x_sb_view_t *  view ,
	int  is_dent
	)
{
	draw_icon( view , 0 , view->height - BOTTOM_MARGIN ,
		is_dent ? arrow_down_dent_src : arrow_down_src ,
		WIDTH , BOTTOM_MARGIN) ;
}

static void
draw_scrollbar(
	x_sb_view_t *  view ,
	int  bar_top_y ,
	unsigned int  bar_height
	)
{
	/* drawing bar */
	x_window_fill( view->window , 1 , bar_top_y , WIDTH - 1 , bar_height) ;

	/* left side shade */
	x_window_fill_with( view->window , &white , 0 , bar_top_y , 1 , bar_height) ;

	/* up side shade */
	x_window_fill_with( view->window , &white , 0 , bar_top_y , WIDTH , 1) ;

	/* right side shade */
	x_window_fill_with( view->window , &black ,
		WIDTH - 1 , bar_top_y , 1 , bar_height) ;

	/* down side shade */
	x_window_fill_with( view->window , &black ,
		1 , bar_top_y + bar_height - 1 , WIDTH , 1) ;
}

static void
draw_background(
	x_sb_view_t *  view ,
	int  y ,
	unsigned int  height
	)
{
	x_window_clear( view->window , 0 , y , WIDTH , height) ;
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
	x_sb_view_t *  view ;

	if( ( view = calloc( 1 , sizeof( x_sb_view_t))) == NULL)
	{
		return  NULL ;
	}

	view->version = 1 ;

	view->get_geometry_hints = get_geometry_hints ;
	view->get_default_color = get_default_color ;
	view->realized = realized ;
	view->resized = resized ;
	view->delete = delete ;

	view->draw_scrollbar = draw_scrollbar ;
	view->draw_background = draw_background ;
	view->draw_up_button = draw_up_button ;
	view->draw_down_button = draw_down_button ;

	return  view ;
}
