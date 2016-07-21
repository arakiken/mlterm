/*
 *	$Id$
 */

#include  <stdio.h>
#include  <stdarg.h>
#include  <stdlib.h>		/* getenv */
#include  <unistd.h>
#include  <fcntl.h>		/* open */
#include  <sys/select.h>	/* select */
#include  <errno.h>
#include  <string.h>		/* memcpy */
#include  <signal.h>
#include  <sys/socket.h>
#include  <sys/un.h>
#include  <netdb.h>
#include  <netinet/in.h>
#include  <termios.h>
#include  <sys/stat.h>


/* --- static variables --- */

static int  connect_fd = -1 ;
static int  pty_fd = -1 ;
static struct termios  std_tio ;


/* --- static functions --- */

static void
help(void)
{
	printf( "Usage: mlclient-con [options]\n") ;
}

static char *
get_path(
	char *  file
	)
{
	char *  home ;
	char *  path ;
	char *  p ;

	if( ! ( home = getenv( "HOME")) ||
	    ! ( path = malloc( strlen( home) + 16 + strlen( file) + 1)))
	{
		return  NULL ;
	}

	sprintf( path , "%s/.config/mlterm/%s" , home , file) ;
	p = strrchr( path , '/') ;
	if( p > path + strlen(home) + 15)
	{
		struct stat  st ;

		*p = '\0' ;
		if( stat( path , &st) == 0)
		{
			*p = '/' ;

			return  path ;
		}
	}

	sprintf( path , "%s/.mlterm/%s" , home , file) ;

	return  path ;
}

static void
debug_printf(
	const char *  format ,
	...
	)
{
	va_list  arg_list ;
	static char *  path ;
	FILE *  fp ;

	va_start( arg_list , format) ;

	if( ! path && ! ( path = get_path( "msg.log")))
	{
		return ;
	}

	if( ( fp = fopen( path , "a")))
	{
		vfprintf( fp , format , arg_list) ;
		fclose( fp) ;
	}
}

static void
sig_winch(
	int  sig
	)
{
	struct winsize  ws ;

	if( ioctl( pty_fd , TIOCGWINSZ , &ws) == 0)
	{
		char msg[10 + 11 * 4 + 1] ;

		sprintf( msg , "\x1b[8;%d;%d;4;%d;%dt" , ws.ws_row , ws.ws_col ,
					ws.ws_ypixel , ws.ws_xpixel) ;
		write( connect_fd , msg , strlen(msg)) ;
	}

	signal( SIGWINCH , sig_winch) ;
}

static int
connect_to_server(
	int  argc ,
	char **  argv
	)
{
	char *  path ;
	struct sockaddr_un  servaddr ;
	int  count ;
	struct termios  tio ;

	if( ! ( path = get_path( "socket-con")))
	{
		return  0 ;
	}

	if( ( connect_fd = socket( AF_LOCAL , SOCK_STREAM , 0)) == -1)
	{
		free( path) ;

		return  0 ;
	}

	memset( &servaddr , 0 , sizeof( servaddr)) ;
	servaddr.sun_family = AF_LOCAL ;
	strcpy( servaddr.sun_path , path) ;
	free( path) ;
	path = servaddr.sun_path ;

	if( connect( connect_fd , (struct sockaddr*) &servaddr , sizeof( servaddr)) == -1)
	{
		close( connect_fd) ;

		return  0 ;
	}

	fcntl( connect_fd , F_SETFL , fcntl( connect_fd , F_GETFL , 0) & ~O_NONBLOCK) ;

	write( connect_fd , argv[0] , strlen(argv[0])) ;
	for( count = 0 ; count < argc ; count++)
	{
		write( connect_fd , " \"" , 2) ;
		write( connect_fd , argv[count] , strlen(argv[count])) ;
		write( connect_fd , "\"" , 1) ;
	}
	write( connect_fd , "\n" , 1) ;

	if( ( pty_fd = open( ttyname(STDIN_FILENO) , O_RDWR)) == -1)
	{
		close( connect_fd) ;

		return  0 ;
	}

	close(STDIN_FILENO) ;
	close(STDOUT_FILENO) ;
	close(STDERR_FILENO) ;

	tio = std_tio ;
	tio.c_iflag &= ~(IXON|IXOFF|ICRNL|INLCR|IGNCR|IMAXBEL|ISTRIP) ;
	tio.c_iflag |= IGNBRK ;
	tio.c_oflag &= ~(OPOST|ONLCR|OCRNL|ONLRET) ;
	tio.c_lflag &= ~(IEXTEN|ICANON|ECHO|ECHOE|ECHONL|ECHOCTL|ECHOPRT|ECHOKE|ECHOCTL|ISIG) ;
	tio.c_cc[VMIN] = 1 ;
	tio.c_cc[VTIME] = 0 ;

	tcsetattr( pty_fd , TCSANOW , &tio) ;

	sig_winch(SIGWINCH) ;

	return  1 ;
}


/* --- global functions --- */

int
main(
	int  argc ,
	char **  argv
	)
{
	fd_set  fds ;
	int  maxfd ;

	if( argc == 2 && strcmp( argv[1] , "-h") == 0)
	{
		help() ;

		return  0 ;
	}

	tcgetattr( STDIN_FILENO , &std_tio) ;

	if( ! connect_to_server( argc , argv))
	{
		return  -1 ;
	}

	while( 1)
	{
		char  buf[1024] ;
		ssize_t  len ;

		FD_ZERO(&fds) ;

		FD_SET( pty_fd , &fds) ;
		FD_SET( connect_fd , &fds) ;

		if( pty_fd > connect_fd)
		{
			maxfd = pty_fd ;
		}
		else
		{
			maxfd = connect_fd ;
		}

		if( select( maxfd + 1 , &fds , NULL , NULL , NULL) < 0)
		{
			/* received signal */
			continue ;
		}

		if( FD_ISSET( pty_fd , &fds))
		{
			fcntl( pty_fd , F_SETFL ,
				fcntl( pty_fd , F_GETFL , 0) | O_NONBLOCK) ;
			while( ( len = read( pty_fd , buf , sizeof(buf))) > 0)
			{
				write( connect_fd , buf , len) ;
			}
			fcntl( pty_fd , F_SETFL ,
				fcntl( pty_fd , F_GETFL , 0) & ~O_NONBLOCK) ;
		}

		if( FD_ISSET( connect_fd , &fds))
		{
			fcntl( connect_fd , F_SETFL ,
				fcntl( connect_fd , F_GETFL , 0) | O_NONBLOCK) ;
			while( ( len = read( connect_fd , buf , sizeof(buf))) > 0)
			{
				write( pty_fd , buf , len) ;
			}
			fcntl( connect_fd , F_SETFL ,
				fcntl( connect_fd , F_GETFL , 0) & ~O_NONBLOCK) ;
		}
	}

	return  0 ;
}
