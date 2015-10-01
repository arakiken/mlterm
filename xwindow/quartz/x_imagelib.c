/*
 *	$Id$
 */

#ifndef  NO_IMAGE

#include  <ApplicationServices/ApplicationServices.h>

#include  "../x_imagelib.h"

#include  <stdio.h>		/* sprintf */
#include  <math.h>		/* pow */

#include  <kiklib/kik_util.h>	/* DIGIT_STR_LEN */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>

#include  "cocoa.h"


#if  1
#define  BUILTIN_SIXEL
#endif


/* --- static functions --- */

#define  CARD_HEAD_SIZE  0
#include  "../../common/c_sixel.c"
#include  "../../common/c_regis.c"

static void
value_table_refresh(
	u_char *  value_table ,		/* 256 bytes */
	x_picture_modifier_t *  mod
	)
{
	int i , tmp ;
	double real_gamma , real_brightness , real_contrast ;

	real_gamma = (double)(mod->gamma) / 100 ;
	real_contrast = (double)(mod->contrast) / 100 ;
	real_brightness = (double)(mod->brightness) / 100 ;

	for( i = 0 ; i < 256 ; i++)
	{
		tmp = real_contrast * (255 * pow(((double)i + 0.5)/ 255, real_gamma) -128)
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
adjust_pixmap(
	u_char *  image ,
	u_int  width ,
	u_int  height ,
	x_picture_modifier_t *  pic_mod
	)
{
	u_char *  value_table ;
	u_int  y ;
	u_int  x ;
	u_char  r , g , b , a ;
	u_int32_t  pixel ;

	if( ! x_picture_modifier_is_normal( pic_mod) &&
	    ( value_table = alloca( 256)))
	{
		value_table_refresh( value_table , pic_mod) ;
	}
	else
	{
		return ;
	}

	for( y = 0 ; y < height ; y++)
	{
		for( x = 0 ; x < width ; x++)
		{
			pixel = *(((u_int32_t*)image) + (y * width + x)) ;

			a = (pixel >> 24) & 0xff ;
			r = (pixel >> 16) & 0xff ;
			g = (pixel >> 8) & 0xff ;
			b = pixel & 0xff ;

			r = (value_table[r] * (255 - pic_mod->alpha) +
				pic_mod->blend_red * pic_mod->alpha) / 255 ;
			g = (value_table[g] * (255 - pic_mod->alpha) +
				pic_mod->blend_green * pic_mod->alpha) / 255 ;
			b = (value_table[b] * (255 - pic_mod->alpha) +
				pic_mod->blend_blue * pic_mod->alpha) / 255 ;

			pixel = (a << 24) | (r << 16) | (g << 8) | b ;
			*(((u_int32_t*)image) + (y * width + x)) = pixel ;
		}
	}
}

static int
check_has_alpha(
	u_char *  image ,
	u_int  width ,
	u_int  height
	)
{
	u_int  x ;
	u_int  y ;
	u_char *  p = image + 3 ;

	for( y = 0 ; y < height ; y++)
	{
		for( x = 0 ; x < width ; x++ , p += 4)
		{
			if( *p != 0xff)
			{
				return  1 ;
			}
		}
	}

	return  0 ;
}

static int
load_file(
	char *  path ,	/* must be UTF-8 */
	u_int *  width ,
	u_int *  height ,
	x_picture_modifier_t *  pic_mod ,
	Pixmap *  pixmap ,
	PixmapMask *  mask
	)
{
	char *  suffix ;
	u_char *  image ;
	CGImageAlphaInfo  info ;

	suffix = path + strlen(path) - 4 ;
#ifdef  BUILTIN_SIXEL
	if( strcasecmp( suffix , ".six") == 0 &&
	    *width == 0 && *height == 0 &&
	    ( image = load_sixel_from_file( path , width , height)))
	{
		adjust_pixmap( image , *width , *height , pic_mod) ;

		info = check_has_alpha( image , *width , *height) ?
				kCGImageAlphaPremultipliedLast : kCGImageAlphaNoneSkipLast ;

		CGColorSpaceRef  cs = CGColorSpaceCreateDeviceRGB() ;
		CGContextRef ctx = CGBitmapContextCreate( image , *width , *height ,
					8 , *width * 4 , cs , info) ;
		CGColorSpaceRelease( cs) ;

		*pixmap = CGBitmapContextCreateImage( ctx) ;
	}
	else
#endif
	{
		if( strcasecmp( suffix , ".rgs") == 0)
		{
			convert_regis_to_bmp( path) ;
		}

		/* XXX pic_mod is ignored */
		*pixmap = cocoa_load_image( path , width , height) ;

		info = CGImageGetAlphaInfo( *pixmap) ;
	}

	if( info == kCGImageAlphaPremultipliedLast ||
	    info == kCGImageAlphaPremultipliedFirst ||
	    info == kCGImageAlphaLast ||
	    info == kCGImageAlphaFirst)
	{
		*mask = 1L ;	/* dummy */
	}

	return  1 ;
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
#if  0
	u_int  width ;
	u_int  height ;
	HBITMAP  hbmp ;
	HBITMAP  hbmp_w ;
	HDC  hdc ;
	HDC  hmdc_tmp ;
	HDC  hmdc ;

	width = height = 0 ;
	if( ! load_file( path , &width , &height , pic_mod , &hbmp , NULL))
	{
		BITMAP  bmp ;
	#if  defined(__CYGWIN__) || defined(__MSYS__)
		/* MAX_PATH which is 260 (3+255+1+1) is defined in win32 alone. */
		char  winpath[MAX_PATH] ;
		cygwin_conv_to_win32_path( path , winpath) ;
		path = winpath ;
	#endif

		if( ! ( hbmp = LoadImage( 0 , path , IMAGE_BITMAP , 0 , 0 , LR_LOADFROMFILE)))
		{
			return  None ;
		}

		GetObject( hbmp , sizeof(BITMAP) , &bmp) ;
		width = bmp.bmWidth ;
		height = bmp.bmHeight ;
	}

	hdc = GetDC( win->my_window) ;

	hmdc_tmp = CreateCompatibleDC( hdc) ;
	SelectObject( hmdc_tmp , hbmp) ;

	hbmp_w = CreateCompatibleBitmap( hdc , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win)) ;
	hmdc = CreateCompatibleDC( hdc) ;
	SelectObject( hmdc , hbmp_w) ;

	ReleaseDC( win->my_window , hdc) ;

	SetStretchBltMode( hmdc , COLORONCOLOR) ;
	StretchBlt( hmdc , 0 , 0 , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
		hmdc_tmp , 0 , 0 , width , height , SRCCOPY) ;

	DeleteDC( hmdc_tmp) ;
	DeleteObject( hbmp) ;

	return  hmdc ;
#else
	return  None ;
#endif
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
	u_int32_t **  cardinal ,
	Pixmap *  pixmap ,
	PixmapMask *  mask ,
	u_int *  width ,
	u_int *  height
	)
{
	if( cardinal)
	{
		return  0 ;
	}

	if( ! load_file( path , width , height , NULL , pixmap , mask))
	{
		return  0 ;
	}

	return  1 ;
}

int
x_delete_image(
	Display *  display ,
	Pixmap  pixmap
	)
{
	CGImageRelease( pixmap) ;

	return  1 ;
}

int
x_delete_mask(
	Display *  display ,
	PixmapMask  mask	/* can be NULL */
	)
{
	return  1 ;
}

#endif	/* NO_IMAGE */
