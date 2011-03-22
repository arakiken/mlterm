/*
 *	$Id: ccheader,v 1.2 2001/12/01 23:37:26 ken Exp $
 */

#include <stdio.h>
#include <unistd.h>
#include <X11/Xutil.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <kiklib/kik_debug.h>
#include <kiklib/kik_types.h>	/* u_int32_t/u_int16_t, HAVE_STDINT_H */
#include <kiklib/kik_str.h>	/* strdup */
#include <kiklib/kik_util.h>	/* K_MIN */

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

#if  0
#define  __DEBUG
#endif


/* --- static functions --- */

/* returned cmap shuold be freed by the caller */
static int
fetch_colormap(
	Display *  display ,
	Visual *  visual ,
	Colormap  colormap ,
	XColor **  color_list
	)
{
	int  num_cells , i ;

	num_cells = visual->map_entries ;

	if( ( *color_list = calloc( sizeof(XColor), num_cells)) == NULL)
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

	XQueryColors( display , colormap , *color_list, num_cells) ;

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
	GdkInterpType  scale_type
	)
{
	GdkPixbuf *  pixbuf_tmp ;
	GdkPixbuf *  pixbuf ;

#if GDK_PIXBUF_MAJOR >= 2
	pixbuf_tmp = gdk_pixbuf_new_from_file( path , NULL) ;
#else
	pixbuf_tmp = gdk_pixbuf_new_from_file( path) ;
#endif /*GDK_PIXBUF_MAJOR*/

	if( pixbuf_tmp == NULL)
	{
		return  NULL ;
	}

	/* loading from file/cache ends here */

	if( width == 0)
	{
		width = gdk_pixbuf_get_width( pixbuf_tmp) ;
	}
	if( height == 0)
	{
		height = gdk_pixbuf_get_height( pixbuf_tmp) ;
	}

	if( ! ( pixbuf = gdk_pixbuf_scale_simple( pixbuf_tmp , width , height , scale_type)))
	{
		g_object_unref( pixbuf) ;

		return  NULL ;
	}

#ifdef __DEBUG
	kik_warn_printf( KIK_DEBUG_TAG
		"creating a scaled pixbuf(%d x %d)\n" , width , height) ;
#endif

	/* scaling ends here */

	return  pixbuf ;
}

static int
pixbuf_to_pixmap_pseudocolor(
	Display *  display ,
	Visual *  visual ,
	Colormap  colormap ,
	GC  gc ,
	GdkPixbuf *  pixbuf,
	Pixmap  pixmap
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

	if( ( num_cells = fetch_colormap( display , visual , colormap , &color_list)) == 0)
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

		XSetForeground( display , gc , closest) ;
		XDrawPoint( display , pixmap , gc , 0 , y) ;
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

			XSetForeground( display , gc , closest) ;
			XDrawPoint( display , pixmap , gc , x , y) ;

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

		XSetForeground( display , gc , closest) ;
		XDrawPoint( display , pixmap , gc , x , y) ;
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
	Display *  display ,
	Visual *  visual ,
	Colormap  colormap ,
	GC  gc ,
	u_int  depth ,
	GdkPixbuf *  pixbuf
	)
{
	XVisualInfo  vinfo_template ;
	XVisualInfo *  vinfolist ;
	int  nitem ;
	u_int  i , j ;
	u_int  width , height , rowstride , bytes_per_pixel ;
	u_char *  line ;
	u_char *  pixel ;
	long  r_mask , g_mask , b_mask ;
	int  r_offset , g_offset , b_offset ;
	XImage *  image = NULL ;

	vinfo_template.visualid = XVisualIDFromVisual( visual) ;

	if( ! ( vinfolist = XGetVisualInfo( display , VisualIDMask , &vinfo_template , &nitem)))
	{
		return  0 ;
	}

	r_mask = vinfolist[0].red_mask ;
	g_mask = vinfolist[0].green_mask ;
	b_mask = vinfolist[0].blue_mask ;

	XFree( vinfolist) ;

	r_offset = lsb( r_mask) ;
	g_offset = lsb( g_mask) ;
	b_offset = lsb( b_mask) ;

	width = gdk_pixbuf_get_width( pixbuf) ;
	height = gdk_pixbuf_get_height( pixbuf) ;

	bytes_per_pixel = (gdk_pixbuf_get_has_alpha( pixbuf)) ? 4 : 3 ;
	rowstride = gdk_pixbuf_get_rowstride( pixbuf) ;
	line = gdk_pixbuf_get_pixels( pixbuf) ;

	if( depth == 15 || depth == 16)
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

		if( ! ( image = XCreateImage( display , visual , depth , ZPixmap , 0 ,
				      (char *)data , width , height , 16 , width *  2)))
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
	else if( depth == 24 || depth == 32)
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

		if( ! ( image = XCreateImage( display, visual , depth , ZPixmap , 0 ,
				      (char *)data , width , height , 32 , width * 4)))
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
	Display *  display ,
	Visual *  visual ,
	Colormap  colormap ,
	GC  gc ,
	u_int  depth ,
	GdkPixbuf *  pixbuf ,
	Pixmap  pixmap
	)
{
	if( visual->class == TrueColor)
	{
		XImage *  image ;

		if( ( image = pixbuf_to_ximage_truecolor( display , visual ,
					colormap , gc , depth , pixbuf)))
		{
			XPutImage( display , pixmap , gc , image , 0 , 0 , 0 , 0 ,
				   gdk_pixbuf_get_width( pixbuf) ,
				   gdk_pixbuf_get_height( pixbuf)) ;
			XDestroyImage( image) ;

			return  1 ;
		}
	}
	else if( visual->class == PseudoColor)
	{
		return  pixbuf_to_pixmap_pseudocolor( display , visual , colormap , gc ,
				pixbuf , pixmap) ;
	}

	return  0 ;
}

static int
pixbuf_to_pixmap_and_mask(
	Display *  display ,
	Window  win ,
	Visual *  visual ,
	Colormap  colormap ,
	GC  gc ,
	u_int  depth ,
	GdkPixbuf *  pixbuf,
	Pixmap *  pixmap,
	Pixmap *  mask
	)
{
	u_int  width ;
	u_int  height ;

	width = gdk_pixbuf_get_width( pixbuf) ;
	height = gdk_pixbuf_get_height( pixbuf) ;

	if( ( *pixmap = XCreatePixmap( display , win , width , height , depth)) == None)
	{
		return  0 ;
	}

	if( ! pixbuf_to_pixmap( display , visual , colormap , gc , depth , pixbuf , *pixmap))
	{
		return  0 ;
	}

	if( gdk_pixbuf_get_has_alpha( pixbuf))
	{
		int  i , j ;
		int  rowstride ;
		u_char *  line ;
		u_char *  pixel ;
		GC  mask_gc ;
		XGCValues  gcv ;

		/*
		 * DefaultRootWindow should not be used because depth and visual
		 * of DefaultRootWindow don't always match those of mlterm window.
		 * Use x_display_get_group_leader instead.
		 */
		*mask = XCreatePixmap( display , win , width , height , 1) ;
		mask_gc = XCreateGC( display , *mask , 0 , &gcv) ;

		XSetForeground( display , mask_gc , 0) ;
		XFillRectangle( display , *mask , mask_gc , 0 , 0 , width , height) ;
		XSetForeground( display , mask_gc , 1) ;

		line = gdk_pixbuf_get_pixels( pixbuf) ;
		rowstride = gdk_pixbuf_get_rowstride (pixbuf) ;

		for( i = 0 ; i < height ; i++)
		{
			pixel = line + 3 ;
			for( j = 0 ; j < width ; j++)
			{
				if( *pixel > 127)
				{
					XDrawPoint( display , *mask , mask_gc , j , i) ;
				}
				pixel += 4 ;
			}
			line += rowstride ;
		}

		XFreeGC( display , mask_gc) ;
	}
	else
	{
		/* no mask */
		*mask = None ;
	}

	return  1 ;
}


/* --- global functions --- */

int
main(
	int  argc ,
	char **  argv
	)
{
	GdkPixbuf *  pixbuf ;
	Display *  display ;
	Visual *  visual ;
	Colormap  colormap ;
	u_int  depth ;
	GC  gc ;
	Pixmap  pixmap ;
	Pixmap  mask ;
	Window  win ;
	XWindowAttributes  attr ;
	u_int  width ;
	u_int  height ;
	XGCValues  gc_value ;
	char  buf[10] ;

	if( argc != 5)
	{
		return  -1 ;
	}

	display = XOpenDisplay( NULL) ;

	if( ( win = atoi( argv[1])) == 0)
	{
		win = DefaultRootWindow( display) ;
		visual = DefaultVisual( display , DefaultScreen( display)) ;
		colormap = DefaultColormap( display , DefaultScreen( display)) ;
		depth = DefaultDepth( display , DefaultScreen( display)) ;
		gc = DefaultGC( display , DefaultScreen( display)) ;
	}
	else
	{
		XGetWindowAttributes( display , win , &attr) ;
		visual = attr.visual ;
		colormap = attr.colormap ;
		depth = attr.depth ;
		gc = XCreateGC( display , win , 0 , &gc_value) ;
	}

#if GDK_PIXBUF_MAJOR >= 2
	g_type_init() ;
#endif /*GDK_PIXBUF_MAJOR*/

	width = atoi( argv[2]) ;
	height = atoi( argv[3]) ;

	/*
	 * attr.width / attr.height aren't trustworthy because this program can be
	 * called before window is actually resized.
	 */

	if( ! ( pixbuf = load_file( argv[4] , width , height , GDK_INTERP_BILINEAR)))
	{
		return  -1 ;
	}

	if( ! pixbuf_to_pixmap_and_mask( display , win , visual , colormap , gc , depth ,
			pixbuf , &pixmap , &mask))
	{
		return  -1 ;
	}

	XSync( display , False) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " Loaded pixmap %lu %lu\n" , pixmap , mask) ;
#endif

	fprintf( stdout , "%lu %lu" , pixmap , mask) ;
	fflush( stdout) ;
	close( STDOUT_FILENO) ;

	/* Wait for parent process receiving pixmap. */
	read( STDIN_FILENO , buf , sizeof(buf)) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " Exit image loader\n") ;
#endif

	return  0 ;
}
