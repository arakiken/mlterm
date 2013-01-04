/*
 *	$Id$
 */

#include  "../x_imagelib.h"

#include  <stdio.h>		/* sprintf */
#include  <math.h>               /* pow */

#include  <kiklib/kik_util.h>	/* DIGIT_STR_LEN */
#include  <kiklib/kik_debug.h>


/* --- static functions --- */

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
load_file(
	char *  path ,
	u_int *  width ,
	u_int *  height ,
	x_picture_modifier_t *  pic_mod ,
	HBITMAP *  hbmp ,
	HBITMAP *  hbmp_mask
	)
{
	char *  cmd_line ;
	SECURITY_ATTRIBUTES  sa ;
	HANDLE  output_write ;
	HANDLE  output_read ;
	PROCESS_INFORMATION  pi ;
	STARTUPINFO  si ;
	u_int32_t  tmp ;
	DWORD  n_rd ;
	DWORD  size ;
	BITMAPINFOHEADER  header ;
	HDC  hdc ;
	BYTE *  image ;

#define  CMD_LINE_FMT  "mlimgloader.exe 0 %u %u \"%s\" -c"

	if( ! ( cmd_line = alloca( sizeof( CMD_LINE_FMT) + DIGIT_STR_LEN(int) * 2 +
				strlen( path))))
	{
		return  0 ;
	}

	sprintf( cmd_line , CMD_LINE_FMT , *width , *height , path) ;

	/* Set up the security attributes struct. */
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = TRUE;

	/* Create the child output pipe. */
	if( ! CreatePipe( &output_read , &output_write , &sa , 0))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " CreatePipe() failed.\n") ;
	#endif

		return  0 ;
	}

	ZeroMemory(&si,sizeof(STARTUPINFO)) ;
	si.cb = sizeof(STARTUPINFO) ;
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_FORCEOFFFEEDBACK ;
	si.hStdOutput = output_write ;
	si.hStdInput = GetStdHandle(STD_INPUT_HANDLE) ;
	si.hStdError = GetStdHandle(STD_ERROR_HANDLE) ;

	if( ! CreateProcess( "mlimgloader.exe" , cmd_line ,
		NULL , NULL , TRUE , CREATE_NO_WINDOW , NULL, NULL, &si, &pi))
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " CreateProcess() failed.\n") ;
	#endif

		CloseHandle( output_write) ;

		goto  error ;
	}

	CloseHandle( output_write) ;
	CloseHandle( pi.hProcess) ;
	CloseHandle( pi.hThread) ;

	if( ! ReadFile( output_read , &tmp , sizeof(u_int32_t) , &n_rd , NULL) ||
		n_rd != sizeof(u_int32_t))
	{
		goto  error ;
	}

	size = (*width = tmp) * sizeof(u_int32_t) ;

	if( ! ReadFile( output_read , &tmp , sizeof(u_int32_t) , &n_rd , NULL) ||
		n_rd != sizeof(u_int32_t))
	{
		goto  error ;
	}

	size *= (*height = tmp) ;

	if( ! ( image = malloc( size)))
	{
		goto  error ;
	}
	else
	{
		BYTE *  p ;

		p = image ;
		while( ReadFile( output_read , p , size , &n_rd , NULL) && n_rd > 0)
		{
			p += n_rd ;
			size -= n_rd ;
		}

		if( size > 0)
		{
			free( image) ;

			goto  error ;
		}
	}

	CloseHandle( output_read) ;

	adjust_pixmap( image , *width , *height , pic_mod) ;

	if( hbmp_mask)
	{
		int  x ;
		int  y ;
		u_int  data_width ;
		BYTE *  mask_data ;
		BYTE *  dst ;

		*hbmp_mask = None ;

		/* align each line by short. */
		data_width = ((*width) + 15) / 16 * 2 ;

		if( ( dst = mask_data = calloc( data_width * (*height) , 1)))
		{
			int  has_tp ;
			u_int32_t *  src ;

			has_tp = 0 ;
			src = (u_int32_t*)image ;

			for( y = 0 ; y < *height ; y++)
			{
				for( x = 0 ; x < *width ; x++)
				{
					if( *src >= 0x80000000)
					{
						dst[x / 8] |= (1 << (7 - x % 8)) ;
					}
					else
					{
						has_tp = 1 ;
					}

					src ++ ;
				}

				dst += data_width ;
			}

			if( has_tp)
			{
				*hbmp_mask = CreateBitmap( *width , *height , 1 , 1 , mask_data) ;
			}

			free( mask_data) ;
		}
	}

	header.biSize = sizeof(BITMAPINFOHEADER) ;
	header.biWidth = *width ;
	header.biHeight = -(*height) ;
	header.biPlanes = 1 ;
	header.biBitCount = 32 ;
	header.biCompression = BI_RGB ;
	header.biSizeImage = (*width) * (*height) * 4 ;
	header.biXPelsPerMeter = 0 ;
	header.biYPelsPerMeter = 0 ;
	header.biClrUsed = 0 ;
	header.biClrImportant = 0 ;

	hdc = GetDC(NULL) ;
	*hbmp = CreateDIBitmap( hdc , &header , CBM_INIT , image ,
		(BITMAPINFO*)&header , DIB_RGB_COLORS) ;
	ReleaseDC( NULL , hdc) ;

	free( image) ;

	return  1 ;

error:
	CloseHandle( output_read) ;

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
	HBITMAP  hbmp ;
	HDC  hdc ;
	HDC  hmdc ;

	if( cardinal)
	{
		return  0 ;
	}

	if( ! load_file( path , width , height , NULL , &hbmp , mask))
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
			return  0 ;
		}

		GetObject( hbmp , sizeof(BITMAP) , &bmp) ;
		*width = bmp.bmWidth ;
		*height = bmp.bmHeight ;
	}

	hdc = GetDC( NULL) ;

	hmdc = CreateCompatibleDC( hdc) ;
	SelectObject( hmdc , hbmp) ;

	ReleaseDC( NULL , hdc) ;
	DeleteObject( hbmp) ;

	*pixmap = hmdc ;

	return  1 ;
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

int
x_delete_mask(
	Display *  display ,
	PixmapMask  mask	/* can be NULL */
	)
{
	if( mask)
	{
		DeleteObject( mask) ;
	}

	return  1 ;
}
