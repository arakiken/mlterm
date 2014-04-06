/*
 *	$Id$
 */

#ifdef  BUILTIN_IMAGELIB

#include  <kiklib/kik_util.h>	/* DIGIT_STR_LEN */

#include  "c_sixel.c"
#include  "c_animgif.c"


/* for registobmp on cygwin */
#ifndef  BINDIR
#define  BINDIR  "/bin"
#endif

/* for registobmp on other platforms */
#ifndef  LIBEXECDIR
#define  LIBEXECDIR  "/usr/local/libexec"
#endif


/* --- static functions --- */

#if  defined(USE_WIN32API)

static int
convert_regis_to_bmp(
	char *  path
	)
{
	size_t  len = strlen( path) ;
	char  cmd[17 + len * 2] ;
	STARTUPINFO  si ;
	PROCESS_INFORMATION  pi ;

	path[len - 4] = '\0' ;
	sprintf( cmd , "registobmp.exe %s.rgs %s.bmp" , path , path) ;
	strcat( path , ".bmp") ;

	ZeroMemory(&si,sizeof(STARTUPINFO)) ;
	si.cb = sizeof(STARTUPINFO) ;
	si.dwFlags = STARTF_FORCEOFFFEEDBACK ;

	if( CreateProcess( NULL , cmd , NULL , NULL , FALSE , CREATE_NO_WINDOW ,
			NULL , NULL , &si , &pi))
	{
		DWORD  code ;

		WaitForSingleObject( pi.hProcess , INFINITE) ;
		GetExitCodeProcess( pi.hProcess , &code) ;
		CloseHandle( pi.hProcess) ;
		CloseHandle( pi.hThread) ;

		if( code == 0)
		{
			return  1 ;
		}
	}

	return  0 ;
}

#elif  defined(USE_WIN32GUI) || defined(GDK_PIXBUF_VERSION)

#include  <unistd.h>	/* execve */
#include  <sys/wait.h>
#include  <kiklib/kik_path.h>	/* kik_basename */

static int
convert_regis_to_bmp(
	char *  path
	)
{
	pid_t  pid ;
	int  status ;

	if( ( pid = fork()) == -1)
	{
		return  0 ;
	}

	if( pid == 0)
	{
		char *  new_path ;
		size_t  len ;
	#if  defined(__CYGWIN__) || defined(__MSYS__)
		/* To make registobmp work even if it (or SDL) doesn't depend on cygwin. */
		char *  file ;

		file = kik_basename( path) ;
		if( file && path < file)
		{
			*(file - 1) = '\0' ;
			chdir( path) ;
			path = file ;
		}
	#endif

		len = strlen( path) ;

		/* Cast to char* is necessary because this function can be compiled by g++. */
		if( ( new_path = (char*)malloc( len + 1)))
		{
			char *  argv[4] ;

			strncpy( new_path , path , len - 4) ;
			strcpy( new_path + len - 4 , ".bmp") ;

		#if  defined(__CYGWIN__) || defined(__MSYS__)
			argv[0] = BINDIR "/registobmp" ;
		#else
			argv[0] = LIBEXECDIR "/mlterm/registobmp" ;
		#endif
			argv[1] = path ;
			argv[2] = new_path ;
			argv[3] = NULL ;

			execve( argv[0] , argv , NULL) ;
		}

		exit(1) ;
	}

	waitpid( pid , &status , 0) ;

	if( WEXITSTATUS(status) == 0)
	{
		strcpy( path + strlen(path) - 4 , ".bmp") ;

		return  1 ;
	}

	return  0 ;
}

#endif	/* USE_WIN32API */


#ifdef  GDK_PIXBUF_VERSION

#include  <kiklib/kik_str.h>	/* kik_str_alloca_dup */
#include  <kiklib/kik_conf_io.h>	/* kik_get_user_rc_path */

static void
pixbuf_destroy_notify(
	guchar *  pixels ,
	gpointer  data
	)
{
	free( pixels) ;
}

static GdkPixbuf *
gdk_pixbuf_new_from_sixel(
	const char *  path
	)
{
	u_char *  pixels ;
	u_int  width ;
	u_int  height ;

	if( ! ( pixels = load_sixel_from_file( path , &width , &height)))
	{
		return  NULL ;
	}

	/* load_sixel_from_file returns 4 bytes per pixel data. */
	return  gdk_pixbuf_new_from_data( pixels , GDK_COLORSPACE_RGB , TRUE , 8 ,
			width , height , width * 4 , pixbuf_destroy_notify , NULL) ;
}

#define  create_cardinals_from_sixel( path , width , height)  (NULL)

/* create an CARDINAL array for_NET_WM_ICON data */
static u_int32_t *
create_cardinals_from_pixbuf(
	GdkPixbuf *  pixbuf
	)
{
	u_int  width ;
	u_int  height ;
	u_int32_t *  cardinal ;
	int  rowstride ;
	u_char *  line ;
	u_char *  pixel ;
	u_int i , j ;

	width = gdk_pixbuf_get_width( pixbuf) ;
	height = gdk_pixbuf_get_height( pixbuf) ;

	if( width > ((SSIZE_MAX / sizeof(*cardinal)) - 2) / height ||	/* integer overflow */
	    ! ( cardinal = malloc( ( width * height + 2) * sizeof(*cardinal))))
	{
		return  NULL ;
	}

	rowstride = gdk_pixbuf_get_rowstride( pixbuf) ;
	line = gdk_pixbuf_get_pixels( pixbuf) ;

	/* format of the array is {width, height, ARGB[][]} */
	cardinal[0] = width ;
	cardinal[1] = height ;
	if( gdk_pixbuf_get_has_alpha( pixbuf))
	{
		for( i = 0 ; i < height ; i++)
		{
			pixel = line ;
			line += rowstride;
			for( j = 0 ; j < width ; j++)
			{
				/* RGBA to ARGB */
				cardinal[(i*width+j)+2] = ((((((u_int32_t)(pixel[3]) << 8)
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
			for( j = 0 ; j < width ; j++)
			{
				/* all pixels are completely opaque (0xFF) */
				cardinal[(i*width+j)+2] = ((((((u_int32_t)(0x0000FF) <<8)
								+ pixel[0]) << 8)
								+ pixel[1]) << 8) + pixel[2] ;
				pixel += 3 ;
			}
		}
	}

	return  cardinal ;
}

static GdkPixbuf *
gdk_pixbuf_new_from(
	const char *  path
	)
{
	GdkPixbuf *  pixbuf ;

	if( ! strstr( path , ".six") || ! ( pixbuf = gdk_pixbuf_new_from_sixel( path)))
	{
		if( strcmp( path + strlen(path) - 4 , ".gif") == 0 &&
		    ! strstr( path , "mlterm/anim"))
		{
			/* Animation GIF */

			char *  dir ;

			if( ( dir = kik_get_user_rc_path( "mlterm/")))
			{
				int  hash ;

				hash = hash_path( path) ;

				if( strstr( path , "://"))
				{
					char *  cmd ;

					if( ! ( cmd = alloca( 25 + strlen( path) +
							strlen( dir) + DIGIT_STR_LEN(int) + 1)))
					{
						goto  end ;
					}

					sprintf( cmd , "curl -L -k -s %s > %sanim.gif" ,
						path , dir) ;
					if( system( cmd) != 0)
					{
						goto  end ;
					}

					path = cmd + 14 + strlen( path) + 3 ;
				}

				split_animation_gif( path , dir , hash) ;

			end:
				free( dir) ;
			}
		}

	#if GDK_PIXBUF_MAJOR >= 2

		if( strstr( path , "://"))
		{
		#ifdef  __G_IO_H__
			/*
			 * gdk-pixbuf depends on gio. (__G_IO_H__ is defined if
			 * gdk-pixbuf-core.h includes gio.h)
			 */
			GFile *  file ;
			GInputStream *  in ;

			if( ( in = (GInputStream*)g_file_read(
					( file = g_vfs_get_file_for_uri(
							g_vfs_get_default() , path)) ,
					NULL , NULL)))
			{
				pixbuf = gdk_pixbuf_new_from_stream( in , NULL , NULL) ;
				g_object_unref( in) ;
			}
			else
		#endif
			{
				char *  cmd ;

				pixbuf = NULL ;

				if( ( cmd = alloca( 11 + strlen( path) + 1)))
				{
					FILE *  fp ;

					sprintf( cmd , "curl -L -k -s %s" , path) ;
					if( ( fp = popen( cmd , "r")))
					{
						GdkPixbufLoader *  loader ;
						guchar  buf[65536] ;
						size_t  len ;

						loader = gdk_pixbuf_loader_new() ;
						while( ( len = fread( buf , 1 , sizeof(buf) , fp))
						       > 0)
						{
							gdk_pixbuf_loader_write( loader ,
								buf , len , NULL) ;
						}
						gdk_pixbuf_loader_close( loader , NULL) ;

						pclose( fp) ;

						if( ( pixbuf = gdk_pixbuf_loader_get_pixbuf(
									loader)))
						{
							g_object_ref( pixbuf) ;
						}

						g_object_unref( loader) ;
					}
				}
			}

		#ifdef  __G_IO_H__
			g_object_unref( file) ;
		#endif
		}
		else
		{
			if( strstr( path , ".rgs"))
			{
				char *  new_path ;

				new_path = kik_str_alloca_dup( path) ;
				if( convert_regis_to_bmp( new_path))
				{
					path = new_path ;
				}
			}

			pixbuf = gdk_pixbuf_new_from_file( path , NULL) ;
		}

	#else	/* GDK_PIXBUF_MAJOR */

		pixbuf = gdk_pixbuf_new_from_file( path) ;

	#endif	/* GDK_PIXBUF_MAJOR */
	}

	return  pixbuf ;
}

#else	/* GDK_PIXBUF_VERSION */

#define  gdk_pixbuf_new_from_sixel(path)  (NULL)

static u_int32_t *
create_cardinals_from_sixel(
	const char *  path
	)
{
	u_int  width ;
	u_int  height ;
	u_int32_t *  cardinal ;

	if( ! ( cardinal = (u_int32_t*)load_sixel_from_file( path , &width , &height)))
	{
		return  NULL ;
	}

	cardinal -= 2 ;

	cardinal[0] = width ;
	cardinal[1] = height ;

	return  cardinal ;
}

#endif	/* GDK_PIXBUF_VERSION */

#endif  /* BUILTIN_IMAGELIB */


#ifdef  USE_X11

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
	u_long  min = 0xffffff ;
	u_long  diff ;
	int  diff_r , diff_g , diff_b ;

	for( i = 0 ; i < len ; i++)
	{
		/* lazy color-space conversion*/
		diff_r = red - (color_list[i].red >> 8) ;
		diff_g = green - (color_list[i].green >> 8) ;
		diff_b = blue - (color_list[i].blue >> 8) ;
		diff = diff_r * diff_r * 9 + diff_g * diff_g * 30 + diff_b * diff_b ;
		if ( diff < min)
		{
			min = diff ;
			closest = i ;
			/* no one may notice the difference (4[2^3/2]*4*9+4*4*30+4*4) */
			if ( diff < 640)
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

#endif	/* USE_X11 */
