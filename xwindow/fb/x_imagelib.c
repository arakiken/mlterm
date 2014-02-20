/*
 *	$Id$
 */

#ifndef  NO_IMAGE

#include  "../x_imagelib.h"

#include  <stdio.h>	/* sprintf */
#include  <unistd.h>	/* write , STDIN_FILENO */
#ifdef  DLOPEN_LIBM
#include <kiklib/kik_dlfcn.h>   /* dynamically loading pow */
#else
#include <math.h>               /* pow */
#endif
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>

#include  "x_display.h"		/* x_cmap_get_closest_color */


/* Trailing "/" is appended in value_table_refresh(). */
#ifndef  LIBMDIR
#define  LIBMDIR  "/lib"
#endif

#ifndef  LIBEXECDIR
#define  LIBEXECDIR  "/usr/local/libexec"
#endif


/* --- static functions --- */

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

		if( ( ! ( handle = kik_dl_open( LIBMDIR "/" , "m")) &&
		      ! ( handle = kik_dl_open( "" , "m"))) ||
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

		kik_dl_close_at_exit( handle) ;
	#else  /* DLOPEN_LIBM */
		pow_func = pow ;
	#endif /* DLOPEN_LIBM */
	}

	for( i = 0 ; i < 256 ; i++)
	{
		tmp = real_contrast * (255 * (*pow_func)(((double)i + 0.5)/ 255, real_gamma) -128)
			+ 128 * real_brightness ;
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

static void
modify_pixmap(
	Display *  display ,
	Pixmap  pixmap ,
	x_picture_modifier_t *  pic_mod ,
	u_int  depth
	)
{
	u_char *  value_table ;
	u_int32_t *  src ;
	u_char *  dst ;
	u_int  num_of_pixels ;
	u_int  count ;
	u_char  r , g , b ;
	u_long  pixel ;

	if( ! x_picture_modifier_is_normal( pic_mod) &&
	    ( value_table = alloca( 256)))
	{
		value_table_refresh( value_table , pic_mod) ;
	}
	else if( display->bytes_per_pixel == 4 &&
	         /* RRGGBB */
	         display->rgbinfo.r_offset == 16 &&
		 display->rgbinfo.g_offset == 8 &&
		 display->rgbinfo.b_offset == 0)
	{
		return ;
	}
	else
	{
		value_table = NULL ;
	}

	src = dst = pixmap->image ;
	num_of_pixels = pixmap->width * pixmap->height ;

	for( count = 0 ; count < num_of_pixels ; count++)
	{
		pixel = *(src ++) ;

		r = (pixel >> 16) & 0xff ;
		g = (pixel >> 8) & 0xff ;
		b = pixel & 0xff ;

		if( value_table)
		{
			r = (value_table[r] * (255 - pic_mod->alpha) +
				pic_mod->blend_red * pic_mod->alpha) / 255 ;
			g = (value_table[g] * (255 - pic_mod->alpha) +
				pic_mod->blend_green * pic_mod->alpha) / 255 ;
			b = (value_table[b] * (255 - pic_mod->alpha) +
				pic_mod->blend_blue * pic_mod->alpha) / 255 ;
		}

		if( x_cmap_get_closest_color( &pixel , r , g , b))
		{
		#ifdef  USE_GRF
			*((u_int16_t*)dst) = pixel ;
			dst += 2 ;
		#else
			*(dst ++) = pixel ;
		#endif
		}
		else
		{
			pixel = RGB_TO_PIXEL(r,g,b,display->rgbinfo) |
				(depth == 32 ? (pixel & 0xff000000) : 0) ;

			if( display->bytes_per_pixel == 2)
			{
				*((u_int16_t*)dst) = pixel ;
				dst += 2 ;
			}
			else /* if( display->bytes_per_pixel == 4) */
			{
				*((u_int32_t*)dst) = pixel ;
				dst += 4 ;
			}
		}
	}

	if( display->bytes_per_pixel < 4)
	{
		void *  p ;

		if( ( p = realloc( pixmap->image ,
				pixmap->width * pixmap->height * display->bytes_per_pixel)))
		{
			pixmap->image = p ;
		}
	}
}

/* For old machines and Android (not to use mlimgloader) */
#if  defined(__NetBSD__) || defined(__OpenBSD__) || defined(__ANDROID__)

#include  <string.h>
#include  <kiklib/kik_util.h>
#include  <kiklib/kik_def.h>	/* SSIZE_MAX */

/*
 * This function resizes the sixel image to the specified size and shrink
 * pixmap->image.
 * It frees pixmap->image in failure.
 * Call resize_sixel() after load_sixel_from_file() because it returns at least
 * 1024*1024 pixels memory even if the actual image size is less than 1024*1024.
 */
static int
resize_sixel(
	Pixmap  pixmap ,
	u_int  width ,
	u_int  height ,
	u_int  bytes_per_pixel
	)
{
	void *  p ;
	size_t  line_len ;
	size_t  old_line_len ;
	size_t  image_len ;
	size_t  old_image_len ;
	u_char *  dst ;
	u_char *  src ;
	int  y ;
	u_int  min_height ;

	p = NULL ;

	if( ( width == 0 || width == pixmap->width) &&
	    ( height == 0 || height == pixmap->height))
	{
		goto  end ;
	}

	if( width > SSIZE_MAX / bytes_per_pixel / height)
	{
		goto  error ;
	}

	old_line_len = pixmap->width * bytes_per_pixel ;
	line_len = width * bytes_per_pixel ;
	image_len = line_len * height ;
	old_image_len = old_line_len * pixmap->height ;

	if( image_len > old_image_len)
	{
		if( ! ( p = realloc( pixmap->image , image_len)))
		{
			goto  error ;
		}

		pixmap->image = p ;
	}

	/* Tiling */

	min_height = K_MIN(height,pixmap->height) ;

	if( width > pixmap->width)
	{
		size_t  surplus ;
		u_int  num_of_copy ;
		u_int  count ;
		u_char *  dst_next ;

		y = min_height - 1 ;
		src = pixmap->image + old_line_len * y ;
		dst = pixmap->image + line_len * y ;

		surplus = line_len % old_line_len ;
		num_of_copy = line_len / old_line_len - 1 ;

		for( ; y >= 0 ; y--)
		{
			dst_next = memmove( dst , src , old_line_len) ;

			for( count = num_of_copy ; count > 0 ; count--)
			{
				memcpy( ( dst_next += old_line_len) , dst , old_line_len) ;
			}
			memcpy( dst_next + old_line_len , dst , surplus) ;

			dst -= line_len ;
			src -= old_line_len ;
		}
	}
	else if( width < pixmap->width)
	{
		src = pixmap->image + old_line_len ;
		dst = pixmap->image + line_len ;

		for( y = 1 ; y < min_height ; y++)
		{
			memmove( dst , src , old_line_len) ;
			dst += line_len ;
			src += old_line_len ;
		}
	}

	if( height > pixmap->height)
	{
		y = pixmap->height ;
		src = pixmap->image ;
		dst = src + line_len * y ;

		for( ; y < height ; y++)
		{
			memcpy( dst , src , line_len) ;
			dst += line_len ;
			src += line_len ;
		}
	}

	kik_msg_printf( "Resize sixel from %dx%d to %dx%d\n" ,
		pixmap->width , pixmap->height , width , height) ;

	pixmap->width = width ;
	pixmap->height = height ;

end:
	/* Always realloate pixmap->image according to its width, height and bytes_per_pixel. */
	if( ! p && ( p = realloc( pixmap->image ,
				pixmap->width * pixmap->height * bytes_per_pixel)))
	{
		pixmap->image = p ;
	}

	return  1 ;

error:
	free( pixmap->image) ;

	return  0 ;
}
#endif

/* For old machines and Android (not to use mlimgloader) */
#if  defined(__NetBSD__) || defined(__OpenBSD__) || defined(__ANDROID__)

#ifndef  BUILTIN_IMAGELIB
#define  BUILTIN_IMAGELIB
#endif
#define  CARD_HEAD_SIZE  0
#include  <string.h>	/* memset/memmove */
#include  "../../common/c_imagelib.c"

#endif

/* For old machines (not to use mlimgloader) */
#if  (defined(__NetBSD__) || defined(__OpenBSD__)) && ! defined(USE_GRF)

#ifndef  BUILTIN_IMAGELIB
#define  BUILTIN_IMAGELIB
#endif
#define  SIXEL_1BPP
#include  <string.h>	/* memset/memmove */
#include  "../../common/c_imagelib.c"

/* depth should be checked by the caller. */
static int
load_sixel_with_mask_from_file_1bpp(
	char *  path ,
	u_int  width ,
	u_int  height ,
	Pixmap *  pixmap ,
	PixmapMask *  mask
	)
{
	int  x ;
	int  y ;
	u_char *  src ;
	u_char *  dst ;

	if( ! strstr( path , ".six") || ! ( *pixmap = calloc( 1 , sizeof(**pixmap))))
	{
		return  0 ;
	}

	if( ! ( src = (*pixmap)->image = load_sixel_from_file_1bpp( path ,
						&(*pixmap)->width , &(*pixmap)->height)) ||
	    /* resize_sixel() frees pixmap->image in failure. */
	    ! resize_sixel( *pixmap , width , height , 1))
	{
		free( *pixmap) ;

		return  0 ;
	}

	if( mask && ( dst = *mask = calloc( 1 , (*pixmap)->width * (*pixmap)->height)))
	{
		int  has_tp ;

		has_tp = 0 ;

		for( y = 0 ; y < (*pixmap)->height ; y++)
		{
			for( x = 0 ; x < (*pixmap)->width ; x++)
			{
				if( *src >= 0x80)
				{
					*dst = 1 ;
					/* clear opaque mark */
					*src &= 0x7f ;
				}
				else
				{
					has_tp = 1 ;
				}

				src ++ ;
				dst ++ ;
			}
		}

		if( ! has_tp)
		{
			free( *mask) ;
			*mask = None ;
		}
	}
	else
	{
		for( y = 0 ; y < (*pixmap)->height ; y++)
		{
			for( x = 0 ; x < (*pixmap)->width ; x++)
			{
				/* clear opaque mark */
				*(src ++) &= 0x7f ;
			}
		}
	}

	return  1 ;
}

#endif


static int
load_file(
	Display *  display ,
	char *  path ,
	u_int  width ,
	u_int  height ,
	x_picture_modifier_t *  pic_mod ,
	u_int  depth ,
	Pixmap *  pixmap ,
	PixmapMask *  mask
	)
{
	pid_t  pid ;
	int  fds1[2] ;
	int  fds2[2] ;
	ssize_t  size ;
	u_int32_t  tmp ;

	if( ! path || ! *path)
	{
		return  0 ;
	}

/* For old machines */
#if  (defined(__NetBSD__) || defined(__OpenBSD__)) && ! defined(USE_GRF)
	if( depth == 1)
	{
		/* pic_mod is ignored. */
		if( load_sixel_with_mask_from_file_1bpp( path , width , height , pixmap , mask))
		{
			return  1 ;
		}
	}
	else
#endif
#if defined(__NetBSD__) || defined(__OpenBSD__) || defined(__ANDROID__)
	if( strstr( path , ".six") && ( *pixmap = calloc( 1 , sizeof(**pixmap))))
	{
		if( ( (*pixmap)->image = load_sixel_from_file( path ,
						&(*pixmap)->width , &(*pixmap)->height)) &&
		    /* resize_sixel() frees pixmap->image in failure. */
		    resize_sixel( *pixmap , width , height , 4))
		{
		#if  defined(__NetBSD__) || defined(__OpenBSD__)
			x_display_set_cmap( sixel_cmap , sixel_cmap_size) ;
		#endif

			goto  loaded ;
		}
		else
		{
			free( *pixmap) ;
		}
	}
#endif

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
		close( fds1[0]) ;
		close( fds1[1]) ;
		close( fds2[0]) ;
		close( fds2[0]) ;

		return  0 ;
	}

	if( pid == 0)
	{
		/* child process */

		char *  args[7] ;
		char  width_str[DIGIT_STR_LEN(u_int) + 1] ;
		char  height_str[DIGIT_STR_LEN(u_int) + 1] ;

		args[0] = LIBEXECDIR "/mlterm/mlimgloader" ;
		args[1] = "0" ;
		sprintf( width_str , "%u" , width) ;
		args[2] = width_str ;
		sprintf( height_str , "%u" , height) ;
		args[3] = height_str ;
		args[4] = path ;
		args[5] = "-c" ;
		args[6] = NULL ;

		close( fds1[1]) ;
		close( fds2[0]) ;
		if( dup2( fds1[0] , STDIN_FILENO) != -1 && dup2( fds2[1] , STDOUT_FILENO) != -1)
		{
			execv( args[0] , args) ;
		}

		kik_msg_printf( "Failed to exec %s.\n" , args[0]) ;

		exit(1) ;
	}

	close( fds1[0]) ;
	close( fds2[1]) ;

	if( ! ( *pixmap = calloc( 1 , sizeof(**pixmap))))
	{
		goto  error ;
	}

	if( read( fds2[0] , &tmp , sizeof(u_int32_t)) != sizeof(u_int32_t))
	{
		goto  error ;
	}

	size = ((*pixmap)->width = tmp) * sizeof(u_int32_t) ;

	if( read( fds2[0] , &tmp , sizeof(u_int32_t)) != sizeof(u_int32_t))
	{
		goto  error ;
	}

	size *= ((*pixmap)->height = tmp) ;

	if( ! ( (*pixmap)->image = malloc( size)))
	{
		goto  error ;
	}
	else
	{
		u_char *  p ;
		ssize_t  n_rd ;

		p = (*pixmap)->image ;
		while( ( n_rd = read( fds2[0] , p , size)) > 0)
		{
			p += n_rd ;
			size -= n_rd ;
		}

		if( size > 0)
		{
			goto  error ;
		}
	}

	close( fds2[0]) ;
	close( fds1[1]) ;

loaded:
	if( mask)
	{
		u_char *  dst ;

		if( ( dst = *mask = calloc( 1 , (*pixmap)->width * (*pixmap)->height)))
		{
			int  x ;
			int  y ;
			int  has_tp ;
			u_int32_t *  src ;

			has_tp = 0 ;
			src = (u_int32_t*)(*pixmap)->image ;

			for( y = 0 ; y < (*pixmap)->height ; y++)
			{
				for( x = 0 ; x < (*pixmap)->width ; x++)
				{
					if( *(src ++) >= 0x80000000)
					{
						*dst = 1 ;
					}
					else
					{
						has_tp = 1 ;
					}

					dst ++ ;
				}
			}

			if( ! has_tp)
			{
				free( *mask) ;
				*mask = None ;
			}
		}
	}

	modify_pixmap( display , *pixmap , pic_mod , depth) ;

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s(w %d h %d) is loaded%s.\n" ,
			path , (*pixmap)->width , (*pixmap)->height ,
			(mask && *mask) ? " (has mask)" : "") ;
#endif

	return  1 ;

error:
#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " Failed to load %s\n" , path) ;
#endif

	if( *pixmap)
	{
		free( (*pixmap)->image) ;
		free( *pixmap) ;
	}

	close( fds2[0]) ;
	close( fds1[1]) ;

	return  0 ;
}


/* --- global functions --- */

int
x_imagelib_display_opened(
	Display *  display
	)
{
	return  1 ;
}

int
x_imagelib_display_closed(
	Display *  display
	)
{
	return  1 ;
}

Pixmap
x_imagelib_load_file_for_background(
	x_window_t *  win ,
	char *  path ,
	x_picture_modifier_t *  pic_mod
	)
{
	Pixmap  pixmap ;

#if  defined(__NetBSD__) || defined(__OpenBSD__)
	x_display_enable_to_change_cmap( 1) ;
#endif

	if( ! load_file( win->disp->display , path ,
			ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) , pic_mod , win->disp->depth ,
			&pixmap , NULL))
	{
		pixmap = None ;
	}

#if  defined(__NetBSD__) || defined(__OpenBSD__)
	x_display_enable_to_change_cmap( 0) ;
#endif

	return  pixmap ;
}

int
x_imagelib_root_pixmap_available(
	Display *   display
	)
{
	return  0 ;
}

Pixmap
x_imagelib_get_transparent_background(
	x_window_t *  win ,
	x_picture_modifier_t *  pic_mod
	)
{
	return  None ;
}

int
x_imagelib_load_file(
	x_display_t *  disp ,
	char *  path ,
	u_int32_t **  cardinal,
	Pixmap *  pixmap,
	PixmapMask *  mask,
	u_int *  width,
	u_int *  height
	)
{
	if( cardinal)
	{
		return  0 ;
	}

	if( ! load_file( disp->display , path , *width , *height ,
				NULL , disp->depth , pixmap , mask))
	{
		return  0 ;
	}

	if( *width == 0 || *height == 0)
	{
		*width = (*pixmap)->width ;
		*height = (*pixmap)->height ;
	}

	return  1 ;
}

int
x_delete_image(
	Display *  display ,
	Pixmap  pixmap
	)
{
	free( pixmap->image) ;
	free( pixmap) ;

	return  1 ;
}

int
x_delete_mask(
	Display *  display ,
	PixmapMask  mask	/* can be NULL */
	)
{
	free( mask) ;

	return  1 ;
}

#endif	/* NO_IMAGE */
