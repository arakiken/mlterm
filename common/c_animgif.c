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
		write( fd , "\x3b" , 1) ;
		close( fd) ;
	}
}

static int
split_animation_gif(
	const char *  path ,
	const char *  dir ,
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
	char *  format ;

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
	    ! ( p = header = (u_char*)malloc( st.st_size)))
	{
		close( fd) ;

		return  0 ;
	}

	len = read( fd , p , st.st_size) ;
	close( fd) ;

	/* Header */

	/* Cast to char* is necessary because this function can be compiled by g++. */
	if( len != st.st_size || strncmp( (char*)p , "GIF89a" , 6) != 0)
	{
		free( header) ;

		return  0 ;
	}
	p += 10 ;

	if( *(p) & 0x80)
	{
		p += (3 * (2 << ((*p) & 0x7))) ;
	}
	p += 3 ;

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
		if( *(p++) == 0x21 &&
		    *(p++) == 0xf9 &&
		    *(p++) == 0x04)
		{
			/* skip the first frame. */
			if( body)
			{
				/* Graphic Control Extension */
				sprintf( split_path , format , dir , hash , num) ;
				save_gif( split_path , header , header_size ,
					body , p - 3 - body) ;

				/* XXX *p & 4 => Regarded as no dispose. */
				format = (*p & 0x4) ? "%sanimx%d-%d.gif" : "%sanim%d-%d.gif" ;
			}
			else
			{
				format = "%sanim.gif" ;
			}

			body = p - 3 ;
			num ++ ;
		}
	}

	if( body)
	{
		sprintf( split_path , format , dir , hash , num) ;
		save_gif( split_path , header , header_size , body ,
			header + st.st_size - body - 1) ;
	}

	free( header) ;

	return  1 ;
}
