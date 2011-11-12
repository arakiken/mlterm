/*
 *	$Id$
 */

#include  "x_imagelib.h"

#include  <limits.h>		/* MAX_PATH */


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
	char *  file_path ,
	x_picture_modifier_t *  pic_mod
	)
{
	HBITMAP  hbmp ;
	HBITMAP  hbmp_w ;
	BITMAP  bmp ;
	HDC  hdc ;
	HDC  hmdc_tmp ;
	HDC  hmdc ;
#if  defined(__CYGWIN__) || defined(__MSYS__)
	char  winpath[MAX_PATH] ;
	cygwin_conv_to_win32_path( file_path , winpath) ;
	file_path = winpath ;
#endif

	hdc = GetDC( win->my_window) ;

	if( ! ( hbmp = LoadImage( 0 , file_path , IMAGE_BITMAP , 0 , 0 , LR_LOADFROMFILE)))
	{
		return  None ;
	}

	hmdc_tmp = CreateCompatibleDC( hdc) ;
	SelectObject( hmdc_tmp , hbmp) ;

	hbmp_w = CreateCompatibleBitmap( hdc , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win)) ;
	hmdc = CreateCompatibleDC( hdc) ;
	SelectObject( hmdc , hbmp_w) ;

	ReleaseDC( win->my_window , hdc) ;

	GetObject( hbmp , sizeof(BITMAP) , &bmp) ;

	SetStretchBltMode( hmdc , COLORONCOLOR) ;
	StretchBlt( hmdc , 0 , 0 , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
		hmdc_tmp , 0 , 0 , bmp.bmWidth , bmp.bmHeight , SRCCOPY) ;

	DeleteDC( hmdc_tmp) ;
	DeleteObject( hbmp) ;

	return  hmdc ;
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
	Pixmap *  mask,
	unsigned int *  width,
	unsigned int *  height
	)
{
	return 0 ;
}

int
x_delete_image(
	Display *  display ,
	Pixmap  pixmap
	)
{
	HBITMAP  bmp ;

	bmp = CreateBitmap( 1 , 1 , 1 , 1 , NULL) ;
	DeleteObject( SelectObject( pixmap , bmp)) ;
	DeleteDC( pixmap) ;
	DeleteObject( bmp) ;

	return  1 ;
}
