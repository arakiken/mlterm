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
	int  counter ;

	printf( "Not available options.\n") ;

	for( counter = 0 ; counter < sizeof( na_options) / sizeof( na_options[0]) ; counter ++)
	{
		printf( " %s\n" , na_options[counter]) ;
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
	int  counter ;

	for( counter = 0 ; counter < argc ; counter ++)
	{
		if( ( p = strrchr( argv[counter] , '-')) != NULL)
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
	strcpy( servaddr.sun_path , "/tmp/mlterm.unix") ;
	
	if( connect( sock_fd , (struct sockaddr*) &servaddr , sizeof( servaddr)) < 0)
	{
		return  1 ;
	}

	for( counter = 0 ; counter < argc ; counter ++)
	{
		write( sock_fd , argv[counter] , strlen( argv[counter])) ;
		write( sock_fd , " " , 1) ;
	}
	write( sock_fd , "\n" , 1) ;

	return  0 ;
}
