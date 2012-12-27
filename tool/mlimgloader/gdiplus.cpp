/*
 *	$Id$
 */

#include  <stdio.h>
#include  <unistd.h>	/* STDOUT_FILENO */
#include  <stdlib.h>	/* mbstowcs_s */

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_types.h>	/* u_int32_t/u_int16_t */
#include  <kiklib/kik_def.h>	/* SSIZE_MAX, USE_WIN32API */

#ifdef  USE_WIN32API
#include  <fcntl.h>	/* O_BINARY */
#endif

#if  defined(__CYGWIN__) || defined(__MSYS__)
#include  <sys/cygwin.h>
#endif

#include  <windows.h>
#include  <gdiplus.h>

using namespace Gdiplus ;

#if  0
#define  __DEBUG
#endif


/* --- static functions --- */

#include  "../../common/c_imagelib.c"

static u_int32_t *
create_cardinals_from_file(
	const char *  path ,
	u_int  width ,
	u_int  height
	)
{
	wchar_t  wpath[MAX_PATH] ;
	size_t  wpath_len ;
	Gdiplus::GdiplusStartupInput  startup ;
	ULONG_PTR  token ;
	IBindCtx *  ctx ;
	IMoniker *  moniker ;
	IStream *  stream ;
	Gdiplus::Bitmap *  bitmap ;
	u_int32_t *  cardinal ;
	u_int32_t *  p ;

	if( strstr( path , ".six") && ( cardinal = create_cardinals_from_sixel( path)))
	{
		return  cardinal ;
	}

	if( ((ssize_t)( wpath_len = mbstowcs( wpath , path , MAX_PATH))) <= 0 ||
	    Gdiplus::GdiplusStartup( &token , &startup , NULL) != Gdiplus::Ok)
	{
		return  NULL ;
	}

	stream = NULL ;

	if( strstr( path , "://"))
	{
		typedef HRESULT (*func)(IMoniker * , LPCWSTR , IMoniker ** , DWORD) ;
		func  create_url_moniker ;
		HMODULE  module ;

		SetDllDirectory( "") ;
		if( ( module = LoadLibrary( "urlmon")) &&
		    ( create_url_moniker = (func)GetProcAddress( module ,
							"CreateURLMonikerEx")))
		{
			if( (*create_url_moniker)( NULL , wpath , &moniker ,
					URL_MK_UNIFORM) == S_OK)
			{
				if( CreateBindCtx( 0 , &ctx) == S_OK)
				{
					if( moniker->BindToStorage( ctx , NULL ,
						IID_PPV_ARGS(&stream)) != S_OK)
					{
						ctx->Release() ;
						moniker->Release() ;
					}
				}
				else
				{
					moniker->Release() ;
				}
			}
		}
	}

	if( width == 0 || height == 0)
	{
		if( stream ?
		    ! ( bitmap = Gdiplus::Bitmap::FromStream( stream)) :
		    ! ( bitmap = Gdiplus::Bitmap::FromFile( wpath)) )
		{
			goto  error1 ;
		}
	}
	else
	{
		Image *  image ;
		Graphics *  graphics ;

		if( stream ?
		    ! ( image = Image::FromStream( stream)) :
		    ! ( image = Image::FromFile( wpath)) )
		{
			goto  error1 ;
		}

		if( ! ( bitmap = new Bitmap( width , height)))
		{
			delete  image ;

			goto  error1 ;
		}

		if( ! ( graphics = Graphics::FromImage( bitmap)))
		{
			delete  image ;
			delete  bitmap ;

			goto  error1 ;
		}

		Gdiplus::Rect  rect( 0 , 0 , width , height) ;

		graphics->DrawImage( image , rect , 0 , 0 ,
			image->GetWidth() , image->GetHeight() , UnitPixel) ;

		delete  image ;
		delete  graphics ;
	}

	if( bitmap->GetLastStatus() != Gdiplus::Ok)
	{
		goto  error2 ;
	}

	if( stream)
	{
		stream->Release() ;
		ctx->Release() ;
		moniker->Release() ;
	}

	width = bitmap->GetWidth() ;
	height = bitmap->GetHeight() ;

	if( width > ((SSIZE_MAX / sizeof(*cardinal)) - 2) / height ||	/* integer overflow */
	   ! ( p = cardinal = (u_int32_t*)malloc( (width * height + 2) * sizeof(*cardinal))))
	{
		goto  error2 ;
	}

	*(p++) = width ;
	*(p++) = height ;

	for( int  y = 0 ; y < height ; y++)
	{
		for( int  x = 0 ; x < width ; x++)
		{
			Gdiplus::Color  pixel ;

			bitmap->GetPixel( x , y , &pixel) ;

			*(p++) = (pixel.GetA() << 24) | (pixel.GetR() << 16) |
					(pixel.GetG() << 8) | pixel.GetB() ;
		}
	}

	Gdiplus::GdiplusShutdown( token) ;

	return  cardinal ;

error2:
	delete  bitmap ;

error1:
	Gdiplus::GdiplusShutdown( token) ;

	return  NULL ;
}


/* --- global functions --- */

int
main(
	int  argc ,
	char **  argv
	)
{
	u_char *  cardinal ;
	u_int  width ;
	u_int  height ;
	ssize_t  size ;

#if  0
	kik_set_msg_log_file_name( "mlterm/msg.log") ;
#endif

	if( argc != 6 || strcmp( argv[5] , "-c") != 0)
	{
		return  -1 ;
	}

	width = atoi( argv[2]) ;
	height = atoi( argv[3]) ;

	/*
	 * attr.width / attr.height aren't trustworthy because this program can be
	 * called before window is actually resized.
	 */

	if( ! ( cardinal = (u_char*)create_cardinals_from_file( argv[4] , width , height)))
	{
	#if  defined(__CYGWIN__) || defined(__MSYS__)
		char  winpath[MAX_PATH] ;
		cygwin_conv_to_win32_path( argv[4] , winpath) ;

		if( ! ( cardinal = (u_char*)create_cardinals_from_file( winpath , width , height)))
	#endif
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Failed to load %s\n" , argv[4]) ;
		#endif

			return  -1 ;
		}
	}

	width = ((u_int32_t*)cardinal)[0] ;
	height = ((u_int32_t*)cardinal)[1] ;
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

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " Exit image loader\n") ;
#endif

	return  0 ;
}
