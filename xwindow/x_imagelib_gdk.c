/** @file
 *  @brief image handling functions using gdk-pixbuf
 *	$Id$
 */

#include <math.h>                        /* pow */
#include <X11/Xatom.h>                   /* XInternAtom */
#include <X11/Xutil.h>
#include <string.h>                      /* memcpy */
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <kiklib/kik_debug.h>
#include <kiklib/kik_types.h> /* u_int32_t/u_int16_t, HAVE_STDINT_H */
#include <kiklib/kik_unistd.h>
#include <kiklib/kik_str.h>    /* strdup */

#include "x_imagelib.h"

/*
 * 'data' which is malloc'ed for XCreateImage() in pixbuf_to_ximage_truecolor()
 * is free'ed in XDestroyImage().
 * If malloc is replaced kik_mem_malloc in kik_mem.h, kik_mem_free_all() will
 * free 'data' which is already free'ed in XDestroyImage() and
 * segmentation fault error can happen.
 */
#if  defined(KIK_DEBUG) && defined(malloc)
#undef malloc
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifndef SIZE_MAX
#ifdef SIZE_T_MAX
#define SIZE_MAX SIZE_T_MAX
#else
#define SIZE_MAX ((size_t)-1)
#endif
#endif

#define USE_FS 1
#define SUCCESS 0

#if  (GDK_PIXBUF_MAJOR < 2)
#define  g_object_ref( pixbuf) gdk_pixbuf_ref( pixbuf)
#define  g_object_unref( pixbuf) gdk_pixbuf_unref( pixbuf)
#endif


/** Pixmap cache per display. */
typedef struct  pixmap_cache_tag
{
	Display *  display; /**<Display */
	Pixmap  root;      /**<Root pixmap   !!! NOT owned by mlterm. DON'T FREE !!!*/
	Pixmap  cooked;    /**<Background pixmap cache */
	x_picture_modifier_t *  pic_mod; /**<modification applied to "cooked" */
	struct pixmap_cache_tag *  next ;
} pixmap_cache_t ;


/* --- static variables --- */

static pixmap_cache_t *  pixmap_cache = NULL ;
static int  display_count = 0 ;
static unsigned char  value_table[256] ;

/* --- static functions --- */

/* returned cmap shuold be freed by the caller */
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
	*color_list = calloc( sizeof(XColor), num_cells) ;
	if( !*color_list)
	{
#ifdef DEBUG
		kik_warn_printf(KIK_DEBUG_TAG "couldn't allocate color table\n") ;
#endif
		return  0 ;
	}
	for( i = 0 ; i < num_cells ; i ++)
		((*color_list)[i]).pixel = i ;

	XQueryColors( display , cmap , *color_list, num_cells) ;

	return  num_cells ;
}

/* create GdkPixbuf from the specified file path.
 *
 * The returned pixbuf shouled be unrefed by the caller
 * don't modify returned pixbuf since the pixbuf
 * is stored in the cache and may be reused.
 */
static
GdkPixbuf *
load_file(
	char *  path,
	unsigned int  width,
	unsigned int  height,
	GdkInterpType scale_type
	)
{
	static char *  name = NULL ;
	static GdkPixbuf *  data = NULL ;
	static GdkPixbuf *  scaled = NULL ;

	GdkPixbuf *  pixbuf ;

	pixbuf = NULL ;

	if( !path)
	{
		/* free caches */
		if( data)
		{
			g_object_unref( data) ;
			data = NULL ;
		}
		if( scaled)
		{
			g_object_unref( scaled) ;
			scaled = NULL ;
		}
		return  NULL ;
	}

	/* cached one is still valid? */
	if( name && (strcmp( name, path) == 0))
	{
#ifdef DEBUG
		kik_warn_printf(KIK_DEBUG_TAG "returning cached pixbuf\n") ;
#endif
		pixbuf = data ;
	}
	else
	{
		/* create new pixbuf */
#ifdef DEBUG
		kik_warn_printf(KIK_DEBUG_TAG "adding pixbuf to cache(%s)\n", path) ;
#endif
		/* get rid of old cache data */
		free( name) ;
		name = strdup( path) ;
		if( data)
		{
			g_object_unref( data) ;
		}

		if( scaled) /* scaled one is not vaild now */
		{
			g_object_unref( scaled) ;
		}
		scaled = NULL ;

#if GDK_PIXBUF_MAJOR >= 2
		data = gdk_pixbuf_new_from_file( path, NULL) ;
#else
		data = gdk_pixbuf_new_from_file( path) ;
#endif /*GDK_PIXBUF_MAJOR*/

		pixbuf = data ;

	}
	/* loading from file/cache ends here */

	if( !data)
		return  NULL ;

	if( width == 0)
		width = gdk_pixbuf_get_width( data) ;
	if( height == 0)
		height = gdk_pixbuf_get_height( data) ;

	/* Old cached one became obsolete if width/height is changed */
	if( ( width != gdk_pixbuf_get_width( data)) ||
	    ( height != gdk_pixbuf_get_height( data)))
	{
		if( scaled && gdk_pixbuf_get_width( scaled) == width && gdk_pixbuf_get_height( scaled) == height)
		{
#ifdef __DEBUG
			kik_warn_printf(KIK_DEBUG_TAG "using the scaled pixbuf( %d x %d) from cache\n", width, height) ;
#endif
			pixbuf = scaled ;
		}
		else
		{
			if( scaled)
			{
				g_object_unref( scaled) ;
			}
#ifdef __DEBUG
			kik_warn_printf(KIK_DEBUG_TAG "creating a scaled pixbuf(%d x %d) from %d %d \n", width, height) ;
#endif
			scaled = gdk_pixbuf_scale_simple( data, width, height,
							  scale_type) ;
			pixbuf = scaled ;
		}
	}
	/* scaling ends here */

	g_object_ref( pixbuf) ;

	return  pixbuf ;
}

/* create a pixbuf from an array of cardinals */
static GdkPixbuf *
create_pixbuf_from_cardinals(
	u_int32_t *  cardinal,
	int  req_width,
	int  req_height
	)
{
	GdkPixbuf *  pixbuf ;
	GdkPixbuf *  scaled ;
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
		return  NULL ;

	rowstride = gdk_pixbuf_get_rowstride( pixbuf) ;
	line = gdk_pixbuf_get_pixels( pixbuf) ;
	cardinal += 2 ;

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


	if( req_width == 0)
		req_width = width ;
	if( req_height == 0)
		req_height = height ;

	if( (req_width != width) || (req_height != height))
		scaled = gdk_pixbuf_scale_simple(pixbuf, req_width, req_height,
						 GDK_INTERP_TILES) ;
	else
		scaled = NULL ;

	if( scaled)
	{
		g_object_unref( pixbuf) ;
		return  scaled ;
	}
	else
	{
		return  pixbuf ;
	}
}

/* create an CARDINAL array for_NET_WM_ICON data */
static int
create_cardinals_from_bixbuf(
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

	if( !width || !height)
		return -1;

	if( width > ((SIZE_MAX / 4) -2) / height)
		return -1; /* integer overflow */

	*cardinal = malloc( ((size_t)width * height + 2) *4) ;
	if( !(*cardinal))
		return  -1 ;

	/* create (maybe shriked) copy */
	pixbuf = gdk_pixbuf_scale_simple(pixbuf, width, height, GDK_INTERP_TILES) ;

	rowstride = gdk_pixbuf_get_rowstride( pixbuf) ;
	line = gdk_pixbuf_get_pixels( pixbuf) ;

	/* format of the array is {width, height, ARGB[][]} */
	(*cardinal)[0] = width ;
	(*cardinal)[1] = height ;
	if( gdk_pixbuf_get_has_alpha( pixbuf)){
		for( i = 0; i < height; i++) {
			pixel = line ;
			line += rowstride;
			for( j = 0; j < width; j++) {
				/* RGBA to ARGB */
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
				(*cardinal)[(i*width+j)+2] = ((((((u_int32_t)(0x0000FF) <<8)
								 + pixel[0]) << 8)
							       + pixel[1]) << 8) + pixel[2] ;
				pixel += 3 ;
			}
		}
	}

	g_object_unref( pixbuf) ;

	return  SUCCESS ;
}
/* seek the closest color */
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
	int  closest = 0 ;
	int  i ;
	unsigned long  min = 0xffffff ;
	unsigned long  diff ;
	int  diff_r, diff_g, diff_b ;

	for( i = 0 ; i < len ; i++)
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
	return  closest ;
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
		return  -8 ;

	width = gdk_pixbuf_get_width (pixbuf) ;
	height = gdk_pixbuf_get_height (pixbuf) ;

#ifdef USE_FS
	diff_cur = calloc( width*3, 1) ;
	if( !diff_cur)
	{
		return  -1 ;
	}
	diff_next = calloc( width*3, 1) ;
	if( !diff_next)
	{
		free( diff_cur) ;
		return  -2 ;
	}
#endif /* USE_SF */

	bytes_per_pixel = (gdk_pixbuf_get_has_alpha( pixbuf)) ? 4:3 ;
	rowstride = gdk_pixbuf_get_rowstride (pixbuf) ;

	line = gdk_pixbuf_get_pixels( pixbuf) ;
	gc = XCreateGC (display, pixmap, 0, &gcv) ;

	for ( y = 0 ; y < height ; y++)
	{
		pixel = line ;
#ifdef USE_FS
		closest = closest_color_index( display, screen, color_list, num_cells,
					       pixel[0] - diff_cur[0],
					       pixel[1] - diff_cur[1],
					       pixel[2] - diff_cur[2]) ;
		diff_r = (color_list[closest].red   >>8) - pixel[0];
		diff_g = (color_list[closest].green >>8) - pixel[1];
		diff_b = (color_list[closest].blue  >>8) - pixel[2];

		diff_cur[3*1 + 0 ] += diff_r /2;
		diff_cur[3*1 + 1 ] += diff_g /2;
		diff_cur[3*1 + 2 ] += diff_b /2;

		/* initialize next line */
		diff_next[3*0 +0] = diff_r /4;
		diff_next[3*0 +1] = diff_g /4;
		diff_next[3*0 +2] = diff_b /4;

		diff_next[3*1 +0] = diff_r /4;
		diff_next[3*1 +1] = diff_g /4;
		diff_next[3*1 +2] = diff_b /4;
#else
		closest = closest_color_index( display, screen, color_list, num_cells,
					       pixel[0],
					       pixel[1],
					       pixel[2])
#endif /* USE_SF */

		XSetForeground( display, gc, closest) ;
		XDrawPoint( display, pixmap, gc, 0, y) ;
		pixel += bytes_per_pixel ;

		for ( x = 1 ; x < width -2 ; x++)
		{
#ifdef USE_FS
			closest = closest_color_index( display, screen, color_list, num_cells,
						       pixel[0] - diff_cur[3*x +0],
						       pixel[1] - diff_cur[3*x +1],
						       pixel[2] - diff_cur[3*x +2]) ;
			diff_r = (color_list[closest].red   >>8) - pixel[0];
			diff_g = (color_list[closest].green >>8) - pixel[1];
			diff_b = (color_list[closest].blue  >>8) - pixel[2];

			diff_cur[3*(x+1) + 0 ] += diff_r /2;
			diff_cur[3*(x+1) + 1 ] += diff_g /2;
			diff_cur[3*(x+1) + 2 ] += diff_b /2;

			diff_next[3*(x-1) +0] += diff_r /8;
			diff_next[3*(x-1) +1] += diff_g /8;
			diff_next[3*(x-1) +2] += diff_b /8;

			diff_next[3*(x+0) +0] += diff_r /8;
			diff_next[3*(x+0) +1] += diff_g /8;
			diff_next[3*(x+0) +2] += diff_b /8;
			/* initialize next line */
			diff_next[3*(x+1) +0] = diff_r /4;
			diff_next[3*(x+1) +1] = diff_g /4;
			diff_next[3*(x+1) +2] = diff_b /4;
#else
			closest = closest_color_index( display, screen, color_list, num_cells,
						       pixel[0] ,
						       pixel[1] ,
						       pixel[2]) ;
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
		diff_r = (color_list[closest].red   >>8) - pixel[0];
		diff_g = (color_list[closest].green >>8) - pixel[1];
		diff_b = (color_list[closest].blue  >>8) - pixel[2];

		diff_next[3*(x-1) +0] += diff_r /4;
		diff_next[3*(x-1) +1] += diff_g /4;
		diff_next[3*(x-1) +2] += diff_b /4;

		diff_next[3*(x+0) +0] += diff_r /4;
		diff_next[3*(x+0) +1] += diff_g /4;
		diff_next[3*(x+0) +2] += diff_b /4;

		temp = diff_cur;
		diff_cur = diff_next;
		diff_next = temp;
#else
		closest = closest_color_index( display, screen, color_list, num_cells,
					       pixel[0],
					       pixel[1],
					       pixel[2]) ;
#endif /* USE_SF */

		XSetForeground( display, gc, closest) ;
		XDrawPoint( display, pixmap, gc, x, y) ;
		line += rowstride ;
	}
	free( color_list) ;
	XFreeGC( display, gc) ;

#ifdef USE_FS
	free( diff_cur) ;
	free( diff_next) ;
#endif /* USE_SF */

	return  SUCCESS ;
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

	/* border width is not used */
	XGetGeometry( display, pixmap, &dummy, &ax, &ay,
		      &aw, &ah, &bw, &depth) ;

	if ( force_copy ||
	     aw < DisplayWidth( display, screen) ||
	     ah < DisplayHeight( display, screen))
	{
		/* Some WM needs tiling... sigh.*/
		result = XCreatePixmap( display, pixmap,
					DisplayWidth( display, screen),
					DisplayHeight( display, screen),
					depth) ;
		XSetTile( display, gc, pixmap) ;
		XSetTSOrigin( display, gc, 0, 0) ;
		XSetFillStyle( display, gc, FillTiled) ;
		/* XXX not correct with virtual desktop?  */
		XFillRectangle( display, result, gc, 0, 0,
				DisplayWidth( display, screen),
				DisplayHeight( display, screen)) ;

		return  result ;
	}

	return  pixmap ;
}

static Pixmap
root_pixmap(
	Display *  display
	)
{
	Atom  id ;
	int  act_format ;
	u_long  nitems ;
	u_long  bytes_after ;
	u_char *  prop ;

	id = XInternAtom( display, "_XROOTPMAP_ID", True) ;
	if( !id)
	{
#ifdef DEBUG
		kik_warn_printf(KIK_DEBUG_TAG "_XROOTPMAP_ID is not available\n") ;
#endif
		return  None ;
	}

	if( XGetWindowProperty( display, DefaultRootWindow(display), id, 0, 1,
				False, XA_PIXMAP, &id, &act_format,
				&nitems, &bytes_after, &prop) == Success)
	{
		if( prop)
		{
			Pixmap root ;
			root = *((Drawable *)prop) ;
			XFree( prop) ;

			return  root ;
		}
#ifdef DEBUG
		else
		{
			kik_warn_printf(KIK_DEBUG_TAG "failed to read prop\n") ;
		}
#endif
	}

	return  None ;
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

	return  cache ;
}

/* add a new entry to the head of the cache */
static pixmap_cache_t *
cache_add(
	Display *  display
	)
{
	pixmap_cache_t *  cache ;

	cache = malloc( sizeof( pixmap_cache_t)) ;
	if (!cache)
		return  NULL ;
	cache->display = display ;
	cache->root = None ;
	cache->cooked = None ;
	cache->pic_mod = NULL ;
	cache->next = pixmap_cache ;
	pixmap_cache = cache ;

	return  cache ;
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
 *\return  1 when they are same. 0 when not.
 */
static int
is_picmod_eq(
	x_picture_modifier_t *  a ,
	x_picture_modifier_t *  b
	)
{
	if( a == b)
		return  1 ;

	if ( a == NULL)
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
			return  1 ;
		}
	}
	else
	{
		if( (a->brightness == b->brightness) &&
		    (a->contrast == b->contrast) &&
		    (a->gamma == b->gamma))
		{
			return  1 ;
		}
	}
	return  0 ;
}

/**Return  position of the least significant bit
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
		return  0 ;

	while((val & 1) == 0)
	{
		val = val >>1 ;
		nth ++ ;
	}

	return  nth ;
}

/**Return  position of the most significant bit
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
		return  0 ;

	nth = lsb( val) +1 ;

	while(val & (1 << nth))
		nth++ ;

	return  nth ;
}


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

	if( !width || !height)
		return  NULL ;

	r_mask = vinfo[0].red_mask ;
	g_mask = vinfo[0].green_mask ;
	b_mask = vinfo[0].blue_mask ;
	r_offset = lsb( r_mask) ;
	g_offset = lsb( g_mask) ;
	b_offset = lsb( b_mask) ;

	bytes_per_pixel = (gdk_pixbuf_get_has_alpha( pixbuf)) ? 4:3 ;
	rowstride = gdk_pixbuf_get_rowstride (pixbuf) ;
	line = gdk_pixbuf_get_pixels( pixbuf) ;

	switch ( depth)
	{
	case 15:
	case 16:
	{
		int r_limit, g_limit, b_limit ;
		u_int16_t *data ;

		if( width > (SIZE_MAX / 2) / height)
			return  NULL ;

		data = (u_int16_t *)malloc( (size_t)width * height * 2) ;
		if( !data)
			return  NULL ;
		image = XCreateImage( display, DefaultVisual( display, screen),
				      DefaultDepth( display, screen), ZPixmap, 0,
				      (char *)data,
				      width, height,
				      16,
				      width *  2) ;
		r_limit = 8 + r_offset - msb( r_mask) ;
		g_limit = 8 + g_offset - msb( g_mask) ;
		b_limit = 8 + b_offset - msb( b_mask) ;
		for (i = 0; i < height; i++)
		{
			pixel = line ;
			for (j = 0; j < width; j++)
			{
				XPutPixel( image, j, i,
					   ( ( (pixel[0] >> r_limit) << r_offset) & r_mask) |
					   ( ( (pixel[1] >> g_limit) << g_offset) & g_mask) |
					   ( ( (pixel[2] >> b_limit) << b_offset) & b_mask)) ;
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

		if( width > (SIZE_MAX / 4) / height)
			return  NULL ;

		data = (u_int32_t *)malloc( (size_t)width *  height * 4) ;
		if( !data)
			return  NULL;

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
				XPutPixel( image, j, i, pixel[0] <<r_offset | pixel[1] <<g_offset | pixel[2]<<b_offset) ;
				pixel +=bytes_per_pixel ;
			}
			line += rowstride ;
		}
		break ;
	}
	default:
		break;
	}

	return  image ;
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
		return  -1 ;

	vinfo.visualid = XVisualIDFromVisual( DefaultVisual( display, screen)) ;
	if (!vinfo.visualid)
		return  -2 ;
	vinfolist = XGetVisualInfo( display, VisualIDMask, &vinfo, &matched) ;
	if( !vinfolist)
		return  -3 ;
	if ( !matched)
	{
		XFree( vinfolist) ;
		return  -4 ;
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
			return  SUCCESS ;
		}
		break ;
	}
	case PseudoColor:
	{
		XFree( vinfolist) ;
		if( pixbuf_to_pixmap_pseudocolor( display, screen, pixbuf, pixmap) != SUCCESS)
			return  -1;
		return  SUCCESS ;
		break ;
	}
	default:
	{
		XFree( vinfolist) ;
		break ;
	}
	}

	return  -1 ;
}

static XImage *
compose_truecolor(
	Display *  display,
	int  screen,
	GdkPixbuf *  pixbuf,
	Pixmap  pixmap,
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
	long  data ;

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

	for( i = 0; i < height; i++)
	{
		pixel = line ;
		for( j = 0; j < width; j++)
		{
			data = XGetPixel( image, j, i) ;

			r = ((data & r_mask) >>r_offset) ;
			g = ((data & g_mask) >>g_offset) ;
			b = ((data & b_mask) >>b_offset) ;

			r = (r*(256 - pixel[3]) + pixel[0] *  pixel[3])>>8 ;
			g = (g*(256 - pixel[3]) + pixel[1] *  pixel[3])>>8 ;
			b = (b*(256 - pixel[3]) + pixel[2] *  pixel[3])>>8 ;

			XPutPixel( image, j, i,
				   (r <<r_offset) |
				   (g <<g_offset) |
				   (b <<b_offset)) ;
			pixel += 4 ;
		}
		line += rowstride ;
	}

	return  image ;
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
	long  data ;
	XColor *  color_list ;

	num_cells = fetch_colormap( display, screen, &color_list) ;

	if( !num_cells)
		return  NULL ;

	width = gdk_pixbuf_get_width (pixbuf) ;
	height = gdk_pixbuf_get_height (pixbuf) ;

	image = XGetImage( display, pixmap, 0, 0, width, height, AllPlanes, ZPixmap) ;

	rowstride = gdk_pixbuf_get_rowstride( pixbuf) ;
	line = gdk_pixbuf_get_pixels( pixbuf) ;

	for( i = 0; i < height; i++)
	{
		pixel = line ;
		for( j = 0; j < width; j++)
		{
			data = XGetPixel( image, j ,i) ;
			r = color_list[data].red >>8 ;
			g = color_list[data].green >>8 ;
			b = color_list[data].blue >>8 ;

			r = (r*(256 - pixel[3]) + pixel[0] *  pixel[3])>>8 ;
			g = (g*(256 - pixel[3]) + pixel[1] *  pixel[3])>>8 ;
			b = (b*(256 - pixel[3]) + pixel[2] *  pixel[3])>>8 ;

			XPutPixel( image, j, i,
				   closest_color_index( display, screen,
							color_list, num_cells,
							r, g, b)) ;
			pixel += 4 ;
		}
		line += rowstride ;
	}
	free( color_list) ;

	return  image ;
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
		return  -1 ;
	vinfo.visualid = XVisualIDFromVisual( DefaultVisual( display, screen)) ;
	if (!vinfo.visualid)
		return  -2 ;
	vinfolist = XGetVisualInfo( display, VisualIDMask, &vinfo, &matched) ;
	if( !vinfolist)
		return  -3 ;
	if ( !matched)
	{
		XFree( vinfolist) ;
		return  -4 ;
	}

	switch( vinfolist[0].class)
	{
	case TrueColor:
		image = compose_truecolor( display,
					   screen,
					   pixbuf,
					   pixmap,
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

	XFree( vinfolist) ;

	if ( !image)
	{
		return  -5 ;
	}

	XPutImage( display, pixmap, DefaultGC( display, screen), image, 0, 0, 0, 0,
		   gdk_pixbuf_get_width( pixbuf), gdk_pixbuf_get_height( pixbuf)) ;
	XDestroyImage( image) ;

	return  SUCCESS ;
}

static int
pixbuf_to_pixmap_and_mask(
	Display *  display,
	int  screen,
	GdkPixbuf *  pixbuf,
	Pixmap *  pixmap,
	Pixmap *  mask
	)
{
	if( pixbuf_to_pixmap( display, screen, pixbuf, *pixmap) == -1)
		return  -1 ;

	if( gdk_pixbuf_get_has_alpha( pixbuf))
	{
		int  i, j ;
		int  width, height, rowstride ;
		unsigned char *  line ;
		unsigned char *  pixel ;
		GC  gc ;
		XGCValues  gcv ;

		width = gdk_pixbuf_get_width (pixbuf) ;
		height = gdk_pixbuf_get_height (pixbuf) ;

		*mask = XCreatePixmap( display,
				       DefaultRootWindow( display),
				       width, height, 1) ;
		gc = XCreateGC (display, *mask, 0, &gcv) ;

		XSetForeground( display, gc, 0) ;
		XFillRectangle( display, *mask, gc,
				0, 0, width, height) ;
		XSetForeground( display, gc, 1) ;

		line = gdk_pixbuf_get_pixels( pixbuf) ;
		rowstride = gdk_pixbuf_get_rowstride (pixbuf) ;

		for (i = 0; i < height; i++)
		{
			pixel = line + 3;
			for (j = 0; j < width; j++)
			{
				if( *pixel > 127)
					XDrawPoint( display, *mask, gc, j, i) ;
				pixel += 4 ;
			}
			line += rowstride ;
		}
		XFreeGC( display, gc) ;
	}
	else
	{ /* no mask */
		*mask = None ;
	}

	return  SUCCESS ;
}

static void
value_table_refresh(
	x_picture_modifier_t *  mod
	)
{
	int i, tmp ;
	double real_gamma , real_brightness, real_contrast;

	real_gamma = (double)(mod->gamma) / 100 ;
	real_contrast = (double)(mod->contrast) / 100 ;
	real_brightness = (double)(mod->brightness) / 100 ;

	for( i = 0 ; i < 256 ; i++)
	{
		tmp = real_contrast * (255 * pow(((double)i + 0.5)/ 255, real_gamma) -128)
			+ 128 *  real_brightness ;
		if( tmp >= 255)
			break;

		if( tmp < 0)
			value_table[i] = 0 ;
		else
			value_table[i] = tmp ;
	}
	for( ; i< 256 ; i++)
		value_table[i] = 255 ;
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

	if ( !pixbuf)
		return  -1 ;
	bytes_per_pixel = (gdk_pixbuf_get_has_alpha( pixbuf)) ? 4:3 ;
	width = gdk_pixbuf_get_width (pixbuf) ;
	height = gdk_pixbuf_get_height (pixbuf) ;
	rowstride = gdk_pixbuf_get_rowstride (pixbuf) ;

	if( !pic_mod)
		return  -2 ;
	if( is_picmod_eq( pic_mod, NULL))
		return  SUCCESS ;

	line = gdk_pixbuf_get_pixels( pixbuf) ;

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

	return  SUCCESS ;
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

	int  r_offset, g_offset, b_offset ;
	int  r_mask, g_mask, b_mask ;
	int  matched ;
	XVisualInfo *  vinfolist ;
	XVisualInfo  vinfo ;

	if ( !pixmap)
		return  -1 ;

	if( is_picmod_eq( pic_mod, NULL))
		return  SUCCESS ;

	vinfo.visualid = XVisualIDFromVisual( DefaultVisual( display, screen)) ;
	vinfolist = XGetVisualInfo( display, VisualIDMask, &vinfo, &matched) ;
	if( !vinfolist)
		return  -2;
	if ( !matched)
	{
		XFree( vinfolist) ;
		return  -3;
	}

	XGetGeometry( display, pixmap, &root, &x, &y,
		      &width, &height, &border, &depth) ;
	image = XGetImage( display, pixmap, 0, 0, width, height, AllPlanes, ZPixmap) ;

	switch( vinfolist[0].class)
	{
	case TrueColor:
	{
		unsigned char  r, g, b ;
		int r_limit, g_limit, b_limit ;
		long  data ;

		r_mask = vinfolist[0].red_mask ;
		g_mask = vinfolist[0].green_mask ;
		b_mask = vinfolist[0].blue_mask ;

		r_offset = lsb( r_mask) ;
		g_offset = lsb( g_mask) ;
		b_offset = lsb( b_mask) ;

		r_limit = 8 + r_offset - msb( r_mask) ;
		g_limit = 8 + g_offset - msb( g_mask) ;
		b_limit = 8 + b_offset - msb( b_mask) ;

		value_table_refresh( pic_mod) ;
		for (i = 0; i < height; i++)
		{
			for (j = 0; j < width; j++)
			{
				data = XGetPixel( image, j, i) ;

				r = ((data & r_mask) >> r_offset)<<r_limit ;
				g = ((data & g_mask) >> g_offset)<<g_limit ;
				b = ((data & b_mask) >> b_offset)<<b_limit ;

				r = value_table[r] ;
				g = value_table[g] ;
				b = value_table[b] ;

				XPutPixel( image, j, i,
					   (r >> r_limit) << r_offset |
					   (g >> g_limit) << g_offset |
					   (b >> b_limit) << b_offset) ;
			}
		}
		XPutImage( display, pixmap, DefaultGC( display, screen),
			   image, 0, 0, 0, 0, width, height) ;
		break ;
	}
	case PseudoColor:
		switch( vinfolist[0].depth)
		{
		case 8:
		{
			XColor *  color_list ;
			int num_cells ;
			long  data ;
			unsigned char  r, g, b ;
			num_cells = fetch_colormap( display, screen, &color_list) ;
			if( !num_cells)
				return  -7 ;

			value_table_refresh( pic_mod) ;
			for (i = 0; i < height; i++)
			{
				for (j = 0; j < width; j++)
				{
					data = XGetPixel( image, j, i) ;

					r = (color_list[data].red >>8)  ;
					g = (color_list[data].green >>8)  ;
					b = (color_list[data].blue >>8)  ;

					r = value_table[r] ;
					g = value_table[g] ;
					b = value_table[b] ;

					XPutPixel( image, j, i,
						   closest_color_index( display, screen,
									color_list, num_cells,
									r, g, b)) ;
				}

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

	return  SUCCESS ;
}

/* --- global functions --- */

int
x_imagelib_display_opened(
	Display *  display
	)
{
	if (display_count == 0)
	{
#if GDK_PIXBUF_MAJOR >= 2
		g_type_init() ;
#endif /*GDK_PIXBUF_MAJOR*/
	}
	display_count ++ ;
	return  1 ;
}

int
x_imagelib_display_closed(
	Display *  display
	)
{
	display_count -- ;
	cache_delete( display) ;

	if( display_count == 0)
	{
		/* drop pixbuf cache */
		load_file( NULL, 0, 0, 0) ;
	}

	return  1 ;
}

/** Load an image from the specified file.
 *\param win mlterm window.
 *\param path File full path.
 *\param pic_mod picture modifier.
 *
 *\return  Pixmap to be used as a window's background.
 */
Pixmap
x_imagelib_load_file_for_background(
	x_window_t *  win,
	char *  file_path,
	x_picture_modifier_t *  pic_mod
	)
{
	static x_picture_modifier_t *  cached_mod = NULL ;
	static GdkPixbuf *  cached_pixbuf = NULL ;
	static char *  cached_pixbuf_file_path = NULL ;

	GdkPixbuf *  pixbuf ;
	Pixmap pixmap ;

	if(!file_path)
		return  None ;

	if( cached_pixbuf
	    && (is_picmod_eq( pic_mod, cached_mod))
	    && ACTUAL_WIDTH(win) == gdk_pixbuf_get_width( cached_pixbuf)
	    && ACTUAL_HEIGHT(win) == gdk_pixbuf_get_height( cached_pixbuf)
	    && (strcmp( file_path, cached_pixbuf_file_path) == 0))
	{
		/* use pixbuf in the cache */
		pixbuf = cached_pixbuf ;
	}
	else
	{
		/* re-generate cache */
		if( !( pixbuf = load_file( file_path,
					   ACTUAL_WIDTH(win), ACTUAL_HEIGHT(win),
					   GDK_INTERP_NEAREST)))
		{
			return  None ;
		}

		if( cached_pixbuf)
		{
			g_object_unref( cached_pixbuf) ;
		}

		free(cached_mod) ;
		free(cached_pixbuf_file_path) ;

		cached_pixbuf_file_path = strdup( file_path) ;

		if( is_picmod_eq( pic_mod, NULL))
		{
			cached_pixbuf = pixbuf ;
		}
		else
		{
			cached_pixbuf = gdk_pixbuf_copy(pixbuf) ;
			g_object_unref( pixbuf) ;
			modify_image( cached_pixbuf, pic_mod) ;
		}

		if( pic_mod)
		{
			cached_mod = malloc(sizeof(x_picture_modifier_t)) ;
			memcpy( cached_mod, pic_mod, sizeof(x_picture_modifier_t)) ;
		}
		else
		{
			cached_mod = NULL ;
		}

		pixbuf = cached_pixbuf ;
	}

	if( gdk_pixbuf_get_has_alpha ( pixbuf) && (pixmap = x_imagelib_get_transparent_background( win, NULL)))
	{
		if( compose_to_pixmap( win->disp->display, win->disp->screen,
				       pixbuf, pixmap) != SUCCESS)
		{
			XFreePixmap( win->disp->display, pixmap) ;
			return  None ;
		}
	}
	else
	{
		pixmap = XCreatePixmap( win->disp->display, win->my_window,
					ACTUAL_WIDTH(win), ACTUAL_HEIGHT(win),
					DefaultDepth( win->disp->display, win->disp->screen)) ;
		if( pixbuf_to_pixmap( win->disp->display, win->disp->screen,
				      pixbuf, pixmap) != SUCCESS)
		{
			XFreePixmap( win->disp->display, pixmap) ;
			return  None ;
		}
	}

	return  pixmap ;
}

/** Answer whether pseudo transparency is available
 *\param display connection to X server.
 *
 *\return  Success => 1, Failure => 0
 */
int
x_imagelib_root_pixmap_available(
	Display *  display
	)
{
	if( root_pixmap( display))
		return  1 ;
	return  0 ;
}

/** Create an pixmap from root window
 *\param win window structure
 *\param pic_mod picture modifier
 *
 *\return  Newly allocated Pixmap (or None in the case of failure)
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

	current_root =  root_pixmap( win->disp->display) ;

	if(current_root == None)
		return  None;

	cache = cache_seek( win->disp->display) ;
	if( cache)
	{
/* discard old info */
		if ( cache->root !=  current_root)
		{
			if((cache->cooked != None) && (cache->cooked != cache->root))
				XFreePixmap( win->disp->display, cache->cooked) ;
			cache->root = current_root ;

			cache->cooked = None ;

			free(cache->pic_mod) ;
			cache->pic_mod = NULL ;
		}
	}
	else
	{
		cache = cache_add( win->disp->display) ;
		if( !cache)
			return  None ;
		cache->root = current_root ;
	}

	if( ! x_window_get_visible_geometry( win, &x, &y, &pix_x, &pix_y, &width, &height))
		return  None ;

	/* The pixmap to be returned */
	pixmap = XCreatePixmap( win->disp->display, win->my_window, ACTUAL_WIDTH(win), ACTUAL_HEIGHT(win),
				DefaultDepth( win->disp->display, win->disp->screen)) ;

	gc = XCreateGC( win->disp->display, win->my_window, 0, NULL) ;

	if ( !is_picmod_eq( cache->pic_mod, pic_mod))
	{
		if((cache->cooked != None) && (cache->cooked != cache->root))
			XFreePixmap( win->disp->display, cache->cooked) ;

		/* re-creation */
		if(pic_mod)
		{
			/* we need a copy of pixmap to modify */
			cache->cooked = tile_pixmap( win->disp->display,
						     win->disp->screen,
						     gc,
						     current_root
						     ,1) ;
			free(cache->pic_mod) ;
			cache->pic_mod = malloc(sizeof(x_picture_modifier_t)) ;
			memcpy( cache->pic_mod, pic_mod, sizeof(x_picture_modifier_t)) ;
			modify_pixmap( win->disp->display, win->disp->screen, cache->cooked, pic_mod) ;
		}
		else
		{
			cache->cooked = tile_pixmap( win->disp->display,
						     win->disp->screen,
						     gc,
						     current_root,
						     0) ;
		}
	}
	else
	{
		if( cache->cooked == None)
		{
			cache->cooked = tile_pixmap( win->disp->display,
						     win->disp->screen,
						     gc,
						     current_root,
						     0) ;
		}
	}

	XCopyArea( win->disp->display, cache->cooked, pixmap, gc, x, y, width, height, pix_x, pix_y) ;
	XFreeGC( win->disp->display, gc) ;

	return  pixmap ;
}

/** Load an image from the specified file with alpha plane. A pixmap and a mask are returned.
 *\param display connection to the X server.
 *\param path File full path.
 *\param cardinal Returns pointer to a data structure for the extended WM hint spec.
 *\param pixmap Returns an image pixmap for the old WM hint.
 *\param mask Returns a mask bitmap for the old WM hint.
 *\param width Pointer to the desired width. If *width is 0, the returned image would not be scaled and *width would be overwritten by its width. "width" can be NULL and the image would not be scaled and nothing would be returned in this case.
 *\param height Pointer to the desired height. *height can be 0 and height can be NULL(see "width" 's description)
 *
 *\return  Success => 1, Failure => 0
 */
int x_imagelib_load_file(
	Display *  display,
	char *  path,
	u_int32_t **  cardinal,
	Pixmap *  pixmap,
	Pixmap *  mask,
	unsigned int *  width,
	unsigned int *  height
	)
{
	GdkPixbuf *  pixbuf ;
	unsigned int  dst_height, dst_width ;
	if( !width)
	{
		dst_width = 0 ;
	}
	else
	{
		dst_width = *width ;
	}
	if( !height)
	{
		dst_height = 0 ;
	}
	else
	{
		dst_height = *height ;
	}

	if( path)
	{
		/* create a pixbuf from the file and create a cardinal array */
		if( !( pixbuf = load_file( path, dst_width, dst_height, GDK_INTERP_NEAREST)))
		{
#ifdef DEBUG
			kik_warn_printf(KIK_DEBUG_TAG "couldn't load pixbuf\n") ;
#endif
			return  0 ;
		}
		if ( cardinal)
			create_cardinals_from_bixbuf( cardinal, dst_width, dst_height, pixbuf) ;
	}
	else
	{
		if( !cardinal || !(*cardinal))
			return  0 ;
		/* create a pixbuf from the cardinal array */
		pixbuf = create_pixbuf_from_cardinals( *cardinal, dst_width, dst_height) ;
		if( !pixbuf)
			return  0 ;

	}

	dst_width = gdk_pixbuf_get_width( pixbuf) ;
	dst_height = gdk_pixbuf_get_height( pixbuf) ;

	/* Create the Icon pixmap & mask to be used in WMHints.
	 * Note that none as a result is acceptable.
	 * Pixmaps can't be cached since the last pixmap may be freed by someone... */

	if( pixmap)
	{
		*pixmap = XCreatePixmap( display, DefaultRootWindow( display),
					 dst_width, dst_height,
					 DefaultDepth( display, DefaultScreen( display))) ;
		if( mask)
		{
			if( pixbuf_to_pixmap_and_mask( display,
						       DefaultScreen( display),
						       pixbuf, pixmap, mask) != SUCCESS)
			{
				g_object_unref( pixbuf) ;
				XFreePixmap( display, *pixmap) ;
				*pixmap = None ;
				XFreePixmap( display, *mask) ;
				*mask = None ;

				return  0 ;
			}

		}
		else
		{
			if( pixbuf_to_pixmap( display, DefaultScreen( display),
					      pixbuf, *pixmap) != SUCCESS)
			{
				g_object_unref( pixbuf) ;
				XFreePixmap( display, *pixmap) ;
				*pixmap = None ;

				return  0 ;
			}
		}
	}
	if( width && *width == 0)
	{
		*width = dst_width ;
	}

	if( height && *height == 0)
	{
		*height = dst_height ;
	}

	g_object_unref( pixbuf) ;
	return  1 ;
}
