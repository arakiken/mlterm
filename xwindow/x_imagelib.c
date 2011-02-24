/** @file
 *  @brief image handling functions using gdk-pixbuf
 *	$Id$
 */

#include <X11/Xatom.h>		/* XInternAtom */
#include <X11/Xutil.h>
#include <string.h>		/* memcpy */
#include <stdio.h>		/* sscanf */
#ifdef  USE_EXT_IMAGELIB
#include <gdk-pixbuf/gdk-pixbuf.h>
#endif
#ifdef  DLOPEN_LIBM
#include <kiklib/kik_dlfcn.h>	/* dynamically loading pow */
#else
#include <math.h>		/* pow */
#endif

#include <kiklib/kik_debug.h>
#include <kiklib/kik_types.h>	/* u_int32_t/u_int16_t, HAVE_STDINT_H */
#include <kiklib/kik_unistd.h>
#include <kiklib/kik_str.h>	/* strdup */
#include <kiklib/kik_util.h>	/* K_MIN */

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

#if  (GDK_PIXBUF_MAJOR < 2)
#define  g_object_ref( pixbuf) gdk_pixbuf_ref( pixbuf)
#define  g_object_unref( pixbuf) gdk_pixbuf_unref( pixbuf)
#endif

/* Trailing "/" is appended in value_table_refresh(). */
#ifndef  LIBMDIR
#define  LIBMDIR  "/lib"
#endif

#ifndef  LIBEXECDIR
#define  LIBEXECDIR  "/usr/local/libexec"
#endif

#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static int  display_count = 0 ;


/* --- static functions --- */

static Status
get_drawable_size(
	Display *  display ,
	Drawable  drawable ,
	u_int *  width ,
	u_int *  height
	)
{
	Window  root ;
	int  x ;
	int  y ;
	u_int  border ;
	u_int  depth ;
	
	return  XGetGeometry( display , drawable , &root , &x , &y , width , height ,
			&border , &depth) ;
}

/* returned cmap shuold be freed by the caller */
static int
fetch_colormap(
	x_display_t *  disp ,
	XColor **  color_list
	)
{
	int  num_cells , i ;

	num_cells = disp->visual->map_entries ;

	if( ( *color_list = calloc( num_cells , sizeof(XColor))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf(KIK_DEBUG_TAG "couldn't allocate color table\n") ;
	#endif
		return  0 ;
	}

	for( i = 0 ; i < num_cells ; i ++)
	{
		((*color_list)[i]).pixel = i ;
	}

	XQueryColors( disp->display , disp->colormap , *color_list, num_cells) ;

	return  num_cells ;
}

/* seek the closest color */
static int
closest_color_index(
	XColor *  color_list ,
	int  len ,
	int  red ,
	int  green ,
	int  blue
	)
{
	int  closest = 0 ;
	int  i ;
	unsigned long  min = 0xffffff ;
	unsigned long  diff ;
	int  diff_r , diff_g , diff_b ;

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
			{
				break ;
			}
		}
	}

	return  closest ;
}

/* Get an background pixmap from _XROOTMAP_ID */
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
			kik_warn_printf(KIK_DEBUG_TAG " Failed to read prop\n") ;
		}
	#endif
	}

	return  None ;
}

/**Return position of the least significant bit
 *
 *\param val value to count
 *
 */
static int
lsb(
	u_int  val
	)
{
	int nth = 0 ;

	if( val == 0)
	{
		return  0 ;
	}

	while((val & 1) == 0)
	{
		val = val >> 1 ;
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
	u_int val
	)
{
	int nth ;

	if( val == 0)
	{
		return  0 ;
	}

	nth = lsb( val) + 1 ;

	while(val & (1 << nth))
	{
		nth++ ;
	}

	return  nth ;
}

#include <dlfcn.h>

static void
value_table_refresh(
	u_char *  value_table ,		/* 256 bytes */
	x_picture_modifier_t *  mod
	)
{
	int i , tmp ;
	double real_gamma , real_brightness , real_contrast ;
	static double (*pow_func)( double , double) ;

	real_gamma = (double)(mod->gamma) / 100 ;
	real_contrast = (double)(mod->contrast) / 100 ;
	real_brightness = (double)(mod->brightness) / 100 ;

	if( ! pow_func)
	{
	#ifdef  DLOPEN_LIBM
		kik_dl_handle_t  handle ;

		if( ! ( handle = kik_dl_open( LIBMDIR "/" , "m")) ||
		    ! ( pow_func = kik_dl_func_symbol( handle , "pow")))
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Failed to load pow in libm.so\n") ;
		#endif

			if( handle)
			{
				kik_dl_close( handle) ;
			}

			/*
			 * gamma, contrast and brightness options are ignored.
			 * (alpha option still survives.)
			 */
			for( i = 0 ; i < 256 ; i++)
			{
				value_table[i] = i ;
			}

			return ;
		}
	#else  /* DLOPEN_LIBM */
		pow_func = pow ;
	#endif /* USE_EXT_IMAGELIB */
	}
	
	for( i = 0 ; i < 256 ; i++)
	{
		tmp = real_contrast * (255 * (*pow_func)(((double)i + 0.5)/ 255, real_gamma) -128)
			+ 128 *  real_brightness ;
		if( tmp >= 255)
		{
			break;
		}
		else if( tmp < 0)
		{
			value_table[i] = 0 ;
		}
		else
		{
			value_table[i] = tmp ;
		}
	}

	for( ; i < 256 ; i++)
	{
		value_table[i] = 255 ;
	}
}

static int
modify_pixmap(
	x_display_t *  disp ,
	Pixmap  src_pixmap ,
	Pixmap  dst_pixmap ,		/* Can be same as src_pixmap */
	x_picture_modifier_t *  pic_mod	/* Mustn't be normal */
	)
{
	u_char  value_table[256] ;
	u_int  i , j ;
	u_int  width , height ;
	XImage *  image ;
	u_char  r , g , b ;
	long  data ;

	if( disp->visual->class != TrueColor &&
		! (disp->visual->class == PseudoColor && disp->depth == 8))
	{
		return  0 ;
	}

	get_drawable_size( disp->display , src_pixmap , &width , &height) ;
	if( ( image = XGetImage( disp->display , src_pixmap , 0 , 0 , width , height ,
			AllPlanes , ZPixmap)) == NULL)
	{
		return  0 ;
	}

	value_table_refresh( value_table , pic_mod) ;

	if( disp->visual->class == TrueColor)
	{
		XVisualInfo *  vinfo ;
		int  r_mask , g_mask , b_mask ;
		int  r_limit , g_limit , b_limit ;
		int  r_offset , g_offset , b_offset ;

		if( ! ( vinfo = x_display_get_visual_info( disp)))
		{
			XDestroyImage( image) ;

			return  0 ;
		}

		r_mask = vinfo->red_mask ;
		g_mask = vinfo->green_mask ;
		b_mask = vinfo->blue_mask ;

		XFree( vinfo) ;

		r_offset = lsb( r_mask) ;
		g_offset = lsb( g_mask) ;
		b_offset = lsb( b_mask) ;

		r_limit = 8 + r_offset - msb( r_mask) ;
		g_limit = 8 + g_offset - msb( g_mask) ;
		b_limit = 8 + b_offset - msb( b_mask) ;

		for( i = 0 ; i < height ; i++)
		{
			for( j = 0 ; j < width ; j++)
			{
				data = XGetPixel( image , j , i) ;

				r = ((data & r_mask) >> r_offset) << r_limit ;
				g = ((data & g_mask) >> g_offset) << g_limit ;
				b = ((data & b_mask) >> b_offset) << b_limit ;

				r = (value_table[r] * pic_mod->alpha +
					pic_mod->blend_red * (255 - pic_mod->alpha)) / 255 ;
				g = (value_table[g] * pic_mod->alpha +
					pic_mod->blend_green * (255 - pic_mod->alpha)) / 255 ;
				b = (value_table[b] * pic_mod->alpha +
					pic_mod->blend_blue * (255 - pic_mod->alpha)) / 255 ;

				XPutPixel( image , j , i ,
					   (r >> r_limit) << r_offset |
					   (g >> g_limit) << g_offset |
					   (b >> b_limit) << b_offset) ;
			}
		}
	}
	else /* if( disp->visual->class == PseudoColor && disp->depth == 8) */
	{
		XColor *  color_list ;
		int  num_cells ;

		if( ( num_cells = fetch_colormap( disp , &color_list)) == 0)
		{
			XDestroyImage( image) ;

			return  0 ;
		}

		for( i = 0 ; i < height ; i++)
		{
			for( j = 0 ; j < width ; j++)
			{
				data = XGetPixel( image, j, i) ;

				r = (color_list[data].red >>8)  ;
				g = (color_list[data].green >>8)  ;
				b = (color_list[data].blue >>8)  ;

				r = (value_table[r] * pic_mod->alpha +
					pic_mod->blend_red * (255 - pic_mod->alpha)) / 255 ;
				g = (value_table[g] * pic_mod->alpha +
					pic_mod->blend_green * (255 - pic_mod->alpha)) / 255 ;
				b = (value_table[b] * pic_mod->alpha +
					pic_mod->blend_blue * (255 - pic_mod->alpha)) / 255 ;

				XPutPixel( image , j , i ,
					closest_color_index( color_list , num_cells , r , g , b)) ;
			}

		}

		free( color_list) ;
	}

	XPutImage( disp->display , dst_pixmap , disp->gc->gc , image ,
		0 , 0 , 0 , 0 , width , height) ;

	XDestroyImage( image) ;

	return  1 ;
}


#ifdef  USE_EXT_IMAGELIB

/* create GdkPixbuf from the specified file path.
 *
 * The returned pixbuf shouled be unrefed by the caller
 * don't modify returned pixbuf since the pixbuf
 * is stored in the cache and may be reused.
 */
static GdkPixbuf *
load_file(
	char *  path ,		/* If NULL is specified, cache is cleared. */
	u_int  width ,		/* 0 == image width */
	u_int  height ,		/* 0 == image height */
	GdkInterpType scale_type
	)
{
	static char *  name = NULL ;
	static GdkPixbuf *  orig_cache = NULL ;
	static GdkPixbuf *  scaled_cache = NULL ;
	GdkPixbuf *  pixbuf ;

	pixbuf = NULL ;

	if( ! path)
	{
		/* free caches */
		if( orig_cache)
		{
			g_object_unref( orig_cache) ;
			orig_cache = NULL ;
		}
		
		if( scaled_cache)
		{
			g_object_unref( scaled_cache) ;
			scaled_cache = NULL ;
		}
		
		return  NULL ;
	}

	if( name == NULL || (strcmp( name , path) != 0))
	{
		/* create new pixbuf */

	#if GDK_PIXBUF_MAJOR >= 2
		pixbuf = gdk_pixbuf_new_from_file( path , NULL) ;
	#else
		pixbuf = gdk_pixbuf_new_from_file( path) ;
	#endif /*GDK_PIXBUF_MAJOR*/

		if( pixbuf == NULL)
		{
			return  NULL ;
		}

	#ifdef  __DEBUG
		kik_warn_printf(KIK_DEBUG_TAG " adding pixbuf to cache(%s)\n" , path) ;
	#endif

		/* Replace cache */
		
		free( name) ;
		name = strdup( path) ;
		
		if( orig_cache)
		{
			g_object_unref( orig_cache) ;
		}
		orig_cache = pixbuf ;

		if( scaled_cache) /* scaled_cache one is not vaild now */
		{
			g_object_unref( scaled_cache) ;
			scaled_cache = NULL ;
		}
	}
	else
	{
	#ifdef __DEBUG
		kik_warn_printf(KIK_DEBUG_TAG " using the pixbuf from cache\n") ;
	#endif
		pixbuf = orig_cache ;
	}
	/* loading from file/cache ends here */

	if( width == 0)
	{
		width = gdk_pixbuf_get_width( orig_cache) ;
	}
	if( height == 0)
	{
		height = gdk_pixbuf_get_height( orig_cache) ;
	}
	
	/* It is necessary to scale orig_cache if width/height don't correspond. */
	if( ( width != gdk_pixbuf_get_width( orig_cache)) ||
	    ( height != gdk_pixbuf_get_height( orig_cache)))
	{
		/* Old cached scaled_cache pixbuf became obsolete if width/height is changed */
		if( scaled_cache &&
		    gdk_pixbuf_get_width( scaled_cache) == width &&
		    gdk_pixbuf_get_height( scaled_cache) == height)
		{
		#ifdef __DEBUG
			kik_warn_printf(KIK_DEBUG_TAG
				" using the scaled_cache pixbuf(%u x %u) from cache\n" ,
				width , height) ;
		#endif
		
			pixbuf = scaled_cache ;
		}
		else
		{
			if( ! ( pixbuf = gdk_pixbuf_scale_simple( orig_cache ,
						width , height , scale_type)))
			{
				return  NULL ;
			}
			
		#ifdef __DEBUG
			kik_warn_printf( KIK_DEBUG_TAG
				"creating a scaled_cache pixbuf(%u x %u)\n", width, height) ;
		#endif

			if( scaled_cache)
			{
				g_object_unref( scaled_cache) ;
			}
			scaled_cache = pixbuf ;
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
	u_char *  line ;
	u_char *  pixel ;
	int  width , height ;
	int  i , j ;

	width = cardinal[0] ;
	height = cardinal[1] ;
	
	if( ( pixbuf = gdk_pixbuf_new( GDK_COLORSPACE_RGB , TRUE , 8 , width , height)) == NULL)
	{
		return  NULL ;
	}

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
	{
		req_width = width ;
	}
	if( req_height == 0)
	{
		req_height = height ;
	}

	if( (req_width != width) || (req_height != height))
	{
		scaled = gdk_pixbuf_scale_simple( pixbuf , req_width , req_height ,
						 GDK_INTERP_TILES) ;
	}
	else
	{
		scaled = NULL ;
	}

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
create_cardinals_from_pixbuf(
	u_int32_t **  cardinal ,
	u_int  width ,
	u_int  height ,
	GdkPixbuf *  pixbuf
	)
{
	int  rowstride ;
	u_char *line ;
	u_char *pixel ;
	u_int i , j ;

	if( width > ((SIZE_MAX / 4) -2) / height)
	{
		return  0 ; /* integer overflow */
	}

	if( ( *cardinal = malloc( ((size_t)width * height + 2) *4)) == NULL)
	{
		return  0 ;
	}

	/* create (maybe shriked) copy */
	pixbuf = gdk_pixbuf_scale_simple( pixbuf , width , height , GDK_INTERP_TILES) ;

	rowstride = gdk_pixbuf_get_rowstride( pixbuf) ;
	line = gdk_pixbuf_get_pixels( pixbuf) ;

	/* format of the array is {width, height, ARGB[][]} */
	(*cardinal)[0] = width ;
	(*cardinal)[1] = height ;
	if( gdk_pixbuf_get_has_alpha( pixbuf))
	{
		for( i = 0 ; i < height ; i++)
		{
			pixel = line ;
			line += rowstride;
			for( j = 0 ; j < width ; j++)
			{
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
		for( i = 0 ; i < height ; i++)
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

	return  1 ;
}

static int
pixbuf_to_pixmap_pseudocolor(
	x_display_t *  disp,
	GdkPixbuf *  pixbuf,
	Pixmap pixmap
	)
{
	int  width , height , rowstride ;
	u_int  bytes_per_pixel ;
	int  x , y ;
	int  num_cells ;
#ifdef USE_FS
	char *  diff_next ;
	char *  diff_cur ;
	char *  temp ;
#endif /* USE_FS */
	u_char *  line ;
	u_char *  pixel ;
	XColor *  color_list ;
	int  closest ;
	int  diff_r , diff_g , diff_b ;
	int  ret_val = 0 ;

	if( ( num_cells = fetch_colormap( disp , &color_list)) == 0)
	{
		return  0 ;
	}

	width = gdk_pixbuf_get_width( pixbuf) ;
	height = gdk_pixbuf_get_height( pixbuf) ;

#ifdef USE_FS
	if( ( diff_cur = calloc( 1 , width * 3)) == NULL)
	{
		goto  error1 ;
	}
	if( ( diff_next = calloc( 1 , width * 3)) == NULL)
	{
		goto  error2 ;
	}
#endif /* USE_FS */

	bytes_per_pixel = (gdk_pixbuf_get_has_alpha( pixbuf)) ? 4 : 3 ;
	rowstride = gdk_pixbuf_get_rowstride( pixbuf) ;

	line = gdk_pixbuf_get_pixels( pixbuf) ;

	for( y = 0 ; y < height ; y++)
	{
		pixel = line ;
#ifdef USE_FS
		closest = closest_color_index( color_list , num_cells ,
					       pixel[0] - diff_cur[0] ,
					       pixel[1] - diff_cur[1] ,
					       pixel[2] - diff_cur[2]) ;
		diff_r = (color_list[closest].red   >>8) - pixel[0] ;
		diff_g = (color_list[closest].green >>8) - pixel[1] ;
		diff_b = (color_list[closest].blue  >>8) - pixel[2] ;

		diff_cur[3*1 + 0 ] += diff_r /2 ;
		diff_cur[3*1 + 1 ] += diff_g /2 ;
		diff_cur[3*1 + 2 ] += diff_b /2 ;

		/* initialize next line */
		diff_next[3*0 +0] = diff_r /4 ;
		diff_next[3*0 +1] = diff_g /4 ;
		diff_next[3*0 +2] = diff_b /4 ;

		diff_next[3*1 +0] = diff_r /4 ;
		diff_next[3*1 +1] = diff_g /4 ;
		diff_next[3*1 +2] = diff_b /4 ;
#else
		closest = closest_color_index( color_list , num_cells ,
					       pixel[0] , pixel[1] , pixel[2]) ;
#endif /* USE_FS */

		XSetForeground( disp->display , disp->gc->gc , closest) ;
		XDrawPoint( disp->display , pixmap , disp->gc->gc , 0 , y) ;
		pixel += bytes_per_pixel ;

		for( x = 1 ; x < width -2 ; x++)
		{
#ifdef USE_FS
			closest = closest_color_index( color_list , num_cells ,
						       pixel[0] - diff_cur[3*x +0] ,
						       pixel[1] - diff_cur[3*x +1] ,
						       pixel[2] - diff_cur[3*x +2]) ;
			diff_r = (color_list[closest].red   >>8) - pixel[0] ;
			diff_g = (color_list[closest].green >>8) - pixel[1] ;
			diff_b = (color_list[closest].blue  >>8) - pixel[2] ;

			diff_cur[3*(x+1) + 0 ] += diff_r /2 ;
			diff_cur[3*(x+1) + 1 ] += diff_g /2 ;
			diff_cur[3*(x+1) + 2 ] += diff_b /2 ;

			diff_next[3*(x-1) +0] += diff_r /8 ;
			diff_next[3*(x-1) +1] += diff_g /8 ;
			diff_next[3*(x-1) +2] += diff_b /8 ;

			diff_next[3*(x+0) +0] += diff_r /8 ;
			diff_next[3*(x+0) +1] += diff_g /8 ;
			diff_next[3*(x+0) +2] += diff_b /8 ;
			/* initialize next line */
			diff_next[3*(x+1) +0] = diff_r /4 ;
			diff_next[3*(x+1) +1] = diff_g /4 ;
			diff_next[3*(x+1) +2] = diff_b /4 ;
#else
			closest = closest_color_index( color_list , num_cells ,
						       pixel[0] , pixel[1] , pixel[2]) ;
#endif /* USE_FS */

			XSetForeground( disp->display , disp->gc->gc , closest) ;
			XDrawPoint( disp->display , pixmap , disp->gc->gc , x , y) ;

			pixel += bytes_per_pixel ;
		}
#ifdef USE_FS
		closest = closest_color_index( color_list , num_cells ,
					       pixel[0] - diff_cur[3*x +0] ,
					       pixel[1] - diff_cur[3*x +1] ,
					       pixel[2] - diff_cur[3*x +2]) ;
		diff_r = (color_list[closest].red   >>8) - pixel[0] ;
		diff_g = (color_list[closest].green >>8) - pixel[1] ;
		diff_b = (color_list[closest].blue  >>8) - pixel[2] ;

		diff_next[3*(x-1) +0] += diff_r /4 ;
		diff_next[3*(x-1) +1] += diff_g /4 ;
		diff_next[3*(x-1) +2] += diff_b /4 ;

		diff_next[3*(x+0) +0] += diff_r /4 ;
		diff_next[3*(x+0) +1] += diff_g /4 ;
		diff_next[3*(x+0) +2] += diff_b /4 ;

		temp = diff_cur ;
		diff_cur = diff_next ;
		diff_next = temp ;
#else
		closest = closest_color_index( color_list , num_cells ,
					       pixel[0] , pixel[1] , pixel[2]) ;
#endif /* USE_FS */

		XSetForeground( disp->display , disp->gc->gc , closest) ;
		XDrawPoint( disp->display , pixmap , disp->gc->gc , x , y) ;
		line += rowstride ;
	}

	ret_val = 1 ;

#ifdef USE_FS
error2:
	free( diff_cur) ;
	free( diff_next) ;
#endif /* USE_FS */

error1:
	free( color_list) ;

	return  ret_val ;
}

static XImage *
pixbuf_to_ximage_truecolor(
	x_display_t *  disp ,
	GdkPixbuf *  pixbuf
	)
{
	XVisualInfo *  vinfo ;
	u_int  i , j ;
	u_int  width , height , rowstride , bytes_per_pixel ;
	u_char *  line ;
	u_char *  pixel ;
	long  r_mask , g_mask , b_mask ;
	int  r_offset , g_offset , b_offset ;

	XImage *  image = NULL ;

	width = gdk_pixbuf_get_width( pixbuf) ;
	height = gdk_pixbuf_get_height( pixbuf) ;

	if( ! ( vinfo = x_display_get_visual_info( disp)))
	{
		return  NULL ;
	}

	r_mask = vinfo->red_mask ;
	g_mask = vinfo->green_mask ;
	b_mask = vinfo->blue_mask ;

	XFree( vinfo) ;
	
	r_offset = lsb( r_mask) ;
	g_offset = lsb( g_mask) ;
	b_offset = lsb( b_mask) ;

	bytes_per_pixel = (gdk_pixbuf_get_has_alpha( pixbuf)) ? 4 : 3 ;
	rowstride = gdk_pixbuf_get_rowstride( pixbuf) ;
	line = gdk_pixbuf_get_pixels( pixbuf) ;

	if( disp->depth == 15 || disp->depth == 16)
	{
		int  r_limit , g_limit , b_limit ;
		u_int16_t *  data ;

		if( width > (SIZE_MAX / 2) / height)
		{
			return  NULL ;
		}

		if( ! ( data = (u_int16_t *)malloc( (size_t)width * height * 2)))
		{
			return  NULL ;
		}

		if( ! ( image = XCreateImage( disp->display , disp->visual , disp->depth ,
				ZPixmap , 0 , (char *)data , width , height , 16 , width *  2)))
		{
			free( data) ;

			return  NULL ;
		}

		r_limit = 8 + r_offset - msb( r_mask) ;
		g_limit = 8 + g_offset - msb( g_mask) ;
		b_limit = 8 + b_offset - msb( b_mask) ;
		
		for( i = 0 ; i < height ; i++)
		{
			pixel = line ;
			for( j = 0 ; j < width ; j++)
			{
				XPutPixel( image , j , i ,
					   ( ( (pixel[0] >> r_limit) << r_offset) & r_mask) |
					   ( ( (pixel[1] >> g_limit) << g_offset) & g_mask) |
					   ( ( (pixel[2] >> b_limit) << b_offset) & b_mask)) ;
				pixel += bytes_per_pixel ;
			}
			line += rowstride ;
		}
	}
	else if( disp->depth == 24 || disp->depth == 32)
	{
		u_int32_t *  data ;

		if( width > (SIZE_MAX / 4) / height)
		{
			return  NULL ;
		}

		if( ! ( data = (u_int32_t *)malloc( (size_t)width *  height * 4)))
		{
			return  NULL ;
		}

		if( ! ( image = XCreateImage( disp->display, disp->visual , disp->depth ,
				ZPixmap , 0 , (char *)data , width , height , 32 , width * 4)))
		{
			free( data) ;

			return  NULL ;
		}

		for( i = 0 ; i < height ; i++)
		{
			pixel = line ;
			for( j = 0 ; j < width ; j++)
			{
				XPutPixel( image , j , i ,
					pixel[0] << r_offset |
					pixel[1] << g_offset |
					pixel[2] << b_offset) ;
				pixel +=bytes_per_pixel ;
			}
			line += rowstride ;
		}
	}
	else
	{
		return  NULL ;
	}

	return  image ;
}

static int
pixbuf_to_pixmap(
	x_display_t *  disp ,
	GdkPixbuf *  pixbuf ,
	Pixmap  pixmap
	)
{
	if( disp->visual->class == TrueColor)
	{
		XImage *  image ;

		if( ( image = pixbuf_to_ximage_truecolor( disp , pixbuf)))
		{
			XPutImage( disp->display , pixmap , disp->gc->gc , image ,
				0 , 0 , 0 , 0 ,
				gdk_pixbuf_get_width( pixbuf) ,
				gdk_pixbuf_get_height( pixbuf)) ;
			XDestroyImage( image) ;

			return  1 ;
		}
	}
	else if( disp->visual->class == TrueColor)
	{
		return  pixbuf_to_pixmap_pseudocolor( disp , pixbuf , pixmap) ;
	}
	
	return  0 ;
}

static int
pixbuf_to_pixmap_and_mask(
	x_display_t *  disp ,
	GdkPixbuf *  pixbuf ,
	Pixmap *  pixmap ,
	Pixmap *  mask
	)
{
	if( ! pixbuf_to_pixmap( disp, pixbuf, *pixmap))
	{
		return  0 ;
	}

	if( gdk_pixbuf_get_has_alpha( pixbuf))
	{
		int  i , j ;
		int  width , height , rowstride ;
		u_char *  line ;
		u_char *  pixel ;
		GC  mask_gc ;
		XGCValues  gcv ;

		width = gdk_pixbuf_get_width( pixbuf) ;
		height = gdk_pixbuf_get_height( pixbuf) ;

		/*
		 * DefaultRootWindow should not be used because depth and visual
		 * of DefaultRootWindow don't always match those of mlterm window.
		 * Use x_display_get_group_leader instead.
		 */
		*mask = XCreatePixmap( disp->display ,
				       x_display_get_group_leader( disp) ,
				       width, height, 1) ;
		mask_gc = XCreateGC( disp->display , *mask , 0 , &gcv) ;

		XSetForeground( disp->display , mask_gc , 0) ;
		XFillRectangle( disp->display , *mask , mask_gc , 0 , 0 , width , height) ;
		XSetForeground( disp->display , mask_gc , 1) ;

		line = gdk_pixbuf_get_pixels( pixbuf) ;
		rowstride = gdk_pixbuf_get_rowstride (pixbuf) ;

		for( i = 0 ; i < height ; i++)
		{
			pixel = line + 3 ;
			for( j = 0 ; j < width ; j++)
			{
				if( *pixel > 127)
				{
					XDrawPoint( disp->display , *mask , mask_gc , j , i) ;
				}
				pixel += 4 ;
			}
			line += rowstride ;
		}

		XFreeGC( disp->display , mask_gc) ;
	}
	else
	{
		/* no mask */
		*mask = None ;
	}

	return  1 ;
}

static XImage *
compose_truecolor(
	x_display_t *  disp ,
	GdkPixbuf *  pixbuf ,
	Pixmap  pixmap
	)
{
	XVisualInfo *  vinfo ;
	XImage *  image ;
	int  i , j ;
	int  width , height , rowstride ;
	u_char *  line ;
	u_char *  pixel ;
	int  r_offset , g_offset , b_offset;
	long  r_mask , g_mask , b_mask ;
	long  r , g , b ;
	long  data ;

	width = gdk_pixbuf_get_width (pixbuf) ;
	height = gdk_pixbuf_get_height (pixbuf) ;

	if( ! ( vinfo = x_display_get_visual_info( disp)))
	{
		return  NULL ;
	}

	r_mask = vinfo->red_mask ;
	g_mask = vinfo->green_mask ;
	b_mask = vinfo->blue_mask ;

	XFree( vinfo) ;

	r_offset = lsb( r_mask) ;
	g_offset = lsb( g_mask) ;
	b_offset = lsb( b_mask) ;

	if( ! ( image = XGetImage( disp->display , pixmap , 0 , 0 , width , height ,
				AllPlanes , ZPixmap)))
	{
		return  NULL ;
	}

	rowstride = gdk_pixbuf_get_rowstride( pixbuf) ;
	line = gdk_pixbuf_get_pixels( pixbuf) ;

	for( i = 0; i < height; i++)
	{
		pixel = line ;
		for( j = 0 ; j < width ; j++)
		{
			data = XGetPixel( image , j , i) ;

			r = ((data & r_mask) >>r_offset) ;
			g = ((data & g_mask) >>g_offset) ;
			b = ((data & b_mask) >>b_offset) ;

			r = (r*(256 - pixel[3]) + pixel[0] *  pixel[3])>>8 ;
			g = (g*(256 - pixel[3]) + pixel[1] *  pixel[3])>>8 ;
			b = (b*(256 - pixel[3]) + pixel[2] *  pixel[3])>>8 ;

			XPutPixel( image , j , i ,
				   (r <<r_offset) | (g <<g_offset) | (b <<b_offset)) ;
			pixel += 4 ;
		}
		line += rowstride ;
	}

	return  image ;
}

static XImage *
compose_pseudocolor(
	x_display_t *  disp ,
	GdkPixbuf *  pixbuf ,
	Pixmap  pixmap
	)
{
	XImage *  image ;
	int  i , j , num_cells ;
	int  width , height , rowstride ;
	u_int  r , g , b ;
	u_char *  line ;
	u_char *  pixel ;
	long  data ;
	XColor *  color_list ;

	if( ( num_cells = fetch_colormap( disp , &color_list)) == 0)
	{
		return  NULL ;
	}

	width = gdk_pixbuf_get_width (pixbuf) ;
	height = gdk_pixbuf_get_height (pixbuf) ;

	if( ! ( image = XGetImage( disp->display , pixmap , 0 , 0 , width , height ,
				AllPlanes, ZPixmap)))
	{
		free( color_list) ;
		return  NULL ;
	}

	rowstride = gdk_pixbuf_get_rowstride( pixbuf) ;
	line = gdk_pixbuf_get_pixels( pixbuf) ;

	for( i = 0 ; i < height ; i++)
	{
		pixel = line ;
		for( j = 0 ; j < width ; j++)
		{
			data = XGetPixel( image , j , i) ;
			r = color_list[data].red >>8 ;
			g = color_list[data].green >>8 ;
			b = color_list[data].blue >>8 ;

			r = (r*(256 - pixel[3]) + pixel[0] *  pixel[3])>>8 ;
			g = (g*(256 - pixel[3]) + pixel[1] *  pixel[3])>>8 ;
			b = (b*(256 - pixel[3]) + pixel[2] *  pixel[3])>>8 ;

			XPutPixel( image , j , i ,
				closest_color_index( color_list , num_cells , r , g , b)) ;
			pixel += 4 ;
		}
		line += rowstride ;
	}

	free( color_list) ;

	return  image ;
}

static int
compose_to_pixmap(
	x_display_t *  disp ,
	GdkPixbuf *  pixbuf ,
	Pixmap  pixmap
	)
{
	XImage *  image ;

	if( disp->visual->class == TrueColor)
	{
		image = compose_truecolor( disp , pixbuf , pixmap) ;
	}
	else
	{
		image = compose_pseudocolor( disp , pixbuf , pixmap) ;
	}

	if( ! image)
	{
		return  0 ;
	}

	XPutImage( disp->display , pixmap , disp->gc->gc , image , 0 , 0 , 0 , 0 ,
			gdk_pixbuf_get_width( pixbuf) ,
			gdk_pixbuf_get_height( pixbuf)) ;
	XDestroyImage( image) ;

	return  1 ;
}

static int
modify_image(
	GdkPixbuf *  pixbuf ,
	x_picture_modifier_t *  pic_mod		/* Mustn't be normal */
	)
{
	int  i , j ;
	int  width , height , rowstride , bytes_per_pixel ;
	u_char *  line ;
	u_char *  pixel ;
	u_char  value_table[256] ;

	value_table_refresh( value_table , pic_mod) ;

	bytes_per_pixel = (gdk_pixbuf_get_has_alpha( pixbuf)) ? 4 : 3 ;
	width = gdk_pixbuf_get_width (pixbuf) ;
	height = gdk_pixbuf_get_height (pixbuf) ;
	rowstride = gdk_pixbuf_get_rowstride (pixbuf) ;

	line = gdk_pixbuf_get_pixels( pixbuf) ;

	for( i = 0 ; i < height ; i++)
	{
		pixel = line ;
		line += rowstride ;

		for( j = 0 ; j < width ; j++)
		{
			/*
			 * XXX
			 * keeps neither hue nor saturation.
			 * MUST be replaced by another better color model(CIE Yxy? lab?)
			 */
			pixel[0] = (value_table[pixel[0]] * pic_mod->alpha +
					pic_mod->blend_red * (255 - pic_mod->alpha)) / 255 ;
			pixel[1] = (value_table[pixel[1]] * pic_mod->alpha +
					pic_mod->blend_green * (255 - pic_mod->alpha)) / 255 ;
			pixel[2] = (value_table[pixel[2]] * pic_mod->alpha +
					pic_mod->blend_blue * (255 - pic_mod->alpha)) / 255 ;
			/* alpha plane is not changed */
			pixel += bytes_per_pixel ;
		}
	}

	return  1 ;
}

#else  /* USE_EXT_IMAGELIB */

static int
load_file(
	x_display_t *  disp ,
	u_int  width ,
	u_int  height ,
	char *  path ,
	x_picture_modifier_t *  pic_mod ,
	Pixmap *  pixmap ,
	Pixmap *  mask		/* Can be NULL */
	)
{
	pid_t  pid ;
	int  fds1[2] ;
	int  fds2[2] ;
	char  pix_str[DIGIT_STR_LEN(Pixmap) + DIGIT_STR_LEN(Pixmap)] ;
	Pixmap  pixmap_tmp ;
	Pixmap  mask_tmp ;
	ssize_t  size ;

	if( pipe( fds1) == -1)
	{
		return  0 ;
	}
	if( pipe( fds2) == -1)
	{
		close( fds1[0]) ;
		close( fds1[1]) ;

		return  0 ;
	}

	pid = fork() ;
	if( pid == -1)
	{
		return  0 ;
	}

	if( pid == 0)
	{
		/* child process */

		char *  args[4] ;
		char  win_str[DIGIT_STR_LEN(Window)] ;
		char  width_str[DIGIT_STR_LEN(u_int)] ;
		char  height_str[DIGIT_STR_LEN(u_int)] ;

		args[0] = LIBEXECDIR "/mlimgloader" ;
		sprintf( win_str , "%lu" , x_display_get_group_leader( disp)) ;
		args[1] = win_str ;
		sprintf( width_str , "%u" , width) ;
		args[2] = width_str ;
		sprintf( height_str , "%u" , height) ;
		args[3] = height_str ;
		args[4] = path ;
		args[5] = NULL ;

		close( fds1[1]) ;
		close( fds2[0]) ;
		if( dup2( fds1[0] , STDIN_FILENO) != -1 && dup2( fds2[1] , STDOUT_FILENO) != -1)
		{
			execv( args[0] , args) ;
		}

	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " %s failed to start.\n" , args[0]) ;
	#endif

		exit(1) ;
	}

	close( fds1[0]) ;
	close( fds2[1]) ;

	if( ( size = read( fds2[0] , pix_str , sizeof(pix_str) - 1)) <= 0)
	{
		goto  error ;
	}

	pix_str[size] = '\0' ;

	if( sscanf( pix_str , "%lu %lu" , &pixmap_tmp , &mask_tmp) != 2)
	{
		goto  error ;
	}

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " Receiving pixmap %lu %lu\n" , pixmap_tmp , mask_tmp) ;
#endif

	if( width == 0 || height == 0)
	{
		get_drawable_size( disp->display , pixmap_tmp , &width , &height) ;
	}

	if( ( *pixmap = XCreatePixmap( disp->display , x_display_get_group_leader( disp) ,
				width , height , disp->depth)) == None)
	{
		goto  error ;
	}

	if( ! x_picture_modifier_is_normal( pic_mod))
	{
		modify_pixmap( disp , pixmap_tmp , *pixmap , pic_mod) ;
	}
	else
	{
		XCopyArea( disp->display , pixmap_tmp , *pixmap , disp->gc->gc ,
			0 , 0 , width , height , 0 , 0) ;
	}

	if( mask)
	{
		if( mask_tmp &&
		    ( *mask = XCreatePixmap( disp->display ,
				x_display_get_group_leader( disp) , width , height , 1)))
		{
			GC  mask_gc ;
			XGCValues  gcv ;

			mask_gc = XCreateGC( disp->display , *mask , 0 , &gcv) ;
			XCopyArea( disp->display , mask_tmp , *mask , mask_gc ,
				0 , 0 , width , height , 0 , 0) ;

			XFreeGC( disp->display , mask_gc) ;
		}
		else
		{
			*mask = None ;
		}
	}

	XSync( disp->display , False) ;

	close( fds2[0]) ;
	close( fds1[1]) ; /* child process exited by this. pixmap_tmp is alive until here. */

	return  1 ;

error:
	close( fds2[0]) ;
	close( fds1[1]) ;

	return  0 ;
}

/* create an CARDINAL array for_NET_WM_ICON data */
static int
create_cardinals_from_image(
	u_int32_t **  cardinal ,
	u_int  width ,
	u_int  height ,
	XImage *  image
	)
{
	int i, j ;

	if( width > ((SIZE_MAX / 4) -2) / height)
	{
		return  0 ; /* integer overflow */
	}

	if( ( *cardinal = malloc( ((size_t)width * height + 2) *4)) == NULL)
	{
		return  0 ;
	}

	/* format of the array is {width, height, ARGB[][]} */
	(*cardinal)[0] = width ;
	(*cardinal)[1] = height ;
	for( i = 0 ; i < height ; i++)
	{
		for( j = 0 ; j < width ; j++)
		{
			/*
			 * ARGB - all pixels are completely opaque (0xFF)
			 *
			 * XXX see how to process pixel in compose_truecolor/compose_pseudocolor.
			 */
			(*cardinal)[(i*width+j)+2] = 0xff000000 + XGetPixel( image , j , i) ;
		}
	}

	return  1 ;
}

#endif	/* USE_EXT_IMAGELIB */


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

	if( display_count == 0)
	{
	#ifdef  USE_EXT_IMAGELIB
		/* drop pixbuf cache */
		load_file( NULL , 0 , 0 , 0) ;
	#endif
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
#ifdef  USE_EXT_IMAGELIB
	GdkPixbuf *  pixbuf ;
#endif
	Pixmap pixmap ;

	if( ! file_path || ! *file_path)
	{
		return  None ;
	}

	if( strncmp( file_path , "pixmap:" , K_MIN(strlen(file_path),7)) == 0 &&
		sscanf( file_path + 7 , "%lu" , &pixmap) == 1)
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " pixmap:%lu is used.\n" , pixmap) ;
	#endif

		return  pixmap ;
	}

#ifdef  USE_EXT_IMAGELIB

	if( ! ( pixbuf = load_file( file_path , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
				   GDK_INTERP_BILINEAR)))
	{
		return  None ;
	}

	if( ! x_picture_modifier_is_normal( pic_mod))
	{
		/* pixbuf which load_file() returned is cached, so don't modify it. */
		GdkPixbuf *  p ;

		p = gdk_pixbuf_copy( pixbuf) ;
		g_object_unref( pixbuf) ;

		if( ( pixbuf = p) == NULL)
		{
			return  None ;
		}
		
		if( ! modify_image( pixbuf , pic_mod))
		{
			g_object_unref( pixbuf) ;

			return  None ;
		}
	}

	if( gdk_pixbuf_get_has_alpha( pixbuf) &&
		(pixmap = x_imagelib_get_transparent_background( win , NULL)))
	{
		if( ! compose_to_pixmap( win->disp , pixbuf , pixmap))
		{
			goto  error ;
		}
	}
	else
	{
		pixmap = XCreatePixmap( win->disp->display , win->my_window ,
					ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
					win->disp->depth) ;
		
		if( ! pixbuf_to_pixmap( win->disp, pixbuf, pixmap))
		{
			goto  error ;
		}
	}

	g_object_unref( pixbuf) ;

	return  pixmap ;
	
error:
	XFreePixmap( win->disp->display , pixmap) ;
	g_object_unref( pixbuf) ;

	return  None ;

#else	/* USE_EXT_IMAGELIB */

	if( load_file( win->disp , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
				file_path , pic_mod , &pixmap , NULL))
	{
		return  pixmap ;
	}
	else
	{
		return  None ;
	}

#endif	/* USE_EXT_IMAGELIB */
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
	{
		return  1 ;
	}
	else
	{
		return  0 ;
	}
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
	Pixmap  root ;
	Pixmap  pixmap ;
	u_int  root_width ;
	u_int  root_height ;

	if( ( root = root_pixmap( win->disp->display)) == None)
	{
		return  None ;
	}
	
	if( ! x_window_get_visible_geometry( win , &x , &y , &pix_x , &pix_y , &width , &height))
	{
		return  None ;
	}
	
	/* The pixmap to be returned */
	if( ( pixmap = XCreatePixmap( win->disp->display , win->my_window ,
				ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
				win->disp->depth)) == None)
	{
		return  None ;
	}

	get_drawable_size( win->disp->display , root , &root_width , &root_height) ;

	if ( root_width < DisplayWidth( win->disp->display , win->disp->screen) ||
	     root_height < DisplayHeight( win->disp->display , win->disp->screen))
	{
		GC  gc ;

		gc = XCreateGC( win->disp->display , win->my_window , 0 , NULL) ;
		
		x %= root_width ;
		y %= root_height ;
		
		/* Some WM (WindowMaker etc) need tiling... sigh.*/
		XSetTile( win->disp->display , gc , root) ;
		XSetTSOrigin( win->disp->display , gc , -x , -y) ;
		XSetFillStyle( win->disp->display , gc , FillTiled) ;
		/* XXX not correct with virtual desktop?  */
		XFillRectangle( win->disp->display , pixmap , gc ,
			pix_x , pix_y , width , height) ;
		
		XFreeGC( win->disp->display, gc) ;
	}
	else
	{
		XCopyArea( win->disp->display , root , pixmap , win->gc->gc ,
				x , y , width , height , pix_x , pix_y) ;
	}

	if( ! x_picture_modifier_is_normal( pic_mod))
	{
		if( ! modify_pixmap( win->disp , pixmap , pixmap , pic_mod))
		{
			XFreePixmap( win->disp->display , pixmap) ;

			return  None ;
		}
	}

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
int
x_imagelib_load_file(
	x_display_t *  disp,
	char *  path,
	u_int32_t **  cardinal,
	Pixmap *  pixmap,
	Pixmap *  mask,
	u_int *  width,
	u_int *  height
	)
{
	u_int  dst_height, dst_width ;
#ifdef  USE_EXT_IMAGELIB
	GdkPixbuf *  pixbuf ;
#endif

	if( ! width)
	{
		dst_width = 0 ;
	}
	else
	{
		dst_width = *width ;
	}
	if( ! height)
	{
		dst_height = 0 ;
	}
	else
	{
		dst_height = *height ;
	}

#ifdef  USE_EXT_IMAGELIB

	if( path)
	{
		/* create a pixbuf from the file and create a cardinal array */
		if( !( pixbuf = load_file( path , dst_width , dst_height , GDK_INTERP_BILINEAR)))
		{
		#ifdef DEBUG
			kik_warn_printf(KIK_DEBUG_TAG "couldn't load pixbuf\n") ;
		#endif
			return  0 ;
		}

		if( cardinal)
		{
			create_cardinals_from_pixbuf( cardinal , dst_width , dst_height , pixbuf) ;
		}
	}
	else
	{
		if( !cardinal || !(*cardinal))
		{
			return  0 ;
		}

		/* create a pixbuf from the cardinal array */
		if( ! ( pixbuf = create_pixbuf_from_cardinals( *cardinal ,
					dst_width , dst_height)))
		{
			return  0 ;
		}
	}

	dst_width = gdk_pixbuf_get_width( pixbuf) ;
	dst_height = gdk_pixbuf_get_height( pixbuf) ;

	/*
	 * Create the Icon pixmap & mask to be used in WMHints.
	 * Note that none as a result is acceptable.
	 * Pixmaps can't be cached since the last pixmap may be freed by someone...
	 */

	if( pixmap)
	{
		/*
		 * DefaultRootWindow should not be used because depth and visual
		 * of DefaultRootWindow don't always match those of mlterm window.
		 * Use x_display_get_group_leader instead.
		 */
		*pixmap = XCreatePixmap( disp->display , x_display_get_group_leader( disp) ,
					 dst_width , dst_height , disp->depth) ;
		if( mask)
		{
			if( ! pixbuf_to_pixmap_and_mask( disp , pixbuf , pixmap , mask))
			{
				g_object_unref( pixbuf) ;
				XFreePixmap( disp->display, *pixmap) ;
				*pixmap = None ;
				XFreePixmap( disp->display, *mask) ;
				*mask = None ;

				return  0 ;
			}

		}
		else
		{
			if( ! pixbuf_to_pixmap( disp , pixbuf , *pixmap))
			{
				g_object_unref( pixbuf) ;
				XFreePixmap( disp->display , *pixmap) ;
				*pixmap = None ;

				return  0 ;
			}
		}
	}

	g_object_unref( pixbuf) ;

#else	/* USE_EXT_IMAGELIB */

	if( ! load_file( disp , dst_width , dst_height , path , NULL , pixmap , mask))
	{
		return  0 ;
	}

	/* XXX Duplicated in load_file */
	if( dst_width == 0 || dst_height == 0)
	{
		get_drawable_size( disp->display , *pixmap , &dst_width , &dst_height) ;
	}

	if( cardinal)
	{
		XImage *  image ;

		image = XGetImage( disp->display , *pixmap ,
				0 , 0 , dst_width , dst_height , AllPlanes , ZPixmap) ;

		if( ! create_cardinals_from_image( cardinal , dst_width , dst_height , image))
		{
			XDestroyImage( image) ;
			XFreePixmap( disp->display , *pixmap) ;

			return  0 ;
		}

		XDestroyImage( image) ;
	}

#endif	/* USE_EXT_IMAGELIB */

	if( width && *width == 0)
	{
		*width = dst_width ;
	}

	if( height && *height == 0)
	{
		*height = dst_height ;
	}

	return  1 ;
}

Pixmap
x_imagelib_pixbuf_to_pixmap(
	x_window_t *  win ,
	x_picture_modifier_t *  pic_mod ,
	GdkPixbufPtr  pixbuf
	)
{
#ifdef  USE_EXT_IMAGELIB
	Pixmap  pixmap ;
	GdkPixbuf *  target ;

	if( ! x_picture_modifier_is_normal( pic_mod))
	{
		if( ( target = gdk_pixbuf_copy( pixbuf)) == NULL)
		{
			return  None ;
		}

		modify_image( target , pic_mod) ;
	}
	else
	{
		target = pixbuf ;
	}
	
	pixmap = XCreatePixmap( win->disp->display , win->my_window ,
				ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
				win->disp->depth) ;
	
	if( pixbuf_to_pixmap( win->disp , target , pixmap))
	{
		return  pixmap ;
	}

	if( target != pixbuf)
	{
		g_object_unref( target) ;
	}
	
	XFreePixmap( win->disp->display, pixmap) ;
#endif

	return  None ;
}

int
x_delete_image(
	Display *  display ,
	Pixmap  pixmap
	)
{
	XFreePixmap( display , pixmap) ;

	return  1 ;
}
