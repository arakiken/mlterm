/*
 *	$Id$
 */

#include  <stdio.h>
#include  <stdlib.h>
#include  <X11/cursorfont.h>
#include  <ml_sb_view.h>

#include  "exsb_common.h"
#include  "mozmodern_data.h"

#define  WIDTH      15
#define  MARGIN     15

typedef struct  mozmod_sb_view
{
	ml_sb_view_t  view ;

	GC  gc ;

	Pixmap  background ;
	Pixmap  arrow_up ;
	Pixmap  arrow_up_pressed ;
	Pixmap  arrow_down ;
	Pixmap  arrow_down_pressed ;

} mozmod_sb_view_t ;

static char *color_name[] =
{
	"rgb:00/00/00",  /* 0  '#' */
	"rgb:e4/eb/f2",  /* 1  ' ' */
	"rgb:c3/ca/d2",  /* 2  '.' */
	"rgb:b1/bb/c5",  /* 3  ':' */
	"rgb:a4/af/bb",  /* 4  '$' */
	"rgb:8f/9d/ad",  /* 5  '+' */
	"rgb:ad/b6/c0",  /* 6  '^' */
	"rgb:9c/a8/b4",  /* 7  '@' */
	"rgb:93/9f/ad",  /* 8  ',' */
	"rgb:70/80/92",  /* 9  '-' */
	"rgb:92/9e/ac",  /* 10 '~' */
	"rgb:87/95/a4"   /* 11 ';" */
} ;


/* --- static functions --- */

unsigned long
get_pixel_by_symbol(
	ml_sb_view_t *  view ,
	char symbol
	)
{
	int  index ;
	
	switch (symbol)
	{
		case '#':
			index = 0 ;
			break ;
		case ' ':
			index = 1 ;
			break ;
		case '.':
			index = 2 ;
			break ;
		case ':':
			index = 3 ;
			break ;
		case '$':
			index = 4 ;
			break ;
		case '+':
			index = 5 ;
			break ;
		case '^':
			index = 6 ;
			break ;
		case '@':
			index = 7 ;
			break ;
		case ',':
			index = 8 ;
			break ;
		case '-':
			index = 9 ;
			break ;
		case '~':
			index = 10 ;
			break ;
		case ';':
			index = 11 ;
			break ;
		default :
			index = 0 ;
			break ;
	}
	return  exsb_get_pixel( view->display , view->screen , color_name[index]) ;
}

static Pixmap
get_pixmap(
	ml_sb_view_t *  view ,
	GC  gc ,
	char **  data ,
	unsigned int  width ,
	unsigned int  height
	)
{
	Pixmap  pix ;
	char  cur ;
	int  x ;
	int  y ;
	
	pix = XCreatePixmap( view->display , view->window , width , height ,
		DefaultDepth( view->display , view->screen)) ;
	cur = '\0' ;
	for( y = 0 ; y < height ; y ++)
	{
		for( x = 0 ; x < width ; x ++)
		{
			if( cur != data[y][x])
			{
			        XSetForeground( view->display , gc ,
					get_pixel_by_symbol( view , data[y][x])) ;
				cur = data[y][x] ;
			}
			XDrawPoint( view->display , pix , gc , x , y) ;
		}
		x = 0 ;
	}

	return  pix ;
}

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
	*top_margin = MARGIN ;
	*bottom_margin = MARGIN ;
	*up_button_y = 0 ;
	*up_button_height = MARGIN ;
	*down_button_y = -MARGIN ;
	*down_button_height = MARGIN ;
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

static Pixmap
ml_create_sb_bg_pixmap(
	ml_sb_view_t *  view ,
	int width ,
	int height
	)
{
	Pixmap  pix ;
	mozmod_sb_view_t * mozmod_sb ;

	mozmod_sb = (mozmod_sb_view_t*) view ;

	pix = XCreatePixmap( view->display , view->window , width , height ,
		DefaultDepth( view->display , view->screen)) ;

	XSetForeground( view->display , mozmod_sb->gc ,
		exsb_get_pixel( view->display , view->screen , "rgb:98/9e/a6")) ;
	XFillRectangle( view->display , pix , mozmod_sb->gc ,
		1 , 0 , width - 2 , height);

	XSetForeground( view->display , mozmod_sb->gc ,
		BlackPixel( view->display , view->screen)) ;
	XDrawLine( view->display , pix , mozmod_sb->gc ,
		0 , 0 , 0 , height - 1) ;
	XDrawLine( view->display , pix , mozmod_sb->gc ,
		width - 1 , 0 , width - 1 , height - 1) ;

	XSetForeground( view->display , mozmod_sb->gc ,
		exsb_get_pixel( view->display , view->screen , "rgb:79/81/8c")) ;
	XDrawLine( view->display , pix , mozmod_sb->gc ,
		1 , 0 , 1 , height - 1) ;

	XSetForeground( view->display , mozmod_sb->gc ,
		exsb_get_pixel( view->display , view->screen , "rgb:8d/95/9f")) ;
	XDrawLine( view->display , pix , mozmod_sb->gc ,
		2 , 0 , 2 , height - 1) ;

	XSetForeground( view->display , mozmod_sb->gc ,
		exsb_get_pixel( view->display , view->screen , "rgb:a0/a8/ae")) ;
	XDrawLine( view->display , pix , mozmod_sb->gc ,
		width - 2 , 0 , width - 2 , height - 1) ;

	return pix ;
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
	mozmod_sb_view_t *  mozmod_sb ;
	XGCValues  gc_value ;

	mozmod_sb = (mozmod_sb_view_t*) view ;
	
	view->display = display ;
	view->screen = screen ;
	view->window = window ;
	view->gc = gc ;
	view->height = height ;

	XDefineCursor( view->display , view->window ,
		XCreateFontCursor( view->display , XC_left_ptr)) ;

	gc_value.foreground = BlackPixel( view->display , view->screen) ;
	gc_value.background = WhitePixel( view->display , view->screen) ;
	gc_value.graphics_exposures = 0 ;

	mozmod_sb->gc = XCreateGC( view->display , view->window ,
			GCForeground | GCBackground | GCGraphicsExposures ,
			&gc_value) ;

	mozmod_sb->background = ml_create_sb_bg_pixmap( view ,
		WIDTH , view->height - MARGIN * 2);
	mozmod_sb->arrow_up = get_pixmap( view , mozmod_sb->gc ,
				arrow_up_src ,
				WIDTH , MARGIN) ;
	mozmod_sb->arrow_down = get_pixmap( view , mozmod_sb->gc ,
				arrow_down_src ,
				WIDTH , MARGIN) ;
	mozmod_sb->arrow_up_pressed = get_pixmap( view , mozmod_sb->gc ,
				arrow_up_pressed_src ,
				WIDTH , MARGIN) ;
	mozmod_sb->arrow_down_pressed = get_pixmap( view , mozmod_sb->gc ,
				arrow_down_pressed_src ,
				WIDTH , MARGIN) ;

	XCopyArea( view->display , mozmod_sb->background ,
		view->window , view->gc ,
		0 , 0 , WIDTH , view->height , 0 , 0) ;
}

static void
resized(
	ml_sb_view_t *  view ,
	Window  window ,
	unsigned int  height
	)
{
	mozmod_sb_view_t *  mozmod_sb ;

	mozmod_sb = (mozmod_sb_view_t*) view ;

	view->window = window ;
	view->height = height ;

	/*
	 * create new background pixmap to fit well with resized scroll view
	 */
	XFreePixmap( view->display , mozmod_sb->background) ;
	mozmod_sb->background = ml_create_sb_bg_pixmap( view , WIDTH ,
					view->height - MARGIN * 2);
}

static void
delete(
	ml_sb_view_t *  view
	)
{
	mozmod_sb_view_t *  mozmod_sb ;

	mozmod_sb = (mozmod_sb_view_t*) view ;

	if (mozmod_sb)
	{
		XFreePixmap( view->display , mozmod_sb->background) ;
		XFreePixmap( view->display , mozmod_sb->arrow_up) ;
		XFreePixmap( view->display , mozmod_sb->arrow_up_pressed) ;
		XFreePixmap( view->display , mozmod_sb->arrow_down) ;
		XFreePixmap( view->display , mozmod_sb->arrow_down_pressed) ;

		XFreeGC( view->display , mozmod_sb->gc) ;

		free( mozmod_sb) ;
	}
}

static void
draw_arrow_up_icon(
	ml_sb_view_t *  view ,
	int  is_pressed
	)
{
	mozmod_sb_view_t *  mozmod_sb ;
	Pixmap  arrow ;
	char **  src ;

	mozmod_sb = (mozmod_sb_view_t*) view ;
	
	if( is_pressed)
	{
		arrow = mozmod_sb->arrow_up_pressed ;
		src = arrow_up_pressed_src ;
	}
	else
	{
		arrow = mozmod_sb->arrow_up ;
		src = arrow_up_src ;
	}
	
	XCopyArea( view->display , arrow , view->window , view->gc ,
		0 , 0 ,
		WIDTH , MARGIN ,
		0 , 0) ;
}

static void
draw_arrow_down_icon(
	ml_sb_view_t *  view ,
	int  is_pressed
	)
{
	mozmod_sb_view_t *  mozmod_sb ;
	Pixmap  arrow ;
	char **  src ;

	mozmod_sb = (mozmod_sb_view_t*) view ;

	if( is_pressed)
	{
		arrow = mozmod_sb->arrow_down_pressed ;
		src = arrow_down_pressed_src ;
	}
	else
	{
		arrow = mozmod_sb->arrow_down ;
		src = arrow_down_src ;
	}

	XCopyArea( view->display , arrow , view->window , view->gc ,
		0 , 0 ,
		WIDTH , MARGIN ,
		0 , view->height - MARGIN) ;
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
draw_scrollbar_common(
	ml_sb_view_t *  view ,
	int  bar_top_y ,
	unsigned int  bar_height ,
	int is_transparent
	)
{
	mozmod_sb_view_t *  mozmod_sb ;
	int  y;
	XSegment line[3] ;

	mozmod_sb = (mozmod_sb_view_t*) view ;

	/* drawing background */
	/* FIXME: shoule use XSetWindowBackgroundPixmap() */
	if (is_transparent)
	{
		XClearArea( view->display , view->window ,
			0 , MARGIN ,
			WIDTH , view->height - MARGIN * 2 , 0) ;
	}
	else
	{
		XCopyArea( view->display , mozmod_sb->background ,
			view->window , view->gc ,
			0 , 0 ,
			WIDTH , bar_top_y - MARGIN ,
			0 , MARGIN) ;
		XCopyArea( view->display , mozmod_sb->background ,
			view->window , view->gc ,
			0 , 0 ,
			WIDTH , view->height - bar_top_y - bar_height - MARGIN ,
			0 , bar_top_y + bar_height ) ;
	}

	/* drawing bar */
	if (bar_height < 6) /* can't draw shade, since too small */
	{
		XSetForeground( view->display , mozmod_sb->gc ,
			get_pixel_by_symbol( view , ':')) ;
		XFillRectangle( view->display , view->window , mozmod_sb->gc ,
			0 , bar_top_y ,
			WIDTH , bar_height) ;
		XSetForeground( view->display , mozmod_sb->gc ,
			BlackPixel(view->display , view->screen)) ;
		XDrawRectangle( view->display , view->window , mozmod_sb->gc ,
			0 , bar_top_y ,
			WIDTH - 1, bar_height ) ;
		return ;
	}

	XSetForeground( view->display , mozmod_sb->gc ,
		get_pixel_by_symbol( view , ':')) ;
	XFillRectangle( view->display , view->window , mozmod_sb->gc ,
		1 , bar_top_y + 1 ,
		WIDTH - 2 , bar_height - 2) ;

	XSetForeground( view->display , mozmod_sb->gc ,
			get_pixel_by_symbol( view , '+')) ;
	line[0].x1 = WIDTH - 2 ;
	line[0].y1 = bar_top_y + 1 ;
	line[0].x2 = WIDTH - 2 ;
	line[0].y2 = bar_top_y + bar_height - 2 ;
	line[1].x1 = 2 ;
	line[1].y1 = bar_top_y + bar_height - 2 ;
	line[1].x2 = WIDTH - 3 ;
	line[1].y2 = bar_top_y + bar_height - 2 ;
	XDrawSegments( view->display , view->window , mozmod_sb->gc ,
			line , 2) ;

	XSetForeground( view->display , mozmod_sb->gc ,
			get_pixel_by_symbol( view , '$')) ;
	line[0].x1 = WIDTH - 3 ;
	line[0].y1 = bar_top_y + 2 ;
	line[0].x2 = WIDTH - 3 ;
	line[0].y2 = bar_top_y + bar_height - 3 ;
	line[1].x1 = 3 ;
	line[1].y1 = bar_top_y + bar_height - 3 ;
	line[1].x2 = WIDTH - 4 ;
	line[1].y2 = bar_top_y + bar_height - 3 ;
	XDrawSegments( view->display , view->window , mozmod_sb->gc ,
			line , 2) ;

	XSetForeground( view->display , mozmod_sb->gc , get_pixel_by_symbol( view , ' ')) ;
	line[0].x1 = 1 ;
	line[0].y1 = bar_top_y + 1 ;
	line[0].x2 = 1 ;
	line[0].y2 = bar_top_y + bar_height - 2 ;
	line[1].x1 = 2 ;
	line[1].y1 = bar_top_y + 1 ;
	line[1].x2 = WIDTH - 3 ;
	line[1].y2 = bar_top_y + 1 ;
	XDrawSegments( view->display , view->window , mozmod_sb->gc ,
			line , 2) ;

	XSetForeground( view->display , mozmod_sb->gc , get_pixel_by_symbol( view , '.')) ;
	line[0].x1 = 2 ;
	line[0].y1 = bar_top_y + 2 ;
	line[0].x2 = 2 ;
	line[0].y2 = bar_top_y + bar_height - 3 ;
	line[1].x1 = 3 ;
	line[1].y1 = bar_top_y + 2 ;
	line[1].x2 = WIDTH - 4 ;
	line[1].y2 = bar_top_y + 2 ;
	XDrawSegments( view->display , view->window , mozmod_sb->gc ,
			line , 2) ;

	XSetForeground( view->display , mozmod_sb->gc ,
		BlackPixel( view->display , view->screen)) ;
	XDrawRectangle( view->display , view->window , mozmod_sb->gc ,
			0 , bar_top_y ,
			WIDTH - 1 , bar_height - 1) ;

	/* draw relief */
	if (bar_height > 17)
	{
		int bar_mid_y = bar_top_y + bar_height / 2 ;
		int i = 0 ;

		for ( y = bar_mid_y - 4 ; y < bar_mid_y + 5 ; y += 4)
		{
			line[i].x1 = 4 ;
			line[i].y1 = y ;
			line[i].x2 = WIDTH - 5 ;
			line[i].y2 = y ;
			i ++ ;
		}
		XSetForeground( view->display , mozmod_sb->gc ,
			exsb_get_pixel( view->display , view->screen , "rgb:6d/80/94")) ;
		XDrawSegments( view->display , view->window , mozmod_sb->gc ,
				line , i) ;

		i = 0 ;
		for ( y = bar_mid_y - 3 ; y < bar_mid_y + 6 ; y += 4)
		{
			line[i].x1 = 4 ;
			line[i].y1 = y ;
			line[i].x2 = WIDTH - 5 ;
			line[i].y2 = y ;
			i ++ ;
		}
		XSetForeground( view->display , mozmod_sb->gc ,
			exsb_get_pixel( view->display , view->screen , "rgb:d7/df/e6")) ;
		XDrawSegments( view->display , view->window , mozmod_sb->gc ,
				line , i) ;
	}

}

static void
draw_scrollbar(
	ml_sb_view_t *  view ,
	int  bar_top_y ,
	unsigned int  bar_height
	)
{
	draw_scrollbar_common( view , bar_top_y , bar_height , 0) ;
}

static void
draw_transparent_scrollbar(
	ml_sb_view_t *  view ,
	int  bar_top_y ,
	unsigned int  bar_height
	)
{
	draw_scrollbar_common( view , bar_top_y , bar_height , 1) ;
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
ml_mozmodern_sb_view_new(void)
{
	mozmod_sb_view_t *  mozmod_sb ;
	
	if( ( mozmod_sb = malloc( sizeof( mozmod_sb_view_t))) == NULL)
	{
		return  NULL ;
	}

	mozmod_sb->view.get_geometry_hints = get_geometry_hints ;
	mozmod_sb->view.get_default_color = get_default_color ;
	mozmod_sb->view.realized = realized ;
	mozmod_sb->view.resized = resized ;
	mozmod_sb->view.delete = delete ;
	
	mozmod_sb->view.draw_decoration = draw_decoration ;
	mozmod_sb->view.draw_scrollbar = draw_scrollbar ;

	mozmod_sb->view.up_button_pressed = up_button_pressed ;
	mozmod_sb->view.down_button_pressed = down_button_pressed ;
	mozmod_sb->view.up_button_released = up_button_released ;
	mozmod_sb->view.down_button_released = down_button_released ;

	mozmod_sb->gc = NULL ;

	mozmod_sb->background = None ;
	mozmod_sb->arrow_up = None ;
	mozmod_sb->arrow_up_pressed = None ;
	mozmod_sb->arrow_down = None ;
	mozmod_sb->arrow_down_pressed = None ;

	return  (ml_sb_view_t*) mozmod_sb ;
}

ml_sb_view_t *
ml_mozmodern_transparent_sb_view_new(void)
{
	mozmod_sb_view_t *  mozmod_sb ;
	
	if( ( mozmod_sb = malloc( sizeof( mozmod_sb_view_t))) == NULL)
	{
		return  NULL ;
	}

	mozmod_sb->view.get_geometry_hints = get_geometry_hints ;
	mozmod_sb->view.get_default_color = get_default_color ;
	mozmod_sb->view.realized = realized ;
	mozmod_sb->view.resized = resized ;
	mozmod_sb->view.delete = delete ;
	
	mozmod_sb->view.draw_decoration = draw_decoration ;
	mozmod_sb->view.draw_scrollbar = draw_transparent_scrollbar ;

	mozmod_sb->view.up_button_pressed = up_button_pressed ;
	mozmod_sb->view.down_button_pressed = down_button_pressed ;
	mozmod_sb->view.up_button_released = up_button_released ;
	mozmod_sb->view.down_button_released = down_button_released ;

	mozmod_sb->gc = NULL ;

	mozmod_sb->background = None ;
	mozmod_sb->arrow_up = None ;
	mozmod_sb->arrow_up_pressed = None ;
	mozmod_sb->arrow_down = None ;
	mozmod_sb->arrow_down_pressed = None ;

	return  (ml_sb_view_t*) mozmod_sb ;
}
