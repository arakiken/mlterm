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

#define USE_FS 1
#define SUCCESS 0

/** Pixmap cache per display. */
typedef struct
pixmap_cache_tag 
{
	Display *  display; /**<Display */
	Pixmap  root;      /**<Root pixmap !!! NOT owned by mlterm. DON'T FREE !!!*/
	Pixmap  cooked;    /**<Background pixmap cache*/
	x_picture_modifier_t *  pic_mod; /**<modification applied to "cooked"*/
	struct pixmap_cache_tag *  next ;
} pixmap_cache_t ;

typedef struct
cache_info_tag 
{
	char *  name ;
	GdkPixbuf *  data ;
	GdkPixbuf *  scaled ;
	int  width ;
	int  height ;
} cache_info_t ;

/* --- static variables --- */

static pixmap_cache_t *  pixmap_cache = NULL ;

static cache_info_t wp ;
static cache_info_t misc ;

static int  display_count = 0 ;

static unsigned char  value_table[256] ;
static x_picture_modifier_t  value_table_mod ;

/* --- static functions --- */
static int
fetch_colormap(
	Display *  display,
	int  screen,
	XColor **  color_list
	)
{
	int  num_cells, i ;
	Colormap  cmap = DefaultColormap( display, screen) ;
	
	num_cells = DisplayCells( display , screen) ;
	*color_list = malloc( num_cells * sizeof(XColor)) ;
	if( !*color_list)
		return 0 ;

	for( i = 0 ; i < num_cells ; i ++)
	{
		((*color_list)[i]).pixel = i ;
	}

	XQueryColors( display , cmap , *color_list, num_cells) ;

	return num_cells ;
}

/* create GdkPixbuf from the specified file path. 
 * don't modify returned pixbuf since the pixbuf
 * is cached and may br reused.
 */
static 
GdkPixbuf * 
load_file(
	cache_info_t *  cache,
	char *  path, 	
	int  width,
	int  height,
	GdkInterpType scale_type 
	)       
{
	GdkPixbuf *  pixbuf ;

	pixbuf = NULL ;

	/* is cached one is still valid? */
	if( misc.name && (strcmp( misc.name, path) == 0))
	{
#ifdef DEBUG
		kik_warn_printf(KIK_DEBUG_TAG "used pixbuf from cache\n");
#endif
		pixbuf = misc.data ;
	}
	else
	{
		/* create new pixbuf */
#ifdef DEBUG
		kik_warn_printf(KIK_DEBUG_TAG "adding pixbuf to cache(%s)\n", path);
#endif
		/* get rid of old cache data */
		if( misc.name)
			free( misc.name);
		misc.name = strdup( path) ;

		if( misc.data)
			gdk_pixbuf_unref( misc.data) ;

		if( misc.scaled)
		{
			gdk_pixbuf_unref( misc.scaled) ;
			misc.scaled = 0 ;
		}
		misc.width = 0 ;
		misc.height = 0 ;

#ifndef OLD_GDK_PIXBUF
		pixbuf = gdk_pixbuf_new_from_file( path, NULL) ;
#else
		pixbuf = gdk_pixbuf_new_from_file( path) ;
#endif /*OLD_GDK_PIXBUF*/
		
		misc.data = pixbuf ;

	}
	
	if( !pixbuf)
		return  NULL ;
	

	if( width == 0)
		width = gdk_pixbuf_get_width( pixbuf) ;
	if( height == 0)
		height = gdk_pixbuf_get_height( pixbuf) ;
	/* do we need scaling ? */
	if( ( width != gdk_pixbuf_get_width( pixbuf) ) ||
	    ( height != gdk_pixbuf_get_height( pixbuf)))
	{
		GdkPixbuf * scaled;
		/* Is scaled one in the cache still valid? */
		if( misc.width == width && misc.height == height && misc.scaled)
		{
#ifdef DEBUG
			kik_warn_printf(KIK_DEBUG_TAG "scaled pixbuf from cache\n");
#endif
			scaled = misc.scaled ;
		}
		else
		{
			if( misc.scaled)
				gdk_pixbuf_unref( misc.scaled) ;
#ifdef DEBUG
			kik_warn_printf(KIK_DEBUG_TAG "creating scaled  pixbuf\n");
#endif
			scaled = gdk_pixbuf_scale_simple(pixbuf, width, height,
							 scale_type);
			if( scaled)
			{
				misc.width = width ;
				misc.height = height ;
				misc.scaled = scaled ;
			}
		}
		if( scaled)
			pixbuf = scaled ;
	}

	return  pixbuf ;
}

/* create a pixbuf from an array of cardnals */
static GdkPixbuf *
create_pixbuf_from_cardinals(
	u_int32_t *  cardinal
	)
{
	GdkPixbuf *  pixbuf ;
	int  rowstride ;
	unsigned char *  line ;
	unsigned char *  pixel ;
	int  width,height ;
	int  i, j ;

	width = cardinal[0] ;
	height = cardinal[1] ;
	pixbuf = gdk_pixbuf_new( GDK_COLORSPACE_RGB,
				 TRUE, 8, width, height) ;
	if( !pixbuf)
		return NULL ;
	
	rowstride = gdk_pixbuf_get_rowstride (pixbuf) ;
	line = gdk_pixbuf_get_pixels (pixbuf) ;
       
	for( i = 0 ; i < width ; i++)
	{ 
		pixel = line ;
		for( j = 0 ; j < height ; j++)
		{
			/*ARGB -> RGBA conversion*/
			pixel[2] = (*cardinal) & 0xFF ;
			pixel[1] = ((*cardinal) & 0xFF00) >> 8 ;
			pixel[0] = ((*cardinal) & 0xFF0000) >>16 ;
			pixel[3] = (*cardinal) >> 24 ;
			
			cardinal++ ;
			pixel += 4;
		}
		line += rowstride ;
	}
	return  pixbuf ;
}

/* create an CARDINAL array for_NET_WM_ICON data */
static int
create_cardinals(
	u_int32_t **  cardinal,
	int  width,
	int  height,
	GdkPixbuf *  pixbuf
	)
{
	int  rowstride ;
	unsigned char *line ;
	unsigned char *pixel ;
	int i, j ;

	rowstride = gdk_pixbuf_get_rowstride (pixbuf) ;
	line = gdk_pixbuf_get_pixels (pixbuf) ;
	*cardinal = malloc((width * height + 2) *4) ;
	if (!(*cardinal))
		return -1 ;

	/* format of the array is {width, height, ARGB[][]} */
	(*cardinal)[0] = width ;
	(*cardinal)[1] = height ;
	if ( gdk_pixbuf_get_has_alpha (pixbuf)){
		for (i = 0; i < height; i++) {
			pixel = line ;
			line += rowstride;
			for (j = 0; j < width; j++) {
				/* from RGBA to ARGB conversion */
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
				/* all pixels are completely opaque (0xFF) */
				(*cardinal)[(i*width+j)+2] = ((((((u_int32_t)(0x0000FF) <<8 )
								 + pixel[0]) << 8)
							       + pixel[1]) << 8) + pixel[2] ;
				pixel += 3 ;
			}
		}
	}
	return SUCCESS ;
}
/* seek the closet color */
static int
closest_color_index(
	Display *  display ,
	int  screen ,
	XColor *  color_list,
	int  len,
	int red,
	int green,
	int blue
	)
{
	int  i ;
	int  closest = 0 ;
	unsigned long  min = 0xffffff ;
	unsigned long  diff ;
	int  diff_r, diff_g, diff_b ;

	for( i = 0 ; i < len ; i ++)
	{
		/* lazy color-space conversion*/
		diff_r = red - (color_list[i].red >> 8) ;
		diff_g = green - (color_list[i].green >> 8) ;
		diff_b = blue - (color_list[i].blue >> 8) ;
		diff = diff_r * diff_r *9 + diff_g * diff_g * 30 + diff_b * diff_b ;
		if ( diff < min)
		{
			min = diff ;
			closest = i ;
			/* no one may notice the difference */
			if ( diff < 31) 
				goto enough ;
		}
	}
enough:
	return closest ;
}


static int
pixbuf_to_pixmap_pseudocolor(
	Display *  display ,
	int screen,
	GdkPixbuf *  pixbuf,
	Pixmap pixmap
	)
{
	unsigned int  width, height, rowstride, bytes_per_pixel ;

	int  x, y ;
	int  num_cells;
#ifdef USE_FS
	char *  diff_next ;
	char *  diff_cur ;
	char *  temp;
#endif /* USE_SF */

	unsigned char *  line;
	unsigned char *  pixel;
	XColor *  color_list ;
	int  closest ;
	GC  gc;
	XGCValues  gcv ;
	int  diff_r, diff_g, diff_b ;

	num_cells = fetch_colormap( display, screen, &color_list) ;
	
	if( !num_cells)
		return -8 ;

	width = gdk_pixbuf_get_width (pixbuf) ;
	height = gdk_pixbuf_get_height (pixbuf) ;

#ifdef USE_FS
	diff_cur = calloc( width*3, 1) ;
	if( !diff_cur)
	{
		return -1 ;
	}
	diff_next = calloc( width*3, 1) ;
	if( !diff_next)
	{
		free( diff_cur) ;
		return -2 ;
	}
#endif /* USE_SF */
	bytes_per_pixel = (gdk_pixbuf_get_has_alpha( pixbuf)) ? 4:3 ;
	rowstride = gdk_pixbuf_get_rowstride (pixbuf) ;

	line = gdk_pixbuf_get_pixels (pixbuf) ;
	gc = XCreateGC (display, pixmap, 0, &gcv) ;

	for ( y = 0 ; y < height ; y++ )
	{
		pixel = line ;
#ifdef USE_FS
		closest = closest_color_index( display, screen, color_list, num_cells,
					       pixel[0] - diff_cur[0],
					       pixel[1] - diff_cur[1],
					       pixel[2] - diff_cur[2]) ;
#else
		closest = closest_color_index( display, screen, color_list, num_cells,
					       pixel[0],
					       pixel[1],
					       pixel[2])
#endif /* USE_SF */

#ifdef USE_FS
		diff_r = (color_list[closest].red   >>8 ) - pixel[0];
		diff_g = (color_list[closest].green >>8 ) - pixel[1];
		diff_b = (color_list[closest].blue  >>8 ) - pixel[2];

		diff_cur[3*1 + 0 ] += diff_r /2;
		diff_cur[3*1 + 1 ] += diff_g /2;
		diff_cur[3*1 + 2 ] += diff_b /2;

		diff_next[3*0 +0] = diff_r /4;
		diff_next[3*0 +1] = diff_g /4;
		diff_next[3*0 +2] = diff_b /4;

		diff_next[3*1 +0] = diff_r /4;
		diff_next[3*1 +1] = diff_g /4;
		diff_next[3*1 +2] = diff_b /4;
#endif /* USE_SF */
		XSetForeground( display, gc, closest) ;
		XDrawPoint( display, pixmap, gc, 0, y) ;
		pixel += bytes_per_pixel ;

		for ( x = 1 ; x < width -2 ; x++ )
		{
#ifdef USE_FS
			closest = closest_color_index( display, screen, color_list, num_cells,
						       pixel[0] - diff_cur[3*x +0],
						       pixel[1] - diff_cur[3*x +1],
						       pixel[2] - diff_cur[3*x +2]) ;
#else
			closest = closest_color_index( display, screen, color_list, num_cells,
						       pixel[0] ,
						       pixel[1] ,
						       pixel[2] ) ;
#endif /* USE_SF */

#ifdef USE_FS
			diff_r = (color_list[closest].red   >>8 ) - pixel[0];
			diff_g = (color_list[closest].green >>8 ) - pixel[1];
			diff_b = (color_list[closest].blue  >>8 ) - pixel[2];

			diff_cur[3*(x+1) + 0 ] += diff_r /2;
			diff_cur[3*(x+1) + 1 ] += diff_g /2;
			diff_cur[3*(x+1) + 2 ] += diff_b /2;

			diff_next[3*(x-1) +0] += diff_r /8;
			diff_next[3*(x-1) +1] += diff_g /8;
			diff_next[3*(x-1) +2] += diff_b /8;

			diff_next[3*(x+0) +0] += diff_r /8;
			diff_next[3*(x+0) +1] += diff_g /8;
			diff_next[3*(x+0) +2] += diff_b /8;

			diff_next[3*(x+1) +0] = diff_r /4;
			diff_next[3*(x+1) +1] = diff_g /4;
			diff_next[3*(x+1) +2] = diff_b /4;
#endif /* USE_SF */
			XSetForeground( display, gc, closest) ;
			XDrawPoint( display, pixmap, gc, x, y) ;

			pixel += bytes_per_pixel ;
		}
#ifdef USE_FS
		closest = closest_color_index( display, screen, color_list, num_cells,
					       pixel[0] - diff_cur[3*x +0],
					       pixel[1] - diff_cur[3*x +1],
					       pixel[2] - diff_cur[3*x +2]) ;
#else
		closest = closest_color_index( display, screen, color_list, num_cells,
					       pixel[0],
					       pixel[1],
					       pixel[2]) ;
#endif /* USE_SF */
		XSetForeground( display, gc, closest) ;
		XDrawPoint( display, pixmap, gc, x, y) ;
#ifdef USE_FS
		diff_r = (color_list[closest].red   >>8 ) - pixel[0];
		diff_g = (color_list[closest].green >>8 ) - pixel[1];
		diff_b = (color_list[closest].blue  >>8 ) - pixel[2];

		diff_next[3*(x-1) +0] += diff_r /4;
		diff_next[3*(x-1) +1] += diff_g /4;
		diff_next[3*(x-1) +2] += diff_b /4;

		diff_next[3*(x+0) +0] += diff_r /4;
		diff_next[3*(x+0) +1] += diff_g /4;
		diff_next[3*(x+0) +2] += diff_b /4;
#endif /* USE_SF */
		line += rowstride ;
#ifdef USE_FS
		temp = diff_cur;
		diff_cur = diff_next;
		diff_next = temp;
#endif /* USE_SF */
	}
	free( color_list) ;
	XFreeGC( display, gc) ;
#ifdef USE_FS
	free( diff_cur) ;
	free( diff_next) ;
#endif /* USE_SF */
	return SUCCESS ;
}


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

static pixmap_cache_t *
cache_seek(
	   Display *  display
	   )
{
	pixmap_cache_t *  cache = pixmap_cache ;

	while( cache)
	{
		if( cache->display == display)
			break ;
		cache = cache->next ;
	}
	return cache ;
}

static pixmap_cache_t *
cache_add(
	   Display *  display
	   )
{
	pixmap_cache_t *  cache ;

	cache = malloc( sizeof( pixmap_cache_t)) ;
	if (!cache)
		return NULL ;
	cache->display = display ;
	cache->root = None ;
	cache->cooked = None ;
	cache->pic_mod = NULL ;
	cache->next = pixmap_cache ;
	pixmap_cache = cache ;

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
	pixmap_cache_t *  cache ;
	pixmap_cache_t *  o ;

	o = NULL ;
	cache = pixmap_cache ;
	while( cache)
	{
		if ( cache->display == display)
		{
			if ((cache->cooked) && (cache->root != cache->cooked))
				XFreePixmap( display, cache->cooked) ;
			if ( cache->pic_mod)
				free( cache->pic_mod) ;
			if ( o == NULL)
			{
				pixmap_cache = cache->next ;
				free( cache) ;
				cache = pixmap_cache ;
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
	x_picture_modifier_t *  a ,
	x_picture_modifier_t *  b
	)
{
	if( a == b )
		return 1 ;
	
	if ( a == NULL )
	{
		a = b ;
		b = NULL ;
	}

	if (b == NULL)
	{
		if( (a->brightness == 100) &&
		    (a->contrast == 100) &&
		    (a->gamma == 100))
		{
			return 1 ;
		}
	}
	else
	{
		if( (a->brightness == b->brightness) &&
		    (a->contrast == b->contrast) &&
		    (a->gamma == b->gamma))
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
pixbuf_to_ximage_truecolor(
	Display *  display,
	int  screen,
	GdkPixbuf *  pixbuf,
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

	width = gdk_pixbuf_get_width( pixbuf) ;
	height = gdk_pixbuf_get_height( pixbuf) ;

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
				      width, height,
				      16,
				      width *  2);
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
				      width, height,
				      32,
				      width * 4) ;
		for( i = 0; i < height; i++)
		{
			pixel = line ;
			for( j = 0; j < width; j++)
			{
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

static int
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
		return -1 ;

	vinfo.visualid = XVisualIDFromVisual( DefaultVisual( display, screen)) ;
	if (!vinfo.visualid)
		return -2 ;
	vinfolist = XGetVisualInfo( display, VisualIDMask, &vinfo, &matched) ;
	if( !vinfolist)
		return -3 ;
	if ( !matched)
	{
		XFree( vinfolist) ;
		return -4 ;
	}
	switch( vinfolist[0].class)
	{
	case TrueColor:
	{
		image = pixbuf_to_ximage_truecolor( display,
						    screen,
						    pixbuf,
						    DefaultDepth( display, screen),
						    vinfolist) ;
		XFree( vinfolist) ;
		if( image)
		{
			XPutImage( display, pixmap, DefaultGC( display, screen), image, 0, 0, 0, 0,
				   gdk_pixbuf_get_width( pixbuf),gdk_pixbuf_get_height( pixbuf)) ;
			XDestroyImage( image) ;
			return SUCCESS ;
		}
		break ;
	}
	case PseudoColor:
	{
		XFree( vinfolist) ;
		if( pixbuf_to_pixmap_pseudocolor( display, screen, pixbuf, pixmap) != SUCCESS)
			return -1;
		return SUCCESS ;
		break ;
	}
	default:
	{
		XFree( vinfolist) ;
		break ;
	}
	}

	return -1 ;
}

static XImage *
compose_truecolor(
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

static XImage *
compose_pseudocolor(
	Display *  display,
	int  screen,
	GdkPixbuf *  pixbuf,
	Pixmap  pixmap,
	int  depth,
	XVisualInfo * vinfo
	)
{
	XImage *  image ;

	unsigned int i, j, num_cells, r, g, b ;
	unsigned int width, height, rowstride ;
	unsigned char *line ;
	unsigned char *pixel ;
	XColor *  color_list ;

	num_cells = fetch_colormap( display, screen, &color_list) ;

	if( !num_cells)
		return NULL ;

	width = gdk_pixbuf_get_width (pixbuf) ;
	height = gdk_pixbuf_get_height (pixbuf) ;

	image = XGetImage( display, pixmap, 0, 0, width, height, AllPlanes, ZPixmap) ;

	rowstride = gdk_pixbuf_get_rowstride( pixbuf) ;
	line = gdk_pixbuf_get_pixels( pixbuf) ;

	switch( depth)
	{
	case 8:
	{
		u_int8_t *  data = (u_int8_t *)(image->data) ;

		for( i = 0; i < height; i++)
		{
			pixel = line ;
			for( j = 0; j < width; j++)
			{
				if(pixel[3] != 0)
				{
					if (pixel[3] != 0xFF)
					{

						r = color_list[*data].red >>8 ;
						g = color_list[*data].green >>8 ;
						b = color_list[*data].blue >>8 ;

						r = (r*(256 - pixel[3]) + pixel[0] *  pixel[3])>>8 ;
						g = (g*(256 - pixel[3]) + pixel[1] *  pixel[3])>>8 ;
						b = (b*(256 - pixel[3]) + pixel[2] *  pixel[3])>>8 ;

						*data = closest_color_index( display, screen,
									     color_list, num_cells,
									     r, g, b ) ;


					}
					else
					{
						*data = closest_color_index( display, screen,
									     color_list, num_cells,
									     pixel[0], pixel[1], pixel[2] ) ;
					}
				}
				data++ ;
				pixel += 4 ;
			}
			line += rowstride ;
		}
		break ;
	}
	default:
		break;
	}
	free( color_list) ;
	return image ;
}

static int
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
		return -1 ;
	vinfo.visualid = XVisualIDFromVisual( DefaultVisual( display, screen)) ;
	if (!vinfo.visualid)
		return -2 ;
	vinfolist = XGetVisualInfo( display, VisualIDMask, &vinfo, &matched) ;
	if( !vinfolist)
		return -3 ;
	if ( !matched)
	{
		XFree( vinfolist) ;
		return -4 ;
	}

	switch( vinfolist[0].class)
	{
	case TrueColor:
		image = compose_truecolor( display,
					   screen,
					   pixbuf,
					   pixmap,
					   DefaultDepth( display, screen),
					   vinfolist) ;
		break;
	case PseudoColor:
		image = compose_pseudocolor( display,
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
		return SUCCESS ;
	}

	XFree( vinfolist) ;

	return -5 ;
}

static int
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
	if( pixbuf_to_pixmap( display, screen, pixbuf, pixmap) == -1)
		return -1 ;

	gc = XCreateGC (display, mask, 0, &gcv) ;
	bytes_per_pixel = (gdk_pixbuf_get_has_alpha( pixbuf)) ? 4:3 ;
	if (bytes_per_pixel == 3)
	{ /* no mask */
		XSetForeground( display, gc, 1) ;
		XFillRectangle( display, mask, gc,
				0, 0, width, height) ;

	}
	else
	{
		XSetForeground( display, gc, 0) ;
		XFillRectangle( display, mask, gc,
				0, 0, width, height) ;
		XSetForeground( display, gc, 1) ;
		line = gdk_pixbuf_get_pixels (pixbuf) ;
		rowstride = gdk_pixbuf_get_rowstride (pixbuf) ;
		for (i = 0; i < height; i++)
		{
			pixel = line ;
       			line += rowstride ;
			for (j = 0; j < width; j++)
			{
				if( pixel[3] > 127)
					XDrawPoint( display, mask, gc, j, i) ;
				pixel += 4 ;
			}
		}
	}
	XFreeGC( display, gc) ;
	return SUCCESS ;
}

static void
value_table_refresh(
	x_picture_modifier_t *  mod
	)
{
	int i ;
	double real_gamma , real_brightness, real_contrast;

	if( is_picmod_eq(mod, &value_table_mod))
		return ;

	value_table_mod.brightness = mod->brightness ;
	value_table_mod.contrast = mod->contrast ;
	value_table_mod.gamma = mod->gamma ;

	memset(value_table, 0, sizeof(value_table)) ;

	real_gamma = (double)(value_table_mod.gamma) / 100 ;
	real_contrast = (double)(value_table_mod.contrast) / 100 ;
	real_brightness = (double)(value_table_mod.brightness) / 100 ;
	i = 128 ;
	while( i > 0)
	{
		value_table[i] = real_contrast * (255 * pow((double)i / 255, real_gamma) -128 ) + 128 *  real_brightness ;
		if (value_table[i--] == 0)
			break ;
	}
	while( i >= 0)
		value_table[i--] = 0;
	i = 128 ;
	while( i < 254)
	{
		value_table[i] = real_contrast * (255 * pow((double)i / 255, real_gamma) -128 ) + 128 *  real_brightness ;
		if (value_table[i++] == 255)
			break ;
	}
	while( i <= 255)
		value_table[i++] = 255 ;
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
			return -1 ;
	bytes_per_pixel = (gdk_pixbuf_get_has_alpha( pixbuf)) ? 4:3 ;
	width = gdk_pixbuf_get_width (pixbuf) ;
	height = gdk_pixbuf_get_height (pixbuf) ;
	rowstride = gdk_pixbuf_get_rowstride (pixbuf) ;

	if( !pic_mod)
		return -2 ;
	if(pic_mod->brightness == 100 && pic_mod->contrast == 100 && pic_mod->gamma == 100)
		return SUCCESS ;

       	line = gdk_pixbuf_get_pixels (pixbuf) ;

	value_table_refresh( pic_mod) ;

	for (i = 0; i < height; i++)
	{
		pixel = line ;
		line += rowstride ;

		for (j = 0; j < width; j++)
		{
/* XXX keeps neither hue nor saturation. MUST be replaced by another better color model(CIE Yxy? lab?)*/
			pixel[0] = value_table[pixel[0]] ;
			pixel[1] = value_table[pixel[1]] ;
			pixel[2] = value_table[pixel[2]] ;
			/* alpha plane is not changed */
			pixel += bytes_per_pixel ;
		}
	}

	return SUCCESS ;
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
		return -1 ;

	if(pic_mod->brightness == 100 && pic_mod->contrast == 100 && pic_mod->gamma == 100)
		return SUCCESS ;

	vinfo.visualid = XVisualIDFromVisual( DefaultVisual( display, screen)) ;
	vinfolist = XGetVisualInfo( display, VisualIDMask, &vinfo, &matched) ;
	if( !vinfolist)
		return -2;
	if ( !matched)
	{
		XFree( vinfolist) ;
		return -3;
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

		switch( depth)
		{
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

			value_table_refresh( pic_mod) ;
			for (i = 0; i < height; i++)
			{
				for (j = 0; j < width; j++)
				{
					r = ((*data & r_mask) >> r_offset)<<r_limit ;
					g = ((*data & g_mask) >> g_offset)<<g_limit ;
					b = ((*data & b_mask) >> b_offset)<<b_limit ;
					
					r = value_table[r] ;
					g = value_table[g] ;
					b = value_table[b] ;

					
					*data = (r >> r_limit) << r_offset |
						(g >> g_limit) << g_offset |
						(b >> b_limit) << b_offset ;
					data++;
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
			value_table_refresh( pic_mod) ;
			for (i = 0; i < height; i++)
			{
				for (j = 0; j < width; j++)
				{
					r = ((*data) >>r_offset) & 0xFF ;
					g = ((*data) >>g_offset) & 0xFF ;
					b = ((*data) >>b_offset) & 0xFF ;


					r = value_table[r] ;
					g = value_table[g] ;
					b = value_table[b] ;
					
					*data = r << r_offset | g << g_offset | b << b_offset ;
						data++ ;
				}
			}

			XPutImage( display, pixmap, DefaultGC( display, screen), image, 0, 0, 0, 0,
				   width, height) ;
			break ;
		}
		default:
			break;
		}
	case PseudoColor:
		switch( vinfolist[0].depth)
		{
		case 8:
		{
			XColor *  color_list ;
			int num_cells ;
			u_int8_t *  data ;

			num_cells = fetch_colormap( display, screen, &color_list) ;
			if( !num_cells)
				return -7 ;

			data = (u_int8_t *)(image->data) ;

			value_table_refresh( pic_mod) ;
			for (i = 0; i < height; i++)
			{
				for (j = 0; j < width; j++)
				{
					r = color_list[*data].red >>8 ;
					g = color_list[*data].green >>8 ;
					b = color_list[*data].blue >>8 ;
					

					r = value_table[r] ;
					g = value_table[g] ;
					b = value_table[b] ;

					*data = closest_color_index( display, screen,
								     color_list, num_cells,
								     r, g, b ) ;
					data++ ;
				}

				*data = closest_color_index( display, screen,
							     color_list, num_cells,
							     r, g, b ) ;
				data++ ;
			}

			XPutImage( display, pixmap, DefaultGC( display, screen), image, 0, 0, 0, 0,
				   width, height) ;
			free( color_list) ;
			break ;
		}
		}
	}
	XFree(vinfolist) ;
	XDestroyImage( image) ;

	return SUCCESS ;
}

/* --- global functions --- */

int
x_imagelib_display_opened(
	Display *  display
	)
{
	int i ;
	if (display_count == 0)
	{
#ifndef OLD_GDK_PIXBUF
		g_type_init() ;
#endif /*OLD_GDK_PIXBUF*/
		for (i = 0; i < 256; i++)		
			value_table[i] = i ;		
		value_table_mod.brightness = 100 ;
		value_table_mod.contrast = 100 ;
		value_table_mod.gamma = 100 ;
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
	static x_picture_modifier_t *  cached_mod = NULL ;
	static Pixmap  cached_pixmap = None ;  
	static GdkPixbuf *  cached_pixbuf = NULL ;  

	GdkPixbuf *  pixbuf ;
	Pixmap pixmap ;
	int  dup_flag = 0;	

	if(!file_path)
		return None ;
	/* XXX have to compare pic_mod */
	if( !( pixbuf = load_file( &wp, file_path,
				ACTUAL_WIDTH(win), ACTUAL_HEIGHT(win),
				   GDK_INTERP_TILES)))
	{
		return None ;
	}

	if( !is_picmod_eq( pic_mod, cached_mod))
	{
		if( cached_mod)
		{
			free(cached_mod) ;
			gdk_pixbuf_unref( cached_pixbuf) ;
		}
		cached_pixbuf = gdk_pixbuf_copy(pixbuf);
		modify_image( cached_pixbuf, pic_mod) ;
		cached_mod = malloc(sizeof(x_picture_modifier_t)) ;
		memcpy( cached_mod, pic_mod, sizeof(x_picture_modifier_t)) ;		
		pixbuf = cached_pixbuf ;
		dup_flag = 1 ;
	}
	if( gdk_pixbuf_get_has_alpha ( pixbuf) )
	{
		pixmap = x_imagelib_get_transparent_background( win, NULL) ;
		if( compose_to_pixmap( win->display, win->screen,
				       pixbuf, pixmap) != SUCCESS)
		{
			XFreePixmap( win->display, pixmap) ;
			return None ;
		}
	}
	else
	{
		pixmap = XCreatePixmap( win->display, win->my_window,
					ACTUAL_WIDTH(win), ACTUAL_HEIGHT(win),
					DefaultDepth( win->display, win->screen));
		if( pixbuf_to_pixmap( win->display, win->screen,
				      pixbuf, pixmap) != SUCCESS)
		{
			XFreePixmap( win->display, pixmap) ;
			return None ;
		}
	}
	if( dup_flag)
		gdk_pixbuf_unref( pixbuf) ;

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
	pixmap_cache_t *  cache ;
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
			if((cache->cooked != None) && (cache->cooked != cache->root))
			{
				XFreePixmap( win->display, cache->cooked) ;
			}
			cache->root = current_root ;

			cache->cooked = None ;
			if ( cache->pic_mod)
				free(cache->pic_mod) ;
			cache->pic_mod = NULL ;
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

	if ( !is_picmod_eq( cache->pic_mod, pic_mod))
	{
		if((cache->cooked != None) && (cache->cooked != cache->root))
			XFreePixmap( win->display, cache->cooked) ;

		/* re-creation */
		if(pic_mod)
		{
			/* we need a copy of pixmap to modify */
			cache->cooked = tile_pixmap( win->display,
						     win->screen,
						     gc,
						     current_root						     
						     ,1 ) ;
			if(cache->pic_mod)
				free(cache->pic_mod) ;
			cache->pic_mod = malloc(sizeof(x_picture_modifier_t)) ;
			memcpy( cache->pic_mod, pic_mod, sizeof(x_picture_modifier_t)) ;
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
	Pixmap *  mask,
	int  width,
	int  height
	)
{
	GdkPixbuf *  pixbuf ;

	if( !path && !cardinal)
		return 0 ;	
	if( path)
	{
		/* create a pixbuf from the file and create a cardianl array */
		if( !( pixbuf = load_file( &misc, path, width, height, GDK_INTERP_NEAREST)))
		{
#ifdef DEBUG
		kik_warn_printf(KIK_DEBUG_TAG "couldn't load pixbuf\n");
#endif			
			return 0 ;
		}

		if ( cardinal)
			create_cardinals( cardinal, width, height, pixbuf) ;
	}
	else
	{
		if( !(*cardinal))
			return 0 ;
		/* create a pixbuf from the cardianl array */		
		pixbuf = create_pixbuf_from_cardinals( *cardinal) ;
		if( !pixbuf)
			return 0 ;

	}
/* Create the Icon pixmap & mask to be used in WMHints.
 * Note that none as a result is acceptable.
 * Pixmaps can't be cached since the last pixmap may be freed by someone... */
	if( pixmap)
	{
		*pixmap = XCreatePixmap( display, DefaultRootWindow( display),
					 width, height,
					 DefaultDepth( display, DefaultScreen( display))) ;
		if( mask)
		{
			*mask = XCreatePixmap( display,
					       DefaultRootWindow( display),
					       width, height, 1) ;
			if( pixbuf_to_pixmap_and_mask( display,
						       DefaultScreen( display),
						       pixbuf, *pixmap, *mask) != SUCCESS)
			{
				XFreePixmap( display, *pixmap) ;
				*pixmap = 0 ;
				XFreePixmap( display, *mask) ;
				*mask = 0 ;
				return 0 ;
			}

		}
		else
		{
			if( pixbuf_to_pixmap( display, DefaultScreen( display),
					      pixbuf, *pixmap) != SUCCESS)
			{
				XFreePixmap( display, *pixmap) ;
				*pixmap = 0 ;
				return 0 ;
			}
		}
	}

	return 1 ;
}
