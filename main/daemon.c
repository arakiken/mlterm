/*
 *	$Id$
 */

#include  "daemon.h"

#ifndef  USE_WIN32API

#include  <stdio.h>
#include  <string.h>		/* memset/memcpy */
#include  <unistd.h>		/* setsid/unlink */
#include  <signal.h>		/* SIGHUP/kill */
#include  <errno.h>
#include  <sys/stat.h>		/* umask */

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>	/* alloca/malloc/free */
#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_net.h>	/* socket/bind/listen/sockaddr_un */

#include  <x_screen_manager.h>
#include  <x_event_source.h>


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static int  sock_fd = -1 ;
static char *  un_file ;


/* --- static functions --- */

static void
client_connected(void)
{
	struct sockaddr_un  addr ;
	socklen_t  sock_len ;
	int  fd ;
	FILE *  fp ;
	kik_file_t *  from ;
	char *  line ;
	size_t  line_len ;
	char *  args ;

	fp = NULL ;

	sock_len = sizeof( addr) ;

	if( ( fd = accept( sock_fd , (struct sockaddr *) &addr , &sock_len)) < 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " accept failed.\n") ;
	#endif
	
		return ;
	}

	/*
	 * Set the close-on-exec flag.
	 * If this flag off, this fd remained open until the child process forked in
	 * open_screen_intern()(ml_term_open_pty()) close it.
	 */
	kik_file_set_cloexec( fd) ;

	if( ( fp = fdopen( fd , "r+")) == NULL)
	{
		goto  crit_error ;
	}

	if( ( from = kik_file_new( fp)) == NULL)
	{
		goto  error ;
	}

	if( ( line = kik_file_get_line( from , &line_len)) == NULL)
	{
		kik_file_delete( from) ;
		
		goto  error ;
	}

	if( ( args = alloca( line_len)) == NULL)
	{
		kik_file_delete( from) ;

		goto  error ;
	}

	strncpy( args , line , line_len - 1) ;
	args[line_len - 1] = '\0' ;

	kik_file_delete( from) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s\n" , args) ;
#endif

	if( strstr( args , "--kill"))
	{
		daemon_final() ;
		fprintf( fp , "bye\n") ;
		exit( 0) ;
	}
	else if( ! x_mlclient( args , fp))
	{
		goto  error ;
	}

	fclose( fp) ;

	return ;
	
error:
	{
		char  msg[] = "Error happened.\n" ;

		write( fd , msg , sizeof( msg) - 1) ;
	}
	
crit_error:
	if( fp)
	{
		fclose( fp) ;
	}
	else
	{
		close( fd) ;
	}
}


/* --- global functions --- */

int
daemon_init(void)
{
	pid_t  pid ;
	struct sockaddr_un  servaddr ;
	char *  path ;

	if( ( path = kik_get_user_rc_path( "mlterm/socket")) == NULL)
	{
		return  0 ;
	}

	if( strlen( path) >= sizeof(servaddr.sun_path) || ! kik_mkdir_for_file( path , 0700))
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Failed mkdir for %s\n" , path) ;
	#endif
		free( path) ;

		return  0 ;
	}

	memset( &servaddr , 0 , sizeof( servaddr)) ;
	servaddr.sun_family = AF_LOCAL ;
	strcpy( servaddr.sun_path , path) ;
	free( path) ;
	path = servaddr.sun_path ;

	if( ( sock_fd = socket( PF_LOCAL , SOCK_STREAM , 0)) < 0)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " socket failed\n") ;
	#endif

		return  0 ;
	}
	kik_file_set_cloexec( sock_fd);

	for( ;;)
	{
		int  ret ;
		int  saved_errno ;
		mode_t  mode ;

		mode = umask( 077) ;
		ret = bind( sock_fd , (struct sockaddr *) &servaddr , sizeof( servaddr)) ;
		saved_errno = errno ;
		umask( mode) ;

		if( ret == 0)
		{
			break ;
		}
		else if( saved_errno == EADDRINUSE)
		{
			if( connect( sock_fd , (struct sockaddr*) &servaddr , sizeof( servaddr)) == 0)
			{
				close( sock_fd) ;

				kik_msg_printf( "daemon is already running.\n") ;

				return  -1 ;
			}

			kik_msg_printf( "removing stale lock file %s.\n" , path) ;

			if( unlink( path) == 0)
			{
				continue ;
			}
		}
		else
		{
			kik_msg_printf( "failed to lock file %s: %s\n" ,
				path , strerror(saved_errno)) ;

			goto  error ;
		}
	}

	pid = fork() ;

	if( pid == -1)
	{
		goto  error ;
	}

	if( pid != 0)
	{
		exit(0) ;
	}

	/*
	 * child
	 */

	/*
	 * This process becomes a session leader and purged from control terminal.
	 */
	setsid() ;

	/*
	 * SIGHUP signal when the child process exits must not be sent to
	 * the grandchild process.
	 */
	signal( SIGHUP , SIG_IGN) ;

	pid = fork() ;

	if( pid != 0)
	{
		exit(0) ;
	}

	/*
	 * grandchild
	 */

	if( listen( sock_fd , 1024) < 0)
	{
		goto  error ;
	}

	/* Mark started as daemon. */
	un_file = strdup( path) ;

	x_event_source_add_fd( sock_fd , client_connected) ;

	return  1 ;

error:
	close( sock_fd) ;

	return  0 ;
}

int
daemon_final(void)
{
	if( un_file)
	{
		close( sock_fd) ;
		unlink( un_file) ;
		free( un_file) ;
	}

	return  1 ;
}

#endif	/* USE_WIN32API */
