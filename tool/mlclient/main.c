/*
 *	$Id$
 */

#include  <sys/types.h>
#include  <unistd.h>		/* write */
#include  <string.h>		/* memset */
#include  <kiklib/kik_net.h>	/* socket/bind/listen/sockaddr_un */

int
main(
	int  argc ,
	char **  argv
	)
{
	int  sock_fd ;
	struct sockaddr_un  servaddr ;
	int  counter ;

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
