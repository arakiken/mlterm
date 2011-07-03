/*
 *	$Id$
 */

#if 0
#define __DEBUG 1
#endif

#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <X11/extensions/shape.h>
#include  <x_sb_view.h>

#define MAX_IMAGE_SIZE 0xffff

#define free_pixmap( d , p) \
	do { \
		if( p) \
		{ \
			XFreePixmap( d , p) ; \
		} \
		p = None ; \
	} while ( 0)

#define STR2NUM_WITH_RANGE_CHECK(str , variables) \
	do { \
		int  v ; \
		sscanf( (str) , "%d" , &v) ; \
		if( v > 1 && v < MAX_IMAGE_SIZE) \
		{ \
			(variables) = v ; \
		} \
		else \
		{ \
			(variables) = 0 ; \
		} \
	} while ( 0)


typedef enum  button_layout
{
	BTN_NONE ,
	BTN_NORMAL ,
	BTN_NORTHGRAVITY ,
	BTN_SOUTHGRAVITY ,
} button_layout_t ;

typedef struct  shared_image
{
	Display *  display ;
	x_sb_view_conf_t *  conf ;

	/* Don't access directly from ps */
	unsigned int  btn_up_h ;
	unsigned int  btn_dw_h ;

	/* background */
	unsigned int  bg_top_h ;
	unsigned int  bg_bottom_h ;
	Pixmap  bg_top ;
	Pixmap  bg_bottom ;
	
	/* up/down buttons */
	Pixmap  btn_up ;
	Pixmap  btn_dw ;
	Pixmap  btn_up_pressed ;
	Pixmap  btn_dw_pressed ;
	Pixmap  btn_up_mask ;
	Pixmap  btn_dw_mask ;
	Pixmap  btn_up_pressed_mask ;
	Pixmap  btn_dw_pressed_mask ;

	/* slider */
	unsigned int  slider_width ;
	unsigned int  slider_top_h ;
	unsigned int  slider_bottom_h ;
	unsigned int  slider_knob_h ;
	Pixmap  slider_top ;
	Pixmap  slider_bottom ;
	Pixmap  slider_knob ;
	Pixmap  slider_top_mask ;
	Pixmap  slider_bottom_mask ;
	Pixmap  slider_knob_mask ;

	unsigned int  use_count ;

} shared_image_t ;

typedef struct  pixmap_sb_view
{
	x_sb_view_t  view ;

	x_sb_view_conf_t *  conf ;

	GC  gc ;

	unsigned int  depth ;

	int  is_transparent ;

	/* rc settings */
	unsigned int  width ;
	unsigned int  top_margin ;
	unsigned int  bottom_margin ;
	unsigned int  btn_up_h ;
	unsigned int  btn_dw_h ;
	unsigned int  pre_slider_h ;
	int  bg_enable_trans ;
	int  bg_tile ;
	button_layout_t  btn_layout ;
	int  slider_tile ;

	shared_image_t *  si ;
	
	/* background */
	unsigned int  bg_body_h ;
	Pixmap  bg_body ;
	Pixmap  bg_cache ;

	/* slider */
	unsigned int  slider_body_h ;
	Pixmap  slider_body ;
	Pixmap  slider_body_mask ;
	Pixmap  slider_tiled_cache ;
	Pixmap  slider_tiled_cache_mask ;

} pixmap_sb_view_t ;


/* --- static variables --- */

static shared_image_t **  shared_images ;
static unsigned int  num_of_shared_images ;


/* --- static functions --- */

/* XXX Hack (declared in x_display.h) */
typedef struct x_display_t *  x_display_ptr_t ;
x_display_ptr_t *  x_get_opened_displays( unsigned int *  num) ;

/* XXX Hack (declared in x_imagelib.h) */
int  x_imagelib_load_file( x_display_ptr_t  disp , char *  path ,
	/* u_int32_t */ unsigned int **  cardinal /* Not used in this file */ ,
	Pixmap *  pixmap , Pixmap *  mask , unsigned int *  width , unsigned int *  height) ;

static void
load_image(
	Display *  display ,
	const char *  dir ,
	const char *  file ,
	Pixmap *  pixmap ,	/* not NULL */
	Pixmap *  mask ,	/* can be NULL */
	unsigned int *  width ,	/* not NULL or 0 */
	unsigned int *  height	/* not NULL */
	)
{
	char *  path ;
	int  len ;
	x_display_ptr_t *  disps ;
	unsigned int  ndisps ;
	unsigned int  count ;

	len = strlen( dir) + strlen( file) + 5 ;
	path = malloc( sizeof( char) * ( len + 1)) ;
	sprintf( path , "%s/%s.png" , dir , file);

	disps = x_get_opened_displays( &ndisps) ;
	count = 0 ;
	while( 1)
	{
		/*
		 * XXX Hack
		 * (*disps) == (x_display_t*) == (Display**)
		 * (Display* is the first member of x_display_t.)
		 */
		if( *((Display**)(*disps)) == display)
		{
			break ;
		}

		if( ++count >= ndisps)
		{
			break ;
		}
		else
		{
			disps ++ ;
		}
	}
	
	if( ! x_imagelib_load_file( *disps , path , NULL , pixmap , mask ,
				width , height))
	{
#ifdef __DEBUG
		printf("x_imagelib_load_file() failed\n");;
#endif
	}

#ifdef __DEBUG
	printf(" path: %s, width: %d, height: %d\n", path , *width, *height) ;
#endif
	free( path) ;

	return  ;
}

static shared_image_t *
acquire_shared_image(
	Display *  display ,
	x_sb_view_conf_t *  conf ,
	unsigned int *  width ,		/* not NULL or 0 */
	unsigned int *  btn_up_h ,	/* not NULL */
	unsigned int *  btn_dw_h	/* not NULL */
	)
{
	unsigned int  count ;
	shared_image_t *  si ;
	void *  p ;

	for( count = 0 ; count < num_of_shared_images ; count++)
	{
		if( shared_images[count]->display == display &&
			shared_images[count]->conf == conf)
		{
			if( *btn_up_h == 0)
			{
				*btn_up_h = shared_images[count]->btn_up_h ;
			}
			if( *btn_dw_h == 0)
			{
				*btn_dw_h = shared_images[count]->btn_dw_h ;
			}
			
			shared_images[count]->use_count ++ ;

			return  shared_images[count] ;
		}
	}
	
	if( ( si = calloc( 1 , sizeof( shared_image_t))) == NULL)
	{
		return  NULL ;
	}

	if( ( p = realloc( shared_images , sizeof( shared_image_t*) * (num_of_shared_images + 1)))
		== NULL)
	{
		free( si) ;

		return  NULL ;
	}

	shared_images = p ;
	shared_images[num_of_shared_images++] = si ;

	si->display = display ;
	si->conf = conf ;

	/*
	 * load background images (separated three parts: top, body and bottom.)
	 */
	load_image( display , si->conf->dir , "bg_top" , &si->bg_top , NULL ,
			width , &si->bg_top_h) ;
	load_image( display , si->conf->dir , "bg_bottom" , &si->bg_bottom , NULL ,
				width , &si->bg_bottom_h) ;

	/* up/down buttons */
	load_image( display , si->conf->dir , "button_up" , &si->btn_up ,
			&si->btn_up_mask , width , btn_up_h) ;
	load_image( display , si->conf->dir , "button_down" , &si->btn_dw ,
			&si->btn_dw_mask , width , btn_dw_h) ;
	load_image( display , si->conf->dir , "button_up_pressed" , &si->btn_up_pressed ,
			&si->btn_up_pressed_mask , width , btn_up_h) ;
	load_image( display , si->conf->dir , "button_down_pressed" , &si->btn_dw_pressed ,
			&si->btn_dw_pressed_mask , width , btn_dw_h) ;

	/*
	 * load slider images (separated three parts: top, body and bottom.)
	 */
	load_image( display , si->conf->dir , "slider_top" , &si->slider_top ,
		&si->slider_top_mask , &si->slider_width , &si->slider_top_h) ;
	load_image( display , si->conf->dir , "slider_bottom" , &si->slider_bottom ,
		&si->slider_bottom_mask , &si->slider_width , &si->slider_bottom_h) ;
	load_image( display , si->conf->dir , "slider_knob" , &si->slider_knob ,
		&si->slider_knob_mask , &si->slider_width , &si->slider_knob_h) ;

	si->btn_up_h = *btn_up_h ;
	si->btn_dw_h = *btn_dw_h ;

	si->use_count = 1 ;

#ifdef  __DEBUG
	fprintf( stderr , "Loading new pixmap scrollbar %s\n" , si->conf->sb_name) ;
#endif

	return  si ;
}

static void
release_shared_image(
	shared_image_t *  si
	)
{
	unsigned int  count ;

	if( -- si->use_count > 0)
	{
		return ;
	}

	for( count = 0 ; count < num_of_shared_images ; count++)
	{
		if( shared_images[count] == si)
		{
			shared_images[count] = shared_images[--num_of_shared_images] ;
			if( num_of_shared_images == 0)
			{
				free( shared_images) ;
				shared_images = NULL ;
			}

			break ;
		}
	}

	free_pixmap( si->display , si->bg_top) ;
	free_pixmap( si->display , si->bg_bottom) ;
	free_pixmap( si->display , si->btn_up) ;
	free_pixmap( si->display , si->btn_dw) ;
	free_pixmap( si->display , si->btn_up_pressed) ;
	free_pixmap( si->display , si->btn_dw_pressed) ;
	free_pixmap( si->display , si->btn_up_mask) ;
	free_pixmap( si->display , si->btn_dw_mask) ;
	free_pixmap( si->display , si->btn_up_pressed_mask) ;
	free_pixmap( si->display , si->btn_dw_pressed_mask) ;
	free_pixmap( si->display , si->slider_top) ;
	free_pixmap( si->display , si->slider_bottom) ;
	free_pixmap( si->display , si->slider_knob) ;
	free_pixmap( si->display , si->slider_top_mask) ;
	free_pixmap( si->display , si->slider_bottom_mask) ;
	free_pixmap( si->display , si->slider_knob_mask) ;

#ifdef  __DEBUG	
	fprintf( stderr , "Freeing pixmap scrollbar %s\n" , si->conf->sb_name) ;
#endif

	free( si) ;
}

static void
create_bg_cache(
	pixmap_sb_view_t *  ps
	)
{
	Display *  d ;
	Window  win ;
	GC  gc ;
	int  bg_h ;

	d = ps->view.display ;
	win = ps->view.window ;
	gc = ps->gc ;
	bg_h = ps->view.height ;

	free_pixmap( d , ps->bg_cache) ;

	if( bg_h <= 0)
	{
		/* ps->view.height is larger than 65536 */
		return ;
	}

	if( ! ps->si->bg_top && ! ps->bg_body && ! ps->si->bg_bottom)
	{
		return ;
	}

	ps->bg_cache = XCreatePixmap( d , win , ps->width , bg_h , ps->depth) ;

	if( ps->bg_body_h && ps->bg_body)
	{
		int  cached_body_h ;

		cached_body_h = bg_h - ps->si->bg_top_h - ps->si->bg_bottom_h ;
		if( cached_body_h <= 0)
		{
			/* height of background is too small, do nothing */
		}
		else if( ps->bg_tile)
		{
			XSetTile( d , gc , ps->bg_body) ;
			XSetTSOrigin( d , gc , 0 , 0) ;
			XSetFillStyle( d , gc , FillTiled) ;
			XFillRectangle( d , ps->bg_cache , gc , 0 ,
				ps->si->bg_top_h , ps->width , cached_body_h) ;
		}
		else /* ! ps->bg_tile (scale) */
		{
			free_pixmap( d , ps->bg_body) ;
			load_image( d , ps->conf->dir , "bg_body" ,
				&ps->bg_body , NULL , &ps->width ,
				&cached_body_h) ;
			XCopyArea( d , ps->bg_body , ps->bg_cache , gc ,
				0 , 0 , ps->width , cached_body_h ,
				0 , ps->si->bg_top_h) ;
		}
	}
	else
	{
		XFillRectangle( d , ps->bg_cache , gc , 0 , 0 , ps->width , bg_h) ;
	}

	if( ps->si->bg_top_h && ps->si->bg_top)
	{
		XCopyArea( d , ps->si->bg_top , ps->bg_cache , gc ,
			0 , 0 , ps->width , ps->si->bg_top_h , 0 , 0) ;
	}

	if( ps->si->bg_bottom_h && ps->si->bg_bottom)
	{
		XCopyArea( d , ps->si->bg_bottom , ps->bg_cache , gc ,
			0 , 0 , ps->width , ps->si->bg_bottom_h , 0 ,
			bg_h - ps->si->bg_bottom_h) ;
	}

}

static void
resize_slider(
	pixmap_sb_view_t *  ps ,
	int  body_height
	)
{
	Display *  d ;
	Window  win ;
	GC  gc ;

	d = ps->view.display ;
	win = ps->view.window ;
	gc = ps->gc ;

	if( body_height <= 0 || ! ps->si->slider_width)
	{
		return ;
	}

	free_pixmap( d , ps->slider_tiled_cache) ;

	ps->slider_tiled_cache = XCreatePixmap( d , win , ps->si->slider_width ,
					body_height , ps->depth) ;

	if( ps->slider_body_h && ps->slider_body)
	{
		if( ps->slider_tile)
		{
			/* tile */
			XSetTile( d , gc , ps->slider_body) ;
			XSetTSOrigin( d , gc , 0 , 0) ;
			XSetFillStyle( d , gc , FillTiled) ;
			XFillRectangle( d , ps->slider_tiled_cache , gc , 0 ,
					0 , ps->si->slider_width , body_height) ;
		}
		else
		{
			/* scale */
			free_pixmap( d , ps->slider_body) ;
			free_pixmap( d , ps->slider_body_mask) ;
			load_image( d , ps->conf->dir , "slider_body" ,
				&ps->slider_body , &ps->slider_body_mask ,
				&ps->si->slider_width , &body_height) ;
		}
	}
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
	pixmap_sb_view_t *  ps ;

	ps = (pixmap_sb_view_t*) view ;

	*width =  ps->width ;
	*top_margin =  ps->top_margin ;
	*bottom_margin =  ps->bottom_margin ;

	*up_button_height = ps->btn_up_h ;
	*down_button_height = ps->btn_dw_h ;
	switch( ps->btn_layout)
	{
		case  BTN_NONE :
			*up_button_y = 0 ;
			*down_button_y = 0 ;
			break ;
		case  BTN_NORMAL :
			*up_button_y = 0 ;
			*down_button_y = -ps->btn_dw_h ;
			break ;
		case  BTN_NORTHGRAVITY :
			*up_button_y = 0 ;
			*down_button_y = ps->btn_up_h ;
			break ;
		case  BTN_SOUTHGRAVITY :
			*up_button_y = -(ps->btn_up_h + ps->btn_dw_h) ;
			*down_button_y = -ps->btn_dw_h ;
			break ;
	}

#ifdef __DEBUG
	printf("[geometry] width: %d, top_margin: %d, bottom_margin: %d, up_button_y: %d, up_button_height: %d, down_button_y: %d, down_button_height: %d\n", *width, *top_margin, *bottom_margin, *up_button_y, *up_button_height, *down_button_y, *down_button_height) ;
#endif
}

static void
get_default_color(
	x_sb_view_t *  view ,
	char **  fg_color ,
	char **  bg_color
	)
{
	/* dummy */
	*fg_color = "black" ;
	*bg_color = "gray" ;
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
	pixmap_sb_view_t *  ps ;
	XGCValues  gc_value ;
	XWindowAttributes  attr ;

	ps = (pixmap_sb_view_t*) view ;

	view->display = display ;
	view->screen = screen ;
	view->window = window ;
	view->gc = gc ;
	view->height = height ;

	gc_value.foreground = BlackPixel( display , screen) ;
	gc_value.background = WhitePixel( display , screen) ;
	gc_value.graphics_exposures = 0 ;

	ps->gc = XCreateGC( display , window , GCForeground | GCBackground | GCGraphicsExposures , &gc_value) ;

	XGetWindowAttributes( view->display , view->window , &attr) ;
	ps->depth = attr.depth ;

	ps->si = acquire_shared_image( display , ps->conf , &ps->width ,
			&ps->btn_up_h , &ps->btn_dw_h) ;
	
	/*
	 * load background images (separated three parts: top, body and bottom.)
	 */
	load_image( display , ps->conf->dir , "bg_body" , &ps->bg_body , NULL ,
			&ps->width , &ps->bg_body_h) ;
	create_bg_cache( ps) ;

	/*
	 * load slider images (separated three parts: top, body and bottom.)
	 */
	load_image( display , ps->conf->dir , "slider_body" , &ps->slider_body ,
		&ps->slider_body_mask , &ps->si->slider_width , &ps->slider_body_h) ;

	/*
	 * verify the size
	 */
	if( ps->width < ps->si->slider_width)
	{
		ps->si->slider_width = ps->width ;
	}
}

static void
resized(
	x_sb_view_t *  view ,
	Window  window ,
	unsigned int  height
	)
{
	pixmap_sb_view_t *  ps ;

	ps = (pixmap_sb_view_t*) view ;
	view->window = window ;
	view->height = height ;

	if( ps->is_transparent && ps->bg_enable_trans)
	{
		return ;
	}

	create_bg_cache( ps) ;
}

static void
delete(
	x_sb_view_t *  view
	)
{
	pixmap_sb_view_t *  ps ;

	ps = (pixmap_sb_view_t*) view ;

	if( ! ps)
	{
		return ;
	}

	release_shared_image( ps->si) ;

	free_pixmap( view->display , ps->bg_body) ;
	free_pixmap( view->display , ps->bg_cache) ;
	free_pixmap( view->display , ps->slider_body) ;
	free_pixmap( view->display , ps->slider_body_mask) ;
	free_pixmap( view->display , ps->slider_tiled_cache) ;
	free_pixmap( view->display , ps->slider_tiled_cache_mask) ;

	XFreeGC( view->display , ps->gc) ;

	ps->conf->use_count -- ;

	free( ps) ;
}

static void
draw_button(
	pixmap_sb_view_t *  ps ,
	int up ,
	int pressed
	)
{
	Display *  d ;
	Window  w ;
	GC  gc ;
	unsigned int  up_y = 0 ;
	unsigned int  dw_y = 0 ;
	unsigned int  y ;
	unsigned int  h ;
	Pixmap  src ;
	Pixmap  mask ;

	d = ps->view.display ;
	w = ps->view.window ;
	gc = ps->gc ;

	switch( ps->btn_layout)
	{
		case  BTN_NONE :
			return ;
		case  BTN_NORMAL :
			up_y = 0 ;
			dw_y = ps->view.height - ps->btn_dw_h ;
			break ;
		case  BTN_NORTHGRAVITY :
			up_y = 0 ;
			dw_y = ps->btn_up_h ;
			break ;
		case  BTN_SOUTHGRAVITY :
			up_y = ps->view.height - (ps->btn_up_h + ps->btn_dw_h) ;
			dw_y = ps->view.height - ps->btn_dw_h ;
			break ;
	}

	if( up)
	{
		if( ! ps->si->btn_up_pressed)
		{
			pressed = 0 ;
		}
		src = pressed ?  ps->si->btn_up_pressed : ps->si->btn_up ;
		mask = pressed ?  ps->si->btn_up_pressed_mask : ps->si->btn_up_mask ;
		y = up_y ;
		h = ps->btn_up_h ;
	}
	else
	{
		if( ! ps->si->btn_dw_pressed)
		{
			pressed = 0 ;
		}
		src = pressed ? ps->si->btn_dw_pressed : ps->si->btn_dw ;
		mask = pressed ? ps->si->btn_dw_pressed_mask : ps->si->btn_dw_mask ;
		y = dw_y ;
		h = ps->btn_dw_h ;
	}

	/* background */
	if( ! (ps->is_transparent && ps->bg_enable_trans) && ps->bg_cache)
	{
		XCopyArea( d , ps->bg_cache , w , gc , 0 , y , ps->width , h , 0 , y) ;
	}
	else
	{
		XClearArea( d , w , 0 , y , ps->width , h , 0) ;
	}

	if( ! src)
	{
		return ;
	}

	if( mask)
	{
		XSetClipMask( d , gc , mask) ;
		XSetClipOrigin( d , gc , 0 , y) ;
	}
	XCopyArea( d , src , w , gc , 0 , 0 , ps->width , h , 0 , y) ;
	XSetClipMask(d , gc , None) ;

}

static void
up_button_pressed(
	x_sb_view_t *  view
	)
{
	draw_button( (pixmap_sb_view_t*) view , 1 , 1) ;
}

static void
down_button_pressed(
	x_sb_view_t *  view
	)
{
	draw_button( (pixmap_sb_view_t*) view , 0 , 1) ;
}

static void
up_button_released(
	x_sb_view_t *  view
	)
{
	draw_button( (pixmap_sb_view_t*) view , 1 , 0) ;
}

static void
down_button_released(
	x_sb_view_t *  view
	)
{
	draw_button( (pixmap_sb_view_t*) view , 0 , 0) ;
}

static void
draw_decoration(
	x_sb_view_t *  view
	)
{
	draw_button( (pixmap_sb_view_t*) view , 1 , 0) ;
	draw_button( (pixmap_sb_view_t*) view , 0 , 0) ;
}

static void
draw_scrollbar(
	x_sb_view_t *  view ,
	int  slider_top_y ,
	unsigned int  slider_height
	)
{
	pixmap_sb_view_t *  ps ;
	Display *  d ;
	Window  win ;
	unsigned int  bg_h = 0 ;
	unsigned int  bg_y = 0 ;
	unsigned int  offset_x ;
	int  slr_body_h ;
	GC  gc ;

	ps = (pixmap_sb_view_t*) view ;
	d = view->display ;
	win = view->window ;
	gc = ps->gc ;
	bg_h = ps->view.height - ps->btn_up_h - ps->btn_dw_h ;
	offset_x = (ps->width - ps->si->slider_width) / 2 ;

	/*
	 * background
	 */
	switch( ps->btn_layout)
	{
		case  BTN_NONE :
			bg_y = 0 ;
			break ;
		case  BTN_NORMAL :
			bg_y = ps->btn_up_h ;
			break ;
		case  BTN_NORTHGRAVITY :
			bg_y = ps->btn_up_h + ps->btn_dw_h ;
			break ;
		case  BTN_SOUTHGRAVITY :
			bg_y = 0 ;
			break ;
	}

	if( ! (ps->is_transparent && ps->bg_enable_trans) && ps->bg_cache)
	{
	        XCopyArea( d , ps->bg_cache , win , gc , 0 , bg_y , ps->width ,
	                bg_h , 0 , bg_y) ;
	}
	else
	{
#if 0
		XClearArea( d , win , offset_x , bg_y , ps->slider_width , bg_h , 0) ;
#else
		XClearArea( d , win , 0 , bg_y , ps->width , bg_h , 0) ;
#endif
	}

	/*
	 * slider
	 */
	if( ps->si->slider_top)
	{
	        XSetClipMask( d , gc , ps->si->slider_top_mask) ;
	        XSetClipOrigin( d , gc , offset_x , slider_top_y) ;
	        XCopyArea( d , ps->si->slider_top , win , gc , 0 , 0 ,
	                ps->si->slider_width ,
			ps->si->slider_top_h < slider_height ?
				ps->si->slider_top_h : slider_height ,
			offset_x , slider_top_y) ;
		XSetClipMask(d , gc , None) ;
	}

	if( ps->si->slider_bottom)
	{
		unsigned int  y ;
		y = slider_top_y + slider_height - ps->si->slider_bottom_h ;
		XSetClipMask( d , gc , ps->si->slider_bottom_mask) ;
		XSetClipOrigin( d , gc , offset_x , y) ;
		XCopyArea( d , ps->si->slider_bottom , win , gc , 0 , 0 ,
			ps->si->slider_width ,
			ps->si->slider_bottom_h < slider_height ?
				ps->si->slider_bottom_h : slider_height ,
			offset_x , y) ;
		XSetClipMask(d , gc , None) ;
	}

	slr_body_h = slider_height - ps->si->slider_top_h - ps->si->slider_bottom_h ;

	if( ( ps->slider_tile && slider_height > ps->pre_slider_h) ||
		( ! ps->slider_tile && ps->pre_slider_h != slider_height))
	{
		resize_slider( ps, slr_body_h) ;
	}
	ps->pre_slider_h = slider_height ;

	if( slr_body_h <= 0)
	{
		return ;
	}

	if( ps->slider_body)
	{
		if( ps->slider_tile)
		{
			XCopyArea( d , ps->slider_tiled_cache ,
				win , gc , 0 , 0 ,
				ps->si->slider_width , slr_body_h ,
				offset_x , slider_top_y + ps->si->slider_top_h) ;
		}
		else /* ! ps->slider_tile (scale) */
		{
			XSetClipMask( d , gc , ps->slider_body_mask) ;
			XSetClipOrigin( d , gc , offset_x , slider_top_y + ps->si->slider_top_h) ;
			XCopyArea( d , ps->slider_body ,
				win , gc , 0 , 0 ,
				ps->si->slider_width , slr_body_h ,
				offset_x , slider_top_y + ps->si->slider_top_h) ;
			XSetClipMask(d , gc , None) ;
		}
	}

	if( ps->si->slider_knob && slr_body_h > ps->si->slider_knob_h)
	{
		int knob_y ;
		knob_y = slider_top_y + (slider_height - ps->si->slider_knob_h)/2 ;

		XSetClipMask( d , gc , ps->si->slider_knob_mask) ;
		XSetClipOrigin( d , gc , offset_x , knob_y) ;
		XCopyArea( d , ps->si->slider_knob , win , gc , 0 , 0 ,
				ps->si->slider_width , ps->si->slider_knob_h ,
				offset_x , knob_y) ;
		XSetClipMask(d , gc , None) ;
	}
}

static int
parse(
	pixmap_sb_view_t *  ps ,
	x_sb_view_conf_t *  conf
	)
{
	int  count ;
	x_sb_view_rc_t *  p ;

	for( p = conf->rc , count = 0 ; count < conf->rc_num ; p ++ , count ++)
	{
		if( strcmp( p->key , "width") == 0)
		{
			STR2NUM_WITH_RANGE_CHECK(p->value , ps->width) ;
		}
		else if( strcmp( p->key , "button_up_height") == 0)
		{
			STR2NUM_WITH_RANGE_CHECK(p->value , ps->btn_up_h) ;
		}
		else if( strcmp( p->key , "button_down_height") == 0)
		{
			STR2NUM_WITH_RANGE_CHECK(p->value , ps->btn_dw_h) ;
		}
		else if( strcmp( p->key , "top_margin") == 0)
		{
			STR2NUM_WITH_RANGE_CHECK(p->value , ps->top_margin) ;
		}
		else if( strcmp( p->key , "bottom_margin") == 0)
		{
			STR2NUM_WITH_RANGE_CHECK(p->value , ps->bottom_margin) ;
		}
		else if( strcmp( p->key , "bg_tile") == 0)
		{
			if( strcmp( p->value , "false") == 0)
			{
				ps->bg_tile = 0 ;
			}
		}
		else if( strcmp( p->key , "bg_enable_trans") == 0)
		{
			if( strcmp( p->value , "true") == 0)
			{
				ps->bg_enable_trans = 1 ;
			}
		}
		else if( strcmp( p->key , "button_layout") == 0)
		{
			if( strcmp( p->value , "none") == 0)
			{
				ps->btn_layout = BTN_NONE ;
			}
			if( strcmp( p->value , "northgravity") == 0)
			{
				ps->btn_layout = BTN_NORTHGRAVITY ;
			}
			if( strcmp( p->value , "southgravity") == 0)
			{
				ps->btn_layout = BTN_SOUTHGRAVITY ;
			}
		}
		else if( strcmp( p->key , "slider_tile") == 0)
		{
			if( strcmp( p->value , "false") == 0)
			{
				ps->slider_tile = 0 ;
			}
		}
#ifdef __DEBUG
		else
		{
			printf("unknown key: %s\n" , p->key);
		}
#endif
	}

	return  1 ;
}


/* --- global functions --- */

x_sb_view_t *
x_pixmap_engine_sb_engine_new(
	x_sb_view_conf_t *  conf ,
	int  is_transparent
	)
{
	pixmap_sb_view_t *  ps ;

	if( ! conf)
	{
		return NULL ;
	}

	if( ( ps = malloc( sizeof( pixmap_sb_view_t))) == NULL)
	{
		return  NULL ;
	}

	/* Initialization and default settings */
	memset( ps , 0 , sizeof( pixmap_sb_view_t)) ;
	ps->bg_tile = 1 ;
	ps->btn_layout = BTN_NORMAL ;
	ps->slider_tile = 1 ;

	ps->conf = conf ;

	if( ! ( parse( ps , conf)))
	{
		free( ps) ;
		return  NULL ;
	}

	/* verify the values */
	if( ps->width == 0)
	{
		free( ps) ;
		return  NULL ;
	}

	if( ps->btn_layout == BTN_NONE && ( ps->btn_up_h || ps->btn_dw_h))
	{
		ps->btn_up_h = 0 ;
		ps->btn_dw_h = 0 ;
	}

	/* event handlers */
	ps->view.get_geometry_hints = get_geometry_hints ;
	ps->view.get_default_color = get_default_color ;
	ps->view.realized = realized ;
	ps->view.resized = resized ;
	ps->view.delete = delete ;
	ps->view.draw_decoration = draw_decoration ;
	ps->view.draw_scrollbar = draw_scrollbar ;
	ps->view.up_button_pressed = up_button_pressed ;
	ps->view.down_button_pressed = down_button_pressed ;
	ps->view.up_button_released = up_button_released ;
	ps->view.down_button_released = down_button_released ;

	ps->si = NULL ;
	
	ps->is_transparent = is_transparent ;

	/* use_count decrement. when it is 0, this plugin will be unloaded. */
	conf->use_count ++ ;

	return  (x_sb_view_t*) ps ;
}


