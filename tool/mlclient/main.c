/*
 *	$Id$
 */

#include  <stdio.h>
#include  <sys/types.h>
#include  <unistd.h>		/* write */
#include  <string.h>		/* memset */
#include  <kiklib/kik_net.h>	/* socket/bind/listen/sockaddr_un */


/* --- static variables --- */

static char *  na_options[] =
{
	"-@/--screens" ,
	"-P/--ptys" ,
	"-R/--fsrange" ,
	"-W/--sep" ,
	"-Y/--decsp" ,
	"-c/--cp932" ,
	"-i/--xim" ,
	"-j/--daemon" ,
} ;


/* --- static functions --- */

static void
version(void)
{
	printf( "mlclient\n") ;
}

static void
help(void)
{
	int  count ;

	printf( "mlclient [prefix options] [options]\n\n") ;
	printf( "prefix optioins:\n") ;
	printf( "  /dev/...: specify pty with which a new window is opened.\n\n") ;
	printf( "options:\n") ;
	printf( "  -P/--ptylist: print pty list.\n\n") ;
	printf( "  N.A. options among those of mlterm.\n") ;

	for( count = 0 ; count < sizeof( na_options) / sizeof( na_options[0]) ; count ++)
	{
		printf( "  %s\n" , na_options[count]) ;
	}
}


/* --- global functions --- */

int
main(
	int  argc ,
	char **  argv
	)
{
	char *  p ;
	int  sock_fd ;
	struct sockaddr_un  servaddr ;
	char  buf[256] ;
	size_t  len ;
	int  count ;
	
	for( count = 1 ; count < argc ; count ++)
	{
		p = argv[count];
		if( *p == '-')
		{
			p ++ ;
			if ( *p == '-')
			{
				/* long option */
				p ++ ;
			}
			
			if( strcmp( p , "help") == 0 || strcmp( p , "h") == 0)
			{
				help() ;

				return  0 ;
			}
			else if( strcmp( p , "version") == 0 || strcmp( p , "v") == 0)
			{
				version() ;

				return  0 ;
			}
			else if( strcmp( p , "e") == 0)
			{
				/* argvs after -e are NOT options for mlterm */
				break ;
			}
		}
	}

	if( ( sock_fd = socket( AF_LOCAL , SOCK_STREAM , 0)) < 0)
	{
		fprintf( stderr , "Mlterm server dead.\n") ;
		
		return  1 ;
	}
	
	memset( &servaddr , 0 , sizeof( servaddr)) ;
	servaddr.sun_family = AF_LOCAL ;
	sprintf( servaddr.sun_path ,
	#ifdef  USE_WIN32GUI
		"/tmp/mlterm-%d.unix" ,
	#else
		"/tmp/.mlterm-%d.unix" ,
	#endif
		getuid()) ;
	
	if( connect( sock_fd , (struct sockaddr*) &servaddr , sizeof( servaddr)) < 0)
	{
		fprintf( stderr , "Mlterm server dead.\n") ;

		return  1 ;
	}

#if  0
	/* Extract program name. */
	if( ( p = strrchr( argv[0] , '/')))
	{
		argv[0] = p + 1 ;
	}
#endif

	count = 0 ;
	while( 1)
	{
		p = argv[count] ;

		write( sock_fd , " \"" , 2) ;

		while( *p)
		{
			if( *p == '\"')
			{
				write( sock_fd , "\\\"" , 2) ;
			}
			else
			{
				write( sock_fd , p , 1) ;
			}

			p ++ ;
		}

		if( ++ count < argc)
		{
			write( sock_fd , "\" " , 2) ;
		}
		else
		{
			write( sock_fd , "\"\n" , 2) ;

			break ;
		}
	}

#ifndef  USE_WIN32GUI
	/*
	 * XXX
	 * read() is blocked in win32.
	 */
	while( ( len = read( sock_fd , buf , sizeof( buf))) > 0)
	{
		write( STDOUT_FILENO , buf , len) ;
	}
#endif

	close( sock_fd) ;

	return  0 ;
}
