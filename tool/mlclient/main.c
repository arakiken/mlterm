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
	"-K/--maxptys" ,
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

				exit(0) ;
			}
			else if( strcmp( p , "version") == 0 || strcmp( p , "v") == 0)
			{
				version() ;

				exit(0) ;
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
	sprintf( servaddr.sun_path , "/tmp/.mlterm-%d.unix" , getuid()) ;
	
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

	write( sock_fd , argv[0] , strlen( argv[0])) ;

	if( argc > 1)
	{
		count = 1 ;
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
				write( sock_fd , "\"" , 1) ;
			}
			else
			{
				write( sock_fd , "\"\n" , 2) ;

				break ;
			}
		}
	}

	while( ( len = read( sock_fd , buf , sizeof( buf))) > 0)
	{
		write( STDOUT_FILENO , buf , len) ;
	}
	
	close( sock_fd) ;

	return  0 ;
}
