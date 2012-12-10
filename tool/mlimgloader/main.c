/*
 *	$Id$
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>	/* fstat */

#ifndef  USE_WIN32GUI
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <kiklib/kik_debug.h>
#include <kiklib/kik_types.h>	/* u_int32_t/u_int16_t */
#include <kiklib/kik_str.h>	/* strdup */
#include <kiklib/kik_def.h>	/* SSIZE_MAX, USE_WIN32API */

#ifdef  USE_WIN32API
#include <fcntl.h>	/* O_BINARY */
#endif


/*
 * 'data' which is malloc'ed for XCreateImage() in pixbuf_to_ximage_truecolor()
 * is free'ed in XDestroyImage().
 * If malloc is replaced kik_mem_malloc in kik_mem.h, kik_mem_free_all() will
 * free 'data' which is already free'ed in XDestroyImage() and
 * segmentation fault error can happen.
 */
#if  defined(KIK_DEBUG) && defined(malloc)
#undef malloc
#undef realloc
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

#include  "../../common/c_imagelib.c"

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

	if( ! strstr( path , ".six") || ! ( pixbuf_tmp = gdk_pixbuf_new_from_sixel( path)))
	{
	#if GDK_PIXBUF_MAJOR >= 2
	#if GDK_PIXBUF_MINOR >= 14
		if( strstr( path , "://"))
		{
			GFile *  file ;
			GFileInputStream *  in ;

			if( ( in = g_file_read(
					( file = g_vfs_get_file_for_uri(
							g_vfs_get_default() , path)) ,
					NULL , NULL)))
			{
				pixbuf_tmp = gdk_pixbuf_new_from_stream(
						(GInputStream*)in , NULL , NULL) ;
				g_object_unref( in) ;
			}
			else
			{
				pixbuf_tmp = NULL ;
			}
		}
		else
	#endif
		{
			pixbuf_tmp = gdk_pixbuf_new_from_file( path , NULL) ;
		}
	#else
		pixbuf_tmp = gdk_pixbuf_new_from_file( path) ;
	#endif /*GDK_PIXBUF_MAJOR*/
	}

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

	pixbuf = gdk_pixbuf_scale_simple( pixbuf_tmp , width , height , scale_type) ;

	g_object_unref( pixbuf_tmp) ;

#ifdef __DEBUG
	if( pixbuf)
	{
		kik_warn_printf( KIK_DEBUG_TAG
			" creating a scaled pixbuf(%d x %d)\n" , width , height) ;
	}
#endif

	/* scaling ends here */

	return  pixbuf ;
}

#ifndef  USE_WIN32GUI

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

	if( ( *color_list = calloc( num_cells , sizeof(XColor))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf(KIK_DEBUG_TAG " couldn't allocate color table\n") ;
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
	u_long  r_mask , g_mask , b_mask ;
	int  r_offset , g_offset , b_offset ;
	int  r_limit , g_limit , b_limit ;
	XImage *  image ;
	char *  data ;

	vinfo_template.visualid = XVisualIDFromVisual( visual) ;
	if( ! ( vinfolist = XGetVisualInfo( display , VisualIDMask , &vinfo_template , &nitem)))
	{
		return  NULL ;
	}

	r_mask = vinfolist[0].red_mask ;
	g_mask = vinfolist[0].green_mask ;
	b_mask = vinfolist[0].blue_mask ;

	XFree( vinfolist) ;

	r_offset = lsb( r_mask) ;
	g_offset = lsb( g_mask) ;
	b_offset = lsb( b_mask) ;

	r_limit = 8 + r_offset - msb( r_mask) ;
	g_limit = 8 + g_offset - msb( g_mask) ;
	b_limit = 8 + b_offset - msb( b_mask) ;

	width = gdk_pixbuf_get_width( pixbuf) ;
	height = gdk_pixbuf_get_height( pixbuf) ;
	/* set num of bytes per pixel of display */
	bytes_per_pixel = depth > 16 ? 4 : 2 ;

	if( width > SSIZE_MAX / bytes_per_pixel / height)
	{
		return  NULL ;	/* integer overflow */
	}

	if( ! ( data = malloc( width * height * bytes_per_pixel)))
	{
		return  NULL ;
	}

	if( ! ( image = XCreateImage( display , visual , depth , ZPixmap , 0 ,
			      data , width , height ,
			      /* in case depth isn't multiple of 8 */
			      bytes_per_pixel * 8 ,
			      width * bytes_per_pixel)))
	{
		free( data) ;

		return  NULL ;
	}

	/* set num of bytes per pixel of pixbuf */
	bytes_per_pixel = (gdk_pixbuf_get_has_alpha( pixbuf)) ? 4 : 3 ;
	rowstride = gdk_pixbuf_get_rowstride( pixbuf) ;
	line = gdk_pixbuf_get_pixels( pixbuf) ;

	for( i = 0 ; i < height ; i++)
	{
		u_char *  pixel ;

		pixel = line ;
		for( j = 0 ; j < width ; j++)
		{
			XPutPixel( image , j , i ,
				(depth == 32 ? 0xff000000 : 0) |
				(((pixel[0] >> r_limit) << r_offset) & r_mask) |
				(((pixel[1] >> g_limit) << g_offset) & g_mask) |
				(((pixel[2] >> b_limit) << b_offset) & b_mask)) ;
			pixel += bytes_per_pixel ;
		}
		line += rowstride ;
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
		else
		{
			return  0 ;
		}
	}
	else /* if( visual->class == PseudoColor) */
	{
		return  pixbuf_to_pixmap_pseudocolor( display , visual , colormap , gc ,
				pixbuf , pixmap) ;
	}
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
	Pixmap *  pixmap,	/* Created in this function. */
	Pixmap *  mask		/* Created in this function. */
	)
{
	u_int  width ;
	u_int  height ;

	width = gdk_pixbuf_get_width( pixbuf) ;
	height = gdk_pixbuf_get_height( pixbuf) ;

	*pixmap = XCreatePixmap( display , win , width , height , depth) ;

	if( ! pixbuf_to_pixmap( display , visual , colormap , gc , depth , pixbuf , *pixmap))
	{
		XFreePixmap( display , *pixmap) ;

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

#endif	/* USE_WIN32GUI */


/* --- global functions --- */

int
main(
	int  argc ,
	char **  argv
	)
{
#ifndef  USE_WIN32GUI
	Display *  display ;
	Visual *  visual ;
	Colormap  colormap ;
	u_int  depth ;
	GC  gc ;
	Pixmap  pixmap ;
	Pixmap  mask ;
	Window  win ;
	XWindowAttributes  attr ;
#endif
	GdkPixbuf *  pixbuf ;
	u_int  width ;
	u_int  height ;

#if  0
	kik_set_msg_log_file_name( "mlterm/msg.log") ;
#endif

#ifndef  USE_WIN32GUI
	if( argc == 5)
	{
		if( ! ( display = XOpenDisplay( NULL)))
		{
			return  -1 ;
		}

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
			XGCValues  gc_value ;

			XGetWindowAttributes( display , win , &attr) ;
			visual = attr.visual ;
			colormap = attr.colormap ;
			depth = attr.depth ;
			gc = XCreateGC( display , win , 0 , &gc_value) ;
		}
	}
	else
#endif
	if( argc != 6 || strcmp( argv[5] , "-c") != 0)
	{
		return  -1 ;
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
	#if  defined(__CYGWIN__) || defined(__MSYS__)
	#define  MAX_PATH  260	/* 3+255+1+1 */
		char  winpath[MAX_PATH] ;
		cygwin_conv_to_win32_path( argv[4] , winpath) ;

		if( ! ( pixbuf = load_file( winpath , width , height , GDK_INTERP_BILINEAR)))
	#endif
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Failed to load %s\n" , argv[4]) ;
		#endif

			return  -1 ;
		}
	}

	if( argc == 6)
	{
		u_char *  cardinal ;
		ssize_t  size ;

		if( width == 0 || height == 0)
		{
			width = gdk_pixbuf_get_width( pixbuf) ;
			height = gdk_pixbuf_get_height( pixbuf) ;
		}

		if( ! ( cardinal = (u_char*)create_cardinals_from_pixbuf(
					pixbuf , width , height)))
		{
			return  -1 ;
		}

		size = sizeof(u_int32_t) * (width * height + 2) ;

	#ifdef  USE_WIN32API
		setmode( STDOUT_FILENO , O_BINARY) ;
	#endif

		while( size > 0)
		{
			ssize_t  n_wr ;

			if( ( n_wr = write( STDOUT_FILENO , cardinal , size)) < 0)
			{
				return  -1 ;
			}

			cardinal += n_wr ;
			size -= n_wr ;
		}
	}
#ifndef  USE_WIN32GUI
	else
	{
		char  buf[10] ;

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
	}
#endif	/* USE_WIN32GUI */

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " Exit image loader\n") ;
#endif

	return  0 ;
}
