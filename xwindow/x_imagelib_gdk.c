/** @file
 *  @brief image handling functions using gdk-pixbuf
 *	$Id$
 */

#include <math.h>                        /* pow */
#include <X11/Xatom.h>                   /* XInternAtom */
#include <X11/Xutil.h>
#include <string.h>                      /* memcpy */
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <kiklib/kik_types.h> /* u_int32_t/u_int16_t */
#include <kiklib/kik_unistd.h>
#include <kiklib/kik_str.h>    /* strdup */

#include "x_imagelib.h"

/** Pixmap cache per display. */
typedef struct display_store_tag {
	Display *  display; /**<Display */
	Pixmap  root;      /**<Root pixmap !!! NOT owned by mlterm. DON'T FREE !!!*/
	Pixmap  cooked;    /**<Background pixmap cache*/
	x_picture_modifier_t  pic_mod; /**<modification applied to "cooked"*/
	struct display_store_tag *  next ;
} display_store_t ;

/* --- static variables --- */

static int  display_count = 0 ;
static unsigned char  gamma_cache[256 +1] ;
static display_store_t *  display_store = NULL ;
static char *  wp_cache_name = NULL ;
static GdkPixbuf *  wp_cache_data = NULL ;
static int  wp_cache_width = 0 ;
static int  wp_cache_height = 0 ;
static GdkPixbuf *  wp_cache_scaled = NULL ;

static int modify_ubound = 255 ;
static int modify_lbound = 0 ;

/* --- static functions --- */

/* Get an background pixmap from _XROOTMAP_ID */

static Pixmap
tile_pixmap(
	Display * display,
	int   screen,
	GC  gc ,
	Pixmap pixmap,
	int force_copy
	)
{
	Window dummy ;
	int ax,ay ;
	unsigned int aw, ah;
	unsigned int bw, depth;
	Pixmap result;

	XGetGeometry( display, pixmap, &dummy, &ax, &ay,
		      &aw, &ah, &bw, &depth) ;
	if ( force_copy ||
	     ( aw < DisplayWidth( display, screen)) ||
	     ( ah < DisplayHeight( display, screen)))
	{
		/* Some WM needs tiling... sigh.*/
		result = XCreatePixmap( display, pixmap,
					       DisplayWidth( display, screen),
					       DisplayHeight( display, screen),
					       depth) ;
		XSetTile( display, gc, pixmap) ;
		XSetTSOrigin( display, gc, 0, 0) ;
		XSetFillStyle( display, gc, FillTiled) ;
		XFillRectangle( display, result, gc, 0, 0,
				DisplayWidth( display, screen),
				DisplayHeight( display, screen)) ;
		return result ;
	}
	return pixmap ;
}

static Pixmap
root_pixmap(
	x_window_t *  win
	)
{
	Atom  id ;
	int  act_format ;
	u_long  nitems ;
	u_long  bytes_after ;
	u_char *  prop ;

	id = XInternAtom( win->display, "_XROOTPMAP_ID", True) ;

	if( !id)
		return None ;

	if( XGetWindowProperty( win->display, DefaultRootWindow(win->display), id, 0, 1,
				False, XA_PIXMAP, &id, &act_format,
				&nitems, &bytes_after, &prop) == Success)
	{
		if( prop)
		{
			Pixmap root ;
			root = *((Drawable *)prop) ;
			XFree( prop) ;
			return root ;
		}
	}
	return None ;
}

static display_store_t *
cache_seek(
	   Display *  display
	   )
{
	display_store_t *  cache = display_store ;

	while( cache)
	{
		if( cache->display == display)
			break ;
		cache = cache->next ;
	}
	return cache ;
}

static display_store_t *
cache_add(
	   Display *  display
	   )
{
	display_store_t *  cache ;

	cache = malloc( sizeof( display_store_t)) ;
	if (!cache)
		return NULL ;
	cache->display = display ;
	cache->root = None ;
	cache->cooked = None ;
	memset( &(cache->pic_mod), 0, sizeof(x_picture_modifier_t)) ;
	cache->next = display_store ;
	display_store = cache ;

	return cache ;
}

/**Remove a cache for the display
 *
 *\param display display to be removed from the cache.
 *
 */
static void
cache_delete(
	Display *  display
	)
{
	display_store_t *  cache ;
	display_store_t *  o ;

	o = NULL ;
	cache = display_store ;
	while( cache)
	{
		if ( cache->display == display)
		{
			if ((cache->cooked) && (cache->root != cache->cooked))
				XFreePixmap( display, cache->cooked) ;
			if ( o == NULL)
			{
				display_store = cache->next ;
				free( cache) ;
				cache = display_store ;
			}
			else
			{
				o->next = cache->next ;
				free( cache) ;
				cache = o->next;
			}
		}
		else
		{
			o = cache ;
			cache = cache->next ;
		}
	}
}

/**judge whether pic_mods are equal or not
 *\param a,b picture modifier
 *\return 1 when they are same. 0 when not.
 */
static int
is_picmod_eq(
	display_store_t *  cache,
	x_picture_modifier_t *  new
	)
{
	if (new == NULL)
	{
		if( (cache->pic_mod.brightness == 100) &&
		    (cache->pic_mod.contrast == 100) &&
		    (cache->pic_mod.gamma == 100))
		{
			return 1 ;
		}
	}
	else
	{
		if( (cache->pic_mod.brightness == new->brightness) &&
		    (cache->pic_mod.contrast == new->contrast) &&
		    (cache->pic_mod.gamma == new->gamma))
		{
			return 1 ;
		}
	}
	return 0 ;
}

/**Return position of the least significant bit
 *
 *\param val value to count
 *
 */
static int
lsb(
	unsigned int  val
	)
{
	int nth = 0 ;

	if( val == 0)
		return 0 ;
	while((val & 1) == 0)
	{
		val = val >>1 ;
		nth ++ ;
	}
	return nth ;
}

/**Return position of the most significant bit
 *
 *\param val value to count
 *
 */
static int
msb(
	unsigned int val
	)
{
	int nth ;
	if( val == 0)
		return 0 ;
	nth = lsb( val) +1 ;
	while(val & (1 << nth))
		nth++ ;

	return nth ;
}


/**convert a pixbuf into a pixmap
 *
 *\param display
 *\param screen
 *\param pixbuf source
 *\param pixmap where an image is rendered(should be created before calling this function)
 *
 */
static XImage *
pixbuf_to_pixmap_truecolor(
	Display *  display,
	int  screen,
	GdkPixbuf *  pixbuf,
	Pixmap  pixmap,
	int  depth ,
	XVisualInfo * vinfo
	)
{
	unsigned int  i, j ;
	unsigned int  width, height, rowstride, bytes_per_pixel ;
	unsigned char *  line ;
	unsigned char *  pixel ;
	long  r_mask, g_mask, b_mask ;
	int  r_offset, g_offset, b_offset;

	XImage *  image = NULL;

	width = gdk_pixbuf_get_width (pixbuf) ;
	height = gdk_pixbuf_get_height (pixbuf) ;

	r_mask = vinfo[0].red_mask ;
	g_mask = vinfo[0].green_mask ;
	b_mask = vinfo[0].blue_mask ;
	r_offset = lsb( r_mask) ;
	g_offset = lsb( g_mask) ;
	b_offset = lsb( b_mask) ;

	bytes_per_pixel = (gdk_pixbuf_get_has_alpha( pixbuf)) ? 4:3 ;
	rowstride = gdk_pixbuf_get_rowstride (pixbuf) ;
	line = gdk_pixbuf_get_pixels (pixbuf) ;

	switch ( depth)
	{
	case 15:
	case 16:
	{
		int r_limit, g_limit, b_limit ;
		u_int16_t *data ;

		data = (u_int16_t *)malloc( width *  height * 2) ;
		if( !data)
			return NULL ;
		image = XCreateImage( display, DefaultVisual( display, screen),
				      DefaultDepth( display, screen), ZPixmap, 0,
				      (char *)data,
				      gdk_pixbuf_get_width( pixbuf),
				      gdk_pixbuf_get_height( pixbuf),
				      16,
				      gdk_pixbuf_get_width( pixbuf) *  2);
		r_limit = 8 + r_offset - msb( r_mask) ;
		g_limit = 8 + g_offset - msb( g_mask) ;
		b_limit = 8 + b_offset - msb( b_mask) ;
		for (i = 0; i < height; i++)
		{
			pixel = line ;
			for (j = 0; j < width; j++)
			{
				data[i*width +j ] =
					(((pixel[0] >> r_limit) << r_offset) & r_mask) |
					(((pixel[1] >> g_limit) << g_offset) & g_mask) |
					(((pixel[2] >> b_limit) << b_offset) & b_mask) ;
				pixel += bytes_per_pixel ;
			}
			line += rowstride ;
		}
		break;
	}
	case 24:
	case 32:
	{
		u_int32_t *  data ;

		data = (u_int32_t *)malloc( width *  height * 4) ;
		if( !data)
			return NULL;
		image = XCreateImage( display, DefaultVisual( display, screen),
				      DefaultDepth( display, screen), ZPixmap, 0,
				      (char *)data,
				      gdk_pixbuf_get_width( pixbuf),
				      gdk_pixbuf_get_height( pixbuf),
				      32,
				      gdk_pixbuf_get_width( pixbuf) *  4) ;
		for( i = 0; i < height; i++){
			pixel = line ;
			for( j = 0; j < width; j++){
				data[i*width +j ] = pixel[0] <<r_offset | pixel[1] <<g_offset | pixel[2]<<b_offset ;
				pixel +=bytes_per_pixel ;
			}
			line += rowstride ;
		}
		break ;
	}
	default:
		break;
	}

	return image ;
}

static void
pixbuf_to_pixmap(
	Display *  display,
	int  screen,
	GdkPixbuf *  pixbuf,
	Pixmap  pixmap
	)
{
	XImage *  image = NULL;
	int  matched ;
	XVisualInfo *  vinfolist ;
	XVisualInfo  vinfo ;

	if( !pixbuf)
		return ;

	vinfo.visualid = XVisualIDFromVisual( DefaultVisual( display, screen)) ;
	if (!vinfo.visualid)
		return ;
	vinfolist = XGetVisualInfo( display, VisualIDMask, &vinfo, &matched) ;
	if( !vinfolist)
		return ;
	if ( !matched)
	{
		XFree( vinfolist) ;
		return ;
	}
	switch( vinfolist[0].class)
	{
	case TrueColor:
		image = pixbuf_to_pixmap_truecolor( display,
						    screen,
						    pixbuf,
						    pixmap,
						    DefaultDepth( display, screen),
						    vinfolist) ;
		break ;
	default:
		break ;
	}
	XFree( vinfolist) ;

	if( image)
	{
		XPutImage( display, pixmap, DefaultGC( display, screen), image, 0, 0, 0, 0,
			   gdk_pixbuf_get_width( pixbuf),gdk_pixbuf_get_height( pixbuf)) ;
		XDestroyImage( image) ;
	}

}

static XImage *
compose__truecolor(
	Display *  display,
	int  screen,
	GdkPixbuf *  pixbuf,
	Pixmap  pixmap,
	int  depth,
	XVisualInfo * vinfo
	)
{
	XImage *  image ;

	unsigned int i, j ;
	unsigned int width, height, rowstride ;
	unsigned char *line ;
	unsigned char *pixel ;

	int r_offset, g_offset, b_offset;
	long r_mask, g_mask, b_mask ;
	long r, g, b ;

	width = gdk_pixbuf_get_width (pixbuf) ;
	height = gdk_pixbuf_get_height (pixbuf) ;

	image = XGetImage( display, pixmap, 0, 0, width, height, AllPlanes, ZPixmap) ;
	r_mask = vinfo[0].red_mask ;
	g_mask = vinfo[0].green_mask ;
	b_mask = vinfo[0].blue_mask ;
	r_offset = lsb( r_mask) ;
	g_offset = lsb( g_mask) ;
	b_offset = lsb( b_mask) ;

	rowstride = gdk_pixbuf_get_rowstride( pixbuf) ;
	line = gdk_pixbuf_get_pixels( pixbuf) ;

	switch( depth)
	{
	case 24:
	case 32:
	{
		u_int32_t *  data ;
		data = (u_int32_t *)(image->data) ;
		for( i = 0; i < height; i++){
			pixel = line ;
			for( j = 0; j < width; j++){
				if(pixel[3] != 0)
				{
					if (pixel[3] != 0xFF)
					{
						r = ((*data) >>r_offset) & 0xFF ;
						g = ((*data) >>g_offset) & 0xFF ;
						b = ((*data) >>b_offset) & 0xFF ;

						r = (r*(256 - pixel[3]) + pixel[0] *  pixel[3])>>8 ;
						g = (g*(256 - pixel[3]) + pixel[1] *  pixel[3])>>8 ;
						b = (b*(256 - pixel[3]) + pixel[2] *  pixel[3])>>8 ;

						*data =	(r <<r_offset ) |
							(g <<g_offset ) |
							(b <<b_offset ) ;
					}
					else
					{
						*data =	(pixel[0] <<r_offset ) |
							(pixel[1] <<g_offset ) |
							(pixel[2] <<b_offset ) ;
					}				       
				}
				data++ ;
				pixel += 4 ;
			}
			line += rowstride ;
		}
	}
	default:
		break;
	}

	return image ;
}

static void
compose_to_pixmap(
	Display *  display,
	int  screen,
	GdkPixbuf *  pixbuf,
	Pixmap  pixmap)
{
	int  matched ;
	XVisualInfo *  vinfolist ;
	XVisualInfo  vinfo ;
	XImage *  image = NULL ;

	if(!pixbuf)
		return ;
	vinfo.visualid = XVisualIDFromVisual( DefaultVisual( display, screen)) ;
	if (!vinfo.visualid)
		return ;
	vinfolist = XGetVisualInfo( display, VisualIDMask, &vinfo, &matched) ;
	if( !vinfolist)
		return ;
	if ( !matched)
	{
		XFree( vinfolist) ;
		return ;
	}

	switch( vinfolist[0].class)
	{
	case TrueColor:
		image = compose__truecolor( display,
					     screen,
					     pixbuf,
					     pixmap,
					     DefaultDepth( display, screen),
					     vinfolist) ;
		break;
	default:
		break;
	}
	if (image)
	{
		XPutImage( display, pixmap, DefaultGC( display, screen), image, 0, 0, 0, 0,
			   gdk_pixbuf_get_width( pixbuf), gdk_pixbuf_get_height( pixbuf)) ;
		XDestroyImage( image) ;
	}
	XFree( vinfolist) ;
}

static void
pixbuf_to_pixmap_and_mask(
	Display *  display,
	int  screen,
	GdkPixbuf *  pixbuf,
	Pixmap  pixmap,
	Pixmap  mask
	)
{
	GC  gc ;
	XGCValues  gcv ;
	int  i, j ;
	int  width, height, rowstride, bytes_per_pixel ;
	unsigned char *  line ;
	unsigned char *  pixel ;

	width = gdk_pixbuf_get_width (pixbuf) ;
	height = gdk_pixbuf_get_height (pixbuf) ;

	pixbuf_to_pixmap( display, screen, pixbuf, pixmap) ;

	gc = XCreateGC (display, mask, 0, &gcv) ;
	bytes_per_pixel = (gdk_pixbuf_get_has_alpha( pixbuf)) ? 4:3 ;
	if (bytes_per_pixel == 3){ /* no mask */
		XSetForeground( display, gc, 1) ;
		XFillRectangle( display, mask, gc,
				0, 0, width, height) ;

	}else{
		XSetForeground( display, gc, 0) ;
		XFillRectangle( display, mask, gc,
				0, 0, width, height) ;
		XSetForeground( display, gc, 1) ;
		line = gdk_pixbuf_get_pixels (pixbuf) ;
		rowstride = gdk_pixbuf_get_rowstride (pixbuf) ;
		for (i = 0; i < height; i++) {
			pixel = line ;
       			line += rowstride ;
			for (j = 0; j < width; j++) {
				if( pixel[3] > 127)
					XDrawPoint( display, mask, gc, j, i) ;
				pixel += 4 ;
			}
		}
	}
	XFreeGC( display, gc) ;
}

static void
modify_bound(
	x_picture_modifier_t *  pic_mod
	)
{
	if( pic_mod->contrast > 0){
		modify_ubound =(256*100 - 128*pic_mod->brightness)/pic_mod->contrast + 128 ;
		modify_lbound =(        - 128*pic_mod->brightness)/pic_mod->contrast + 128 ;

		if (modify_ubound > 255)
			modify_ubound = 255 ;
		if (modify_ubound < 0)
			modify_ubound = 0 ;
	}
}

static unsigned char
modify_color(
	unsigned char  value,
	x_picture_modifier_t *  pic_mod
	)
{
	int result ;

	if( value > modify_ubound)
		return 255;
	if( value < modify_lbound)
		return 0;

	result = pic_mod->contrast*(value - 128)/100 + 128 *  pic_mod->brightness/100 ;
	return (unsigned char)(result) ;
}

static void
gamma_cache_refresh(
int gamma
	)
{
	int i ;
	double real_gamma ;

	if( gamma == gamma_cache[256])
		return ;
	memset(gamma_cache, 0, sizeof(gamma_cache)) ;
	gamma_cache[256] = gamma %256;

	real_gamma = (double)gamma / 100 ;
	i = 128 ;
	while( i > 0){
		gamma_cache[i] = 255 *  pow((double)i/255, real_gamma) ;
		if (gamma_cache[i--] == 0)
			break ;
	}
	while( i >= 0)
		gamma_cache[i--] = 0;
	i = 128 ;
	while( i < 254){
		gamma_cache[i] = 255 *  pow((double)i / 255, real_gamma) ;
		if (gamma_cache[i++] == 255)
			break ;
	}
	while( i <= 255)
		gamma_cache[i++] = 255 ;
}

static int
modify_image(
	GdkPixbuf *  pixbuf,
	x_picture_modifier_t *  pic_mod
	)
{
	int i, j ;
	int width, height, rowstride, bytes_per_pixel ;
	unsigned char *line ;
	unsigned char *pixel ;

	if ( !pixbuf )
			return 0 ;
	bytes_per_pixel = (gdk_pixbuf_get_has_alpha( pixbuf)) ? 4:3 ;
	width = gdk_pixbuf_get_width (pixbuf) ;
	height = gdk_pixbuf_get_height (pixbuf) ;
	rowstride = gdk_pixbuf_get_rowstride (pixbuf) ;

	if( !pic_mod)
		return 1 ;
	if(pic_mod->brightness == 100 && pic_mod->contrast == 100 && pic_mod->gamma == 100)
		return 1 ;
	modify_bound( pic_mod);
       	line = gdk_pixbuf_get_pixels (pixbuf) ;
	for (i = 0; i < height; i++) {
		pixel = line ;
		line += rowstride ;

		for (j = 0; j < width; j++) {
/* XXX keeps neither hue nor saturation. MUST be replaced by another better color model(CIE Yxy? lab?)*/
			pixel[0] = modify_color(pixel[0], pic_mod) ;
			pixel[1] = modify_color(pixel[1], pic_mod) ;
			pixel[2] = modify_color(pixel[2], pic_mod) ;
			pixel += bytes_per_pixel ;
		}
	}

	if( pic_mod->gamma != 100){
		gamma_cache_refresh( pic_mod->gamma) ;
		line = gdk_pixbuf_get_pixels (pixbuf) ;
		for (i = 0; i < height; i++) {
			pixel = line ;
			line += rowstride ;
			for (j = 0; j < width; j++) {
				pixel[0] = gamma_cache[pixel[0]] ;
				pixel[1] = gamma_cache[pixel[1]] ;
				pixel[2] = gamma_cache[pixel[2]] ;
				/* alpha plane is not changed */
				pixel += bytes_per_pixel ;
			}
		}
	}
	return 1 ;
}

static int
modify_pixmap(
	Display *  display,
	int  screen,
	Pixmap  pixmap,
	x_picture_modifier_t *  pic_mod
	)
{
	int  i, j ;
	int  width, height ;
	int  border, depth, x, y ;
	Window  root ;
	XImage *  image ;
	unsigned char  r,g,b ;

	int  r_offset, g_offset, b_offset ;
	int  r_mask, g_mask, b_mask ;
	int  matched ;
	XVisualInfo *  vinfolist ;
	XVisualInfo  vinfo ;

	if ( !pixmap )
		return 0 ;

	if(pic_mod->brightness == 100 && pic_mod->contrast == 100 && pic_mod->gamma == 100)
		return 0 ;

	vinfo.visualid = XVisualIDFromVisual( DefaultVisual( display, screen)) ;
	vinfolist = XGetVisualInfo( display, VisualIDMask, &vinfo, &matched) ;
	if( !vinfolist)
		return 0;
	if ( !matched)
	{
		XFree( vinfolist) ;
		return 0;
	}

	XGetGeometry( display, pixmap, &root, &x, &y,
		      &width, &height, &border, &depth) ;
	image = XGetImage( display, pixmap, 0, 0, width, height, AllPlanes, ZPixmap) ;

	switch( vinfolist[0].class)
	{
	case TrueColor:
		r_mask = vinfolist[0].red_mask ;
		g_mask = vinfolist[0].green_mask ;
		b_mask = vinfolist[0].blue_mask ;

		r_offset = lsb( r_mask) ;
		g_offset = lsb( g_mask) ;
		b_offset = lsb( b_mask) ;

		switch( depth){
		case 1:
			/* XXX not yet supported */
			break ;
		case 8:
			/* XXX not yet supported */
			break ;
		case 15:
		case 16:
		{
			int r_limit, g_limit, b_limit ;
			u_int16_t *  data;

			data = (u_int16_t *)(image->data) ;
			r_limit = 8 + r_offset - msb( r_mask) ;
			g_limit = 8 + g_offset - msb( g_mask) ;
			b_limit = 8 + b_offset - msb( b_mask) ;
			modify_bound( pic_mod);
			if (pic_mod->gamma == 100){
				for (i = 0; i < height; i++) {
					for (j = 0; j < width; j++) {
						r = ((*data & r_mask) >> r_offset)<<r_limit ;
						g = ((*data & g_mask) >> g_offset)<<g_limit ;
						b = ((*data & b_mask) >> b_offset)<<b_limit ;

						r = modify_color( r, pic_mod) ;
						g = modify_color( g, pic_mod) ;
						b = modify_color( b, pic_mod) ;

						*data = (r >> r_limit) << r_offset |
							(g >> g_limit) << g_offset |
							(b >> b_limit) << b_offset ;
						data++;
					}
				}
			}else{
				gamma_cache_refresh( pic_mod->gamma) ;
				for (i = 0; i < height; i++) {
					for (j = 0; j < width; j++) {
						r = ((*data & r_mask) >> r_offset)<<r_limit ;
						g = ((*data & g_mask) >> g_offset)<<g_limit ;
						b = ((*data & b_mask) >> b_offset)<<b_limit ;

						r = gamma_cache[modify_color( r, pic_mod)] ;
						g = gamma_cache[modify_color( g, pic_mod)] ;
						b = gamma_cache[modify_color( b, pic_mod)] ;

						*data = (r >> r_limit) << r_offset |
							(g >> g_limit) << g_offset |
							(b >> b_limit) << b_offset ;
						data++;
					}
				}
			}
			XPutImage( display, pixmap, DefaultGC( display, screen),
				   image, 0, 0, 0, 0, width, height) ;
			break ;
		}
		case 24:
		case 32:
		{
			u_int32_t *  data ;
			data = (u_int32_t *)(image->data) ;
			modify_bound( pic_mod);
			if (pic_mod->gamma == 100){
				for (i = 0; i < height; i++) {
					for (j = 0; j < width; j++) {
						r = ((*data) >>r_offset) & 0xFF ;
						g = ((*data) >>g_offset) & 0xFF ;
						b = ((*data) >>b_offset) & 0xFF ;

						r = modify_color( r, pic_mod) ;
						g = modify_color( g, pic_mod) ;
						b = modify_color( b, pic_mod) ;

						*data = r << r_offset | g << g_offset | b << b_offset ;
						data++ ;
					}
				}
			}else{
				gamma_cache_refresh( pic_mod->gamma) ;
				for (i = 0; i < height; i++) {
					for (j = 0; j < width; j++) {
						r = ((*data) >>r_offset) & 0xFF ;
						g = ((*data) >>g_offset) & 0xFF ;
						b = ((*data) >>b_offset) & 0xFF ;

						r = gamma_cache[modify_color( r, pic_mod)] ;
						g = gamma_cache[modify_color( g, pic_mod)] ;
						b = gamma_cache[modify_color( b, pic_mod)] ;

						*data = r << r_offset | g << g_offset | b << b_offset ;
						data++ ;
					}
				}
			}
			XPutImage( display, pixmap, DefaultGC( display, screen), image, 0, 0, 0, 0,
				   width, height) ;
			break ;
		}
		default:
			break;
		}
	}
	XFree(vinfolist) ;
	XDestroyImage( image) ;
	return 1 ;
}

/* --- global functions --- */

int
x_imagelib_display_opened(
	Display *  display
	)
{
	int i ;
	if (display_count == 0){
#ifndef OLD_GDK_PIXBUF
		g_type_init() ;
#endif
		for (i = 0; i < 256; i++){
			gamma_cache[i] = i ;
		}
		gamma_cache[256] = 100 ;
	}
	display_count ++ ;
	return 1 ;
}

int
x_imagelib_display_closed(
	Display *  display
	)
{
	display_count -- ;
	cache_delete( display) ;
	return 1 ;
}

/** Load an image from the specified file.
 *\param win mlterm window.
 *\param path File full path.
 *\param pic_mod picture modifier.
 *
 *\return Pixmap to be used as a window's background.
 */
Pixmap
x_imagelib_load_file_for_background(
	x_window_t *  win,
	char *  file_path,
	x_picture_modifier_t *  pic_mod
	)
{
	GdkPixbuf *  pixbuf ;
	Pixmap pixmap ;

	if(!file_path)
		return None ;
	if( wp_cache_name && (strcmp( wp_cache_name, file_path) == 0) && wp_cache_data && (!pic_mod)){
		pixbuf = wp_cache_data ;
	}else{
		if ( wp_cache_data){
			gdk_pixbuf_unref( wp_cache_data ) ;
			wp_cache_data = NULL ;
		}
		if ( wp_cache_scaled){
			gdk_pixbuf_unref( wp_cache_scaled ) ;
			wp_cache_scaled = NULL ;
		}
		if ( wp_cache_name){
			free( wp_cache_name) ;
			wp_cache_name = NULL ;
		}
#ifndef OLD_GDK_PIXBUF
		if( ( pixbuf = gdk_pixbuf_new_from_file( file_path, NULL )) == NULL)
			return None ;
#else
		if( ( pixbuf = gdk_pixbuf_new_from_file( file_path )) == NULL)
			return None ;
#endif /*OLD_GDK_PIXBUF*/
		wp_cache_data = pixbuf ;
		wp_cache_name = strdup( file_path) ;

		wp_cache_height = 0 ;
		wp_cache_width = 0 ;
	}

	pixmap = XCreatePixmap( win->display, win->my_window,
				ACTUAL_WIDTH(win), ACTUAL_HEIGHT(win),
				DefaultDepth( win->display, win->screen));

	if (wp_cache_width != ACTUAL_WIDTH(win) || ACTUAL_HEIGHT(win) != wp_cache_height){
		/* it's new pixbuf */
		pixbuf = gdk_pixbuf_scale_simple(pixbuf, ACTUAL_WIDTH(win), ACTUAL_HEIGHT(win),
						 GDK_INTERP_TILES); /* use one of _NEAREST, _TILES, _BILINEAR, _HYPER (speed<->quality) */

		if( pic_mod)
			modify_image( pixbuf, pic_mod) ;

		wp_cache_scaled = pixbuf ;
		wp_cache_width = ACTUAL_WIDTH(win) ;
		wp_cache_height = ACTUAL_HEIGHT(win) ;
	}else
		pixbuf = wp_cache_scaled ;

	if( gdk_pixbuf_get_has_alpha ( pixbuf) ){
		pixmap = x_imagelib_get_transparent_background( win, NULL) ;
		compose_to_pixmap( win->display,
				   win->screen,
				   pixbuf,
				   pixmap) ;
	}else{
		pixbuf_to_pixmap( win->display,
				  win->screen,
				  pixbuf,
				  pixmap) ;
	}
	return pixmap ;
}

/** Answer whether pseudo transparency is available
 *\param display connection to X server.
 *
 *\return Success => 1, Failure => 0
 */
int
x_imagelib_root_pixmap_available(
	Display *  display
	)
{
	if( XInternAtom( display, "_XROOTPMAP_ID", True))
		return 1 ;
	return 0 ;
}

/** Create an pixmap from root window
 *\param win window structure
 *\param pic_mod picture modifier
 *
 *\return Newly allocated Pixmap (or None in the case of failure)
 */
Pixmap
x_imagelib_get_transparent_background(
	x_window_t *  win,
	x_picture_modifier_t *  pic_mod
	)
{
	int  x ;
	int  y ;
	int  pix_x ;
	int  pix_y ;
	u_int  width ;
	u_int  height ;
	display_store_t *  cache ;
	Pixmap  pixmap ;
	Pixmap  current_root ;
	GC  gc ;

	current_root =  root_pixmap( win) ;

	if(current_root == None)
		return None;

	cache = cache_seek( win->display);
	if( cache)
	{
/* discard old info */
		if ( cache->root !=  current_root)
		{
			cache->root = current_root ;
			if((cache->cooked != None) && (cache->cooked != cache->root))
			{
				XFreePixmap( win->display, cache->cooked) ;
			}
			cache->cooked = None ;
		}
	}
	else
	{
		cache = cache_add( win->display);
		if( !cache)
			return None ;
		cache->root = current_root ;
	}

	if( ! x_window_get_visible_geometry( win, &x, &y, &pix_x, &pix_y, &width, &height))
		return None ;

	/* The pixmap to be returned */
	pixmap = XCreatePixmap( win->display, win->my_window, ACTUAL_WIDTH(win), ACTUAL_HEIGHT(win),
				DefaultDepth( win->display, win->screen)) ;

	gc = XCreateGC( win->display, win->my_window, 0, 0 );

	if ( !is_picmod_eq( cache, pic_mod))
	{
		if((cache->cooked != None) && (cache->cooked != cache->root))
			XFreePixmap( win->display, cache->cooked) ;

		/* re-creation */
		if(pic_mod)
		{
			cache->cooked = tile_pixmap( win->display,
						     win->screen,
						     gc,
						     current_root
						     /* we need a copy of pixmap to modify */
						     ,1 ) ;
			memcpy( &(cache->pic_mod), pic_mod, sizeof(x_picture_modifier_t)) ;
			modify_pixmap( win->display, win->screen, cache->cooked, pic_mod) ;
		}
		else
		{
			cache->cooked = tile_pixmap( win->display,
						     win->screen,
						     gc,
						     current_root,
						     0 ) ;
		}
	}
	else
	{
		if( cache->cooked == None )
		{
			cache->cooked = tile_pixmap( win->display,
						     win->screen,
						     gc,
						     current_root,
						     0 ) ;
		}
	}

	XCopyArea( win->display, cache->cooked, pixmap, gc, x, y, width, height, pix_x, pix_y) ;

	XFreeGC( win->display, gc ) ;
	return pixmap ;
}

/** Load an image from the specified file with alpha plane. A pixmap and a mask are returned.
 *\param display connection to the X server.
 *\param path File full path.
 *\param cardinal Returns pointer to a data structure for the extended WM hint spec.
 *\param pixmap Returns an image pixmap for the old WM hint.
 *\param mask Returns a mask bitmap for the old WM hint.
 *
 *\return Success => 1, Failure => 0
 */
int x_imagelib_load_file(
	Display *  display,
	char *  path,
	u_int32_t **  cardinal,
	Pixmap *  pixmap,
	Pixmap *  mask
	)
{
	GdkPixbuf *  pixbuf ;
	int width, height, rowstride ;
#ifndef OLD_GDK_PIXBUF
	pixbuf = gdk_pixbuf_new_from_file( path, NULL ) ;
#else
	pixbuf = gdk_pixbuf_new_from_file( path ) ;
#endif /*OLD_GDK_PIXBUF*/
	if ( !pixbuf )
		return 0 ;

	width = gdk_pixbuf_get_width (pixbuf) ;
	height = gdk_pixbuf_get_height (pixbuf) ;
	if ( cardinal){
		unsigned char *line ;
		unsigned char *pixel ;
		int i, j ;

/* create an CARDINAL array for_NET_WM_ICON data */
		rowstride = gdk_pixbuf_get_rowstride (pixbuf) ;
		line = gdk_pixbuf_get_pixels (pixbuf) ;
		*cardinal = malloc((width * height + 2) *4) ;
		if (!(*cardinal))
			return 0 ;

/* {width, height, ARGB[][]} */
		(*cardinal)[0] = width ;
		(*cardinal)[1] = height ;
		if ( gdk_pixbuf_get_has_alpha (pixbuf)){ /* alpha support (convert to ARGB format)*/
			for (i = 0; i < height; i++) {
				pixel = line ;
				line += rowstride;
				for (j = 0; j < width; j++) {
					(*cardinal)[(i*width+j)+2] = ((((((u_int32_t)(pixel[3]) << 8)
									 + pixel[0]) << 8)
								       + pixel[1]) << 8) + pixel[2] ;
					pixel += 4 ;
				}
			}
		}
		else
		{
			for (i = 0; i < height; i++)
			{
				pixel = line ;
				line += rowstride;
				for (j = 0; j < width; j++) {
					(*cardinal)[(i*width+j)+2] = ((((((u_int32_t)(0x0000FF) <<8 )
									 + pixel[0]) << 8)
								       + pixel[1]) << 8) + pixel[2] ;
					pixel += 3 ;
				}
			}
		}
	}
/* Create the Icon pixmap&mask for WMHints. None as result is acceptable.*/
	if( pixmap)
	{
		*pixmap = XCreatePixmap( display, DefaultRootWindow( display), width, height,
					 DefaultDepth( display, DefaultScreen( display))) ;
		if( mask)
		{
			*mask = XCreatePixmap( display, DefaultRootWindow( display), width, height, 1) ;
			pixbuf_to_pixmap_and_mask( display, DefaultScreen( display), pixbuf, *pixmap, *mask) ;
		}
		else
		{
			pixbuf_to_pixmap( display,
					  DefaultScreen( display),
					  pixbuf,
					  *pixmap);

		}
	}

	gdk_pixbuf_unref(pixbuf) ;
	return 1 ;
}
