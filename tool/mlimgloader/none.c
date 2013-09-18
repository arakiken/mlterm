/*
 *	$Id$
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>	/* strstr */
#include <stdlib.h>	/* malloc/free/atoi */

#include <kiklib/kik_debug.h>
#include <kiklib/kik_types.h>	/* u_int32_t/u_int16_t */
#include <kiklib/kik_def.h>	/* SSIZE_MAX, USE_WIN32API */
#if  defined(__CYGWIN__) || defined(__MSYS__)
#include <kiklib/kik_path.h>	/* cygwin_conv_to_win32_path */
#endif

#ifdef  USE_WIN32API
#include <fcntl.h>	/* O_BINARY */
#endif


#if  0
#define  __DEBUG
#endif


/* --- static functions --- */

#define  BUILTIN_IMAGELIB	/* Necessary to use create_cardinals_from_sixel() */
#include  "../../common/c_imagelib.c"


/* --- global functions --- */

int
main(
	int  argc ,
	char **  argv
	)
{
	u_char *  cardinal ;
	ssize_t  size ;
	u_int  width ;
	u_int  height ;

#if  0
	kik_set_msg_log_file_name( "mlterm/msg.log") ;
#endif

	if( argc != 6 || strcmp( argv[5] , "-c") != 0)
	{
		return  -1 ;
	}

	if( ! ( cardinal = (u_char*)create_cardinals_from_sixel( argv[4])))
	{
	#if  defined(__CYGWIN__) || defined(__MSYS__)
	#define  MAX_PATH  260	/* 3+255+1+1 */
		char  winpath[MAX_PATH] ;
		cygwin_conv_to_win32_path( argv[4] , winpath) ;

		if( ! ( cardinal = (u_char*)create_cardinals_from_sixel( winpath)))
	#endif
		{
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
