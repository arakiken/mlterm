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

	printf( "Not available options.\n") ;

	for( count = 0 ; count < sizeof( na_options) / sizeof( na_options[0]) ; count ++)
	{
		printf( " %s\n" , na_options[count]) ;
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
	int  count ;

	for( count = 0 ; count < argc ; count ++)
	{
		if( ( p = strrchr( argv[count] , '-')) != NULL)
		{
			p ++ ;
			
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
		return  1 ;
	}
	
	memset( &servaddr , 0 , sizeof( servaddr)) ;
	servaddr.sun_family = AF_LOCAL ;
	sprintf( servaddr.sun_path , "/tmp/.mlterm-%d.unix" , getuid()) ;
	
	if( connect( sock_fd , (struct sockaddr*) &servaddr , sizeof( servaddr)) < 0)
	{
		return  1 ;
	}

	count = 0 ;
	while( 1)
	{
		write( sock_fd , argv[count] , strlen( argv[count])) ;

		if( ++ count < argc)
		{
			write( sock_fd , " " , 1) ;
		}
		else
		{
			write( sock_fd , "\n" , 1) ;

			break ;
		}
	}

	return  0 ;
}
