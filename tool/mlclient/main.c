/*
 *	$Id$
 */

#include  <stdio.h>
#include  <stdlib.h>		/* free */
#include  <sys/types.h>
#include  <unistd.h>		/* write */
#include  <string.h>		/* memset */
#include  <kiklib/kik_conf_io.h>	/* kik_get_user_rc_path */
#ifndef  USE_WIN32API
#include  <kiklib/kik_net.h>	/* socket/bind/listen/sockaddr_un */
#endif


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
	"   --depth" ,
	"   --maxptys" ,
	"   --button3" ,
	"   --clip" ,
	"   --restart" ,
	"   --logmsg"
} ;


/* --- static functions --- */

static void
version(void)
{
	printf( "mlclient(x)\n") ;
}

static void
help(void)
{
	int  count ;

	printf( "mlclient(x) [prefix options] [options]\n\n") ;
	printf( "prefix optioins:\n") ;
	printf( "  /dev/...: specify pty with which a new window is opened.\n\n") ;
	printf( "options:\n") ;
	printf( "  -P/--ptylist: print pty list.\n") ;
	printf( "     --kill: kill mlterm server.\n") ;
	printf( "  (--ptylist and --kill options are available if mlterm server is alive.)\n\n") ;
	printf( "  N.A. options among those of mlterm.\n") ;

	for( count = 0 ; count < sizeof( na_options) / sizeof( na_options[0]) ; count ++)
	{
		printf( "  %s\n" , na_options[count]) ;
	}

	printf( "  (Options related to window, font, color and appearance aren't\n") ;
	printf( "   available in mlclientx.)\n") ;
}

#ifndef  USE_WIN32API
static int
set_daemon_socket_path(
	struct sockaddr_un *  addr
	)
{
	const char name[] = "/.mlterm/socket" ;
	const char *  dir ;

	if( ( dir = getenv( "HOME")) == NULL || '/' != dir[0] )
	{
	       return  0 ;
	}

	if( strlen( dir) + sizeof(name) > sizeof( addr->sun_path))
	{
	       return  0 ;
	}

	sprintf( addr->sun_path , "%s%s" , dir , name) ;
	
	return  1 ;
}
#endif

static int
write_argv(
	int  argc ,
	char **  argv ,
	int  fd
	)
{
	char *  p ;
	int  count ;
	
	/* Extract program name. */
	if( ( p = strrchr( argv[0] , '/')))
	{
		argv[0] = p + 1 ;
	}

	/* Don't quote argv[0] by "" for "\x1b]5379;mlclient" sequence. */
	write( fd , argv[0] , strlen(argv[0])) ;

	if( argc == 1)
	{
		return  1 ;
	}
	
	count = 1 ;
	while( 1)
	{
		p = argv[count] ;

		write( fd , " \"" , 2) ;
		
		while( *p)
		{
			if( *p == '\"')
			{
				write( fd , "\\\"" , 2) ;
			}
		#if  0
			else if( *p == '=')
			{
				/*
				 * mlterm 3.0.6 or before doesn't accept '=' in
				 * "\x1b]5379;mlclient" sequence.
				 */
				write( fd , "\" \"" , 3) ;
			}
		#endif
			else
			{
				write( fd , p , 1) ;
			}

			p ++ ;
		}

		if( ++ count < argc)
		{
			write( fd , "\" " , 2) ;
		}
		else
		{
			write( fd , "\"" , 1) ;

			break ;
		}
	}

	return  1 ;
}

/* --- global functions --- */

int
main(
	int  argc ,
	char **  argv
	)
{
	int  count ;
	char *  p ;
	
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

#ifndef  USE_WIN32API
	if( strstr( argv[0] , "mlclientx") == NULL)
	{
		int  fd ;
		struct sockaddr_un  servaddr ;

		if( ( fd = socket( AF_LOCAL , SOCK_STREAM , 0)) != -1)
		{
			memset( &servaddr , 0 , sizeof( servaddr)) ;
			servaddr.sun_family = AF_LOCAL ;
			if( set_daemon_socket_path( &servaddr))
			{
				if( connect( fd , (struct sockaddr*) &servaddr ,
					sizeof( servaddr)) != -1)
				{
					char  buf[256] ;
					ssize_t  len ;
				
					write_argv( argc , argv , fd) ;
					write( fd , "\n" , 1) ;

					while( ( len = read( fd , buf , sizeof( buf))) > 0)
					{
						write( STDERR_FILENO , buf , len) ;

						if( len == 16 &&
						    strncmp( buf , "Error happened.\n" , 16) == 0)
						{
							close( fd) ;
							goto  config_proto ;
						}
					}
			
					close( fd) ;

					return  0 ;
				}
			}
			
			close( fd) ;
		}

		fprintf( stderr , "Mlterm server is dead.\n") ;
	}

config_proto:
	fprintf( stderr , "Retrying by configuration protocol.\n") ;
#endif

	write( STDOUT_FILENO , "\x1b]5379;" , 7) ;
	write_argv( argc , argv , STDOUT_FILENO) ;
	write( STDOUT_FILENO , "\x07" , 1) ;

	return  0 ;
}
