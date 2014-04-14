/*
 *	$Id$
 */

#include  <stdio.h>	/* sprintf */
#include  <fcntl.h>	/* open */
#include  <unistd.h>	/* close */
#include  <sys/stat.h>

#include  <kiklib/kik_util.h>	/* DIGIT_STR_LEN */


/* --- static functions --- */

static int
hash_path(
	const char *  path
	)
{
	int  hash ;

	hash = 0 ;
	while( *path)
	{
		hash += *(path++) ;
	}

	return  hash & 65535 /* 0xffff */ ;
}

static void
save_gif(
	const char *  path ,
	u_char *  header ,
	size_t  header_size ,
	u_char *  body ,
	size_t  body_size
#ifdef  USE_WIN32GUI
	, int  colorkey
#endif
	)
{
	int  fd ;

#ifdef  USE_WIN32API
	if( ( fd = open( path , O_WRONLY|O_CREAT|O_BINARY , 0600)) >= 0)
#else
	if( ( fd = open( path , O_WRONLY|O_CREAT , 0600)) >= 0)
#endif
	{
		write( fd , header , header_size) ;
		write( fd , body , body_size) ;
	#ifdef  USE_WIN32GUI
		if( colorkey >= 0)
		{
			u_char  append[] =
				"\x2c\x00\x00\x00\x00\x01\x00\x01\x00\x00\x08\x01\x00\x00" ;

			append[12] = colorkey ;
			write( fd , append , sizeof(append) - 1) ;
		}
	#endif
		write( fd , "\x3b" , 1) ;
		close( fd) ;
	}
}

static u_char *
skip_gif_header(
	u_char *  p
	)
{
	/* Header */

	p += 10 ;

	if( *(p) & 0x80)
	{
		p += (3 * (2 << ((*p) & 0x7))) ;
	}
	p += 3 ;

	return  p ;
}

#ifdef  GDK_PIXBUF_VERSION
/* read gif information from mlterm/anim*.gif file. */
static int
read_gif_info(
	const char *  path ,
	int *  x_off ,
	int *  y_off ,
	int *  width ,
	int *  height
	)
{
	int  fd ;
	u_char  data[1024] ;	/* enough to get necessary gif information */
	ssize_t  len ;

	if( ( fd = open( path , O_RDONLY)) < 0)
	{
		return  0 ;
	}

	len = read( fd , data , sizeof(data)) ;
	close( fd) ;

	/* Cast to char* is necessary because this function can be compiled by g++. */
	if( len >= 6 && strncmp( (char*)data , "GIF89a" , 6) == 0)
	{
		u_char *  p ;

		p = skip_gif_header( data) ;

		if( p + 12 < data + len &&
		    p[0] == 0x21 && p[1] == 0xf9 && p[2] == 0x04 && p[8] == 0x2c)
		{
			*x_off = (p[10] << 8) | p[9] ;
			*y_off = (p[12] << 8) | p[11] ;
			*width = (data[7] << 8) | data[6] ;
			*height = (data[9] << 8) | data[8] ;

			return  1 ;
		}
	}

	return  0 ;
}
#endif

static int
split_animation_gif(
	const char *  path ,
	const char *  dir ,	/* must end with '/'. */
	int  hash
	)
{
	int  fd ;
	struct stat  st ;
	u_char *  header ;
	size_t  header_size ;
	u_char *  body ;
	u_char *  p ;
	ssize_t  len ;
	int  num ;
	char *  split_path ;
	const char *  format ;
	const char *  next_format ;
#ifdef  USE_WIN32GUI
	int  colorkey ;
#endif

#ifdef  USE_WIN32API
	if( ( fd = open( path , O_RDONLY|O_BINARY)) < 0)
#else
	if( ( fd = open( path , O_RDONLY)) < 0)
#endif
	{
		return  0 ;
	}

	/* Cast to u_char* is necessary because this function can be compiled by g++. */
	if( fstat( fd , &st) != 0 ||
	    ! ( header = (u_char*)malloc( st.st_size)))
	{
		close( fd) ;

		return  0 ;
	}

	len = read( fd , header , st.st_size) ;
	close( fd) ;

	/* Header */

	/* Cast to char* is necessary because this function can be compiled by g++. */
	if( len != st.st_size || strncmp( (char*)header , "GIF89a" , 6) != 0)
	{
		free( header) ;

		return  0 ;
	}

	p = skip_gif_header( header) ;
	header_size = p - header ;

	/* Application Extension */

	if( p[0] == 0x21 && p[1] == 0xff)
	{
		p += 19 ;
	}

	/* Other blocks */

	body = NULL ;
	num = -1 ;
	/* animx%d-%d.gif */
	split_path = (char*)alloca( strlen( dir) + 10 + 5 + DIGIT_STR_LEN(int) + 1) ;

	while( p + 2 < header + st.st_size)
	{
		if( *(p++) == 0x21 && *(p++) == 0xf9 && *(p++) == 0x04)
		{
			/* skip the first frame. */
			if( body)
			{
				/* Graphic Control Extension */
				sprintf( split_path , format , dir , hash , num) ;
				save_gif( split_path , header , header_size ,
					body , p - 3 - body
				#ifdef  USE_WIN32GUI
					, colorkey
				#endif
					) ;

				format = next_format ;
			}
			else
			{
				format = "%sanim%d.gif" ;
			}

			/* XXX *p & 4 => Regarded as no dispose. */
			next_format = (*p & 0x4) ? "%sanimx%d-%d.gif" : "%sanim%d-%d.gif" ;
			body = p - 3 ;

		#ifdef  USE_WIN32GUI
			if( p + 13 < header + st.st_size)
			{
				int  frame_xoff ;
				int  frame_yoff ;
				int  frame_width ;
				int  frame_height ;

				frame_xoff = ((p[7] << 8) | p[6]) ;
				frame_yoff = ((p[9] << 8) | p[8]) ;
				frame_width = ((p[11] << 8) | p[10]) ;
				frame_height = ((p[13] << 8) | p[12]) ;

				/*
				 * XXX
				 * GDI+ which clears margin area with an opaque color if the 2nd
				 * or later frame is smaller than the 1st one.
				 * The hack of embedding a transparent color at x=0 y=0 fixes it.
				 */
				if( frame_xoff > 0 || frame_yoff > 0 ||
				    frame_xoff + frame_width < ((header[7] << 8) | header[6]) ||
				    frame_yoff + frame_height < ((header[9] << 8) | header[8]))
				{
					colorkey = p[3] ;
				}
				else
				{
					colorkey = -1 ;
				}
			}
		#endif

			num ++ ;
		}
	}

	if( body)
	{
		sprintf( split_path , format , dir , hash , num) ;
		save_gif( split_path , header , header_size ,
			body , header + st.st_size - body - 1
		#ifdef  USE_WIN32GUI
			, colorkey
		#endif
			) ;
	}

	free( header) ;

	return  1 ;
}
