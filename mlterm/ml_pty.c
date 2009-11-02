/*
 *	$Id$
 */

#include  "ml_pty.h"

#include  <unistd.h>		/* close */
#include  <sys/ioctl.h>
#include  <termios.h>
#include  <signal.h>		/* signal/SIGWINCH */
#include  <string.h>		/* strchr/memcpy */
#include  <stdlib.h>		/* putenv */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>	/* realloc/alloca */
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_pty.h>


#if  0
#define  __DEBUG
#endif

typedef struct  ml_pty
{
	int  master ;		/* master pty fd */
	int  slave ;
	char *  slave_name ;
	pid_t  child_pid ;

#ifdef  USE_UTMP
	kik_utmp_t  utmp ;
#endif

	/* model to be written */
	u_char *  buf ;
	size_t  left ;
	size_t  size ;

  	ml_pty_event_listener_t *  pty_listener ;

} ml_pty_t ;



/* --- global functions --- */

ml_pty_t *
ml_pty_new(
	char *  cmd_path ,
	char **  cmd_argv ,
	char **  env ,
	char *  host ,
	u_int  cols ,
	u_int  rows
	)
{
	ml_pty_t *  pty ;
	pid_t  pid ;

	if( ( pty = malloc( sizeof( ml_pty_t))) == NULL)
	{
		return  NULL ;
	}

	pid = kik_pty_fork( &pty->master , &pty->slave , &pty->slave_name) ;

	if( pid == -1)
	{
		return  NULL ;
	}

	if( pid == 0)
	{
		/* child process */

		/*
		 * setting environmental variables.
		 */
		while( *env)
		{
			/* an argument string of putenv() must be allocated memory.(see SUSV2) */
			putenv( strdup( *env)) ;

			env ++ ;
		}

		/* reset signals and spin off the command interpreter */
		signal(SIGINT, SIG_DFL) ;
		signal(SIGQUIT, SIG_DFL) ;
		signal(SIGCHLD, SIG_DFL) ;
		signal(SIGPIPE, SIG_DFL) ;

	#if  0
		/*
		 * XXX is this necessary ?
		 *
		 * mimick login's behavior by disabling the job control signals.
		 * a shell that wants them can turn them back on
		 */
		signal(SIGTSTP , SIG_IGN) ;
		signal(SIGTTIN , SIG_IGN) ;
		signal(SIGTTOU , SIG_IGN) ;
	#endif

		if( strchr( cmd_path , '/') == NULL)
		{
			if( execvp( cmd_path , cmd_argv) < 0)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " execve(%s) failed.\n" , cmd_path) ;
			#endif
			
				exit(1) ;
			}
		}
		else
		{
			if( execv( cmd_path , cmd_argv) < 0)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " execve(%s) failed.\n" , cmd_path) ;
			#endif
			
				exit(1) ;
			}
		}
	}

	/* parent process */

#ifdef  USE_UTMP
	if( ( pty->utmp = kik_utmp_new( pty->slave_name , host , pty->master)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG "utmp failed.\n") ;
	#endif
	}
#endif

	pty->child_pid = pid ;
	pty->buf = NULL ;
	pty->left = 0 ;
	pty->size = 0 ;

  	pty->pty_listener = NULL ;
	
	if( ml_set_pty_winsize( pty , cols , rows) == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_set_pty_winsize() failed.\n") ;
	#endif
	}

	return  pty ;
}

int
ml_pty_delete(
	ml_pty_t *  pty
	)
{
	if( pty->pty_listener && pty->pty_listener->closed)
	{
		(*pty->pty_listener->closed)( pty->pty_listener->self) ;
	}

#ifdef  USE_UTMP
	if( pty->utmp)
	{
		kik_utmp_delete( pty->utmp) ;
	}
#endif

#ifdef  __DEBUG
	kik_debug_printf( "%d fd is closed\n" , pty->master) ;
#endif

	close( pty->master) ;
	close( pty->slave) ;
	free( pty->slave_name) ;
		
	free( pty) ;

	return  1 ;
}

int
ml_set_pty_winsize(
	ml_pty_t *  pty ,
	u_int  cols ,
	u_int  rows
	)
{
	struct winsize  ws ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " win size cols %d rows %d.\n" , cols , rows) ;
#endif

	ws.ws_col = cols ;
	ws.ws_row = rows ;
	ws.ws_xpixel = 0 ;
	ws.ws_ypixel = 0 ;
	
	if( ioctl( pty->master , TIOCSWINSZ , &ws) < 0)
	{
	#ifdef  DBEUG
		kik_warn_printf( KIK_DEBUG_TAG " ioctl(TIOCSWINSZ) failed.\n") ;
	#endif
	
		return  0 ;
	}

	kill( pty->child_pid , SIGWINCH) ;

	return  1 ;
}

int
ml_pty_set_listener(
  	ml_pty_t *  pty,
  	ml_pty_event_listener_t *  pty_listener
	)
{
  	pty->pty_listener = pty_listener ;

  	return  1 ;
}

/*
 * Return size of lost bytes.
 */
size_t
ml_write_to_pty(
	ml_pty_t *  pty ,
	u_char *  buf ,
	size_t  len		/* if 0, flushing buffer. */
	)
{
	u_char *  w_buf ;
	size_t  w_buf_size ;
	ssize_t  written_size ;
	void *  p ;

	w_buf_size = pty->left + len ;
	if( w_buf_size == 0)
	{
		return  0 ;
	}
#if  0
	/*
	 * Little influence without this buffering.
	 */
	else if( len > 0 && w_buf_size < 16)
	{
		/*
		 * Buffering until 16 bytes.
		 */

		if( pty->size < 16)
		{
			if( ( p = realloc( pty->buf , 16)) == NULL)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG
					" realloc failed. %d characters not written.\n" , len) ;
			#endif

				return  len ;
			}
			
			pty->size = 16 ;
			pty->buf = p ;
		}

		memcpy( &pty->buf[pty->left] , buf , len) ;
		pty->left = w_buf_size ;

	#if  0
		kik_debug_printf( "buffered(not written) %d characters.\n" , pty->left) ;
	#endif
	
		return  0 ;
	}
#endif

	if( /* pty->buf && */ len == 0)
	{
		w_buf = pty->buf ;
	}
  	else if( /* pty->buf == NULL && */ pty->left == 0)
        {
          	w_buf = buf ;
        }
  	else if( ( w_buf = alloca( w_buf_size)))
        {
          	memcpy( w_buf , pty->buf , pty->left) ;
		memcpy( &w_buf[pty->left] , buf , len) ;
	}
	else
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" alloca() failed. %d characters not written.\n" , len) ;
	#endif
	
		return  len ;
	}

#ifdef  __DEBUG
	{
		int  i ;
		for( i = 0 ; i < w_buf_size ; i++)
		{
			kik_msg_printf( "%.2x" , w_buf[i]) ;
		}
		kik_msg_printf( "\n") ;
	}
#endif

	written_size = write( pty->master , w_buf , w_buf_size) ;
	if( written_size < 0)
	{
		kik_warn_printf( KIK_DEBUG_TAG " write() failed.\n") ;
		written_size = 0 ;
	}

	if( written_size == w_buf_size)
	{
		pty->left = 0 ;
	
		return  0 ;
	}

	/* w_buf_size - written_size == not_written_size */
	if( w_buf_size - written_size > pty->size)
	{
		if( ( p = realloc( pty->buf , w_buf_size - written_size)) == NULL)
		{
			size_t  lost ;
			
			if( pty->size == 0)
			{
				lost = w_buf_size - written_size ;
				pty->left = 0 ;
			}
			else
			{
				lost = w_buf_size - written_size - pty->size ;
				memcpy( pty->buf , &w_buf[written_size] , pty->size) ;
				pty->left = pty->size ;
			}

		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG
				" realloc failed. %d characters are not written.\n" , lost) ;
		#endif

			return  lost ;
		}
		else
		{
			pty->size = pty->left = w_buf_size - written_size ;
			pty->buf = p ;
		}
	}
	else
	{
		pty->left = w_buf_size - written_size ;
	}
	
	memcpy( pty->buf , &w_buf[written_size] , pty->left) ;

	kik_debug_printf( "%d is not written.\n" , pty->left) ;

	return  0 ;
}

/*
 * Flush pty->buf/pty->left.
 */
size_t
ml_flush_pty(
	ml_pty_t *  pty
	)
{
#if  0
	kik_debug_printf( "flushing buffer.\n") ;
#endif

	return  ml_write_to_pty( pty , NULL , 0) ;
}

size_t
ml_read_pty(
	ml_pty_t *  pty ,
	u_char *  bytes ,
	size_t  left
	)
{
	size_t  read_size ;

	read_size = 0 ;
	while( 1)
	{
		int  ret ;

		ret = read( pty->master , &bytes[read_size] , left) ;
		if( ret <= 0)
		{
			return  read_size ;
		}
		else
		{
			read_size += ret ;
			left -= ret ;
		}
	}
}

pid_t
ml_pty_get_pid(
  	ml_pty_t *  pty
  	)
{
  	/* Cast HANDLE => pid_t */
  	return  (pid_t)pty->child_pid ;
}

int
ml_pty_get_master_fd(
	ml_pty_t *  pty
	)
{
	return  pty->master ;
}

int
ml_pty_get_slave_fd(
	ml_pty_t *  pty
	)
{
	return  pty->slave ;
}

char *
ml_pty_get_slave_name(
	ml_pty_t *  pty
	)
{
	return  pty->slave_name ;
}
