/*
 *	$Id$
 */

#include  <sys/types.h>
#include  <sys/socket.h>	/* socket/bind/listen */
#include  <sys/un.h>		/* sockaddr_un */
#include  <unistd.h>		/* write */

int
main(
	int  argc ,
	char **  argv
	)
{
	int  sock_fd ;
	struct sockaddr_un  servaddr ;
	int  counter ;

	sock_fd = socket( AF_LOCAL , SOCK_STREAM , 0) ;
	memset( &servaddr , 0 , sizeof( servaddr)) ;
	servaddr.sun_family = AF_LOCAL ;
	strcpy( servaddr.sun_path , "/tmp/mlterm.unix") ;
	connect( sock_fd , (struct sockaddr*) &servaddr , sizeof( servaddr)) ;

	for( counter = 0 ; counter < argc ; counter ++)
	{
		write( sock_fd , argv[counter] , strlen( argv[counter])) ;
		write( sock_fd , " " , 1) ;
	}
	write( sock_fd , "\n" , 1) ;


	return  0 ;
}
