/*
 *	$Id$
 */

#include  "kik_sig_child.h"

#ifndef  USE_WIN32API

#include  <errno.h>		/* EINTR */
#include  <sys/wait.h>
#include  <signal.h>

#include  "kik_debug.h"
#include  "kik_mem.h"		/* realloc/free */


typedef struct  sig_child_event_listener
{
	void *  self ;
	void (*exited)( void * , pid_t) ;
	
} sig_child_event_listener_t ;


/* --- static variables --- */

static sig_child_event_listener_t *  listeners ;
static u_int  num_of_listeners ;
static int  is_init ;


/* --- static functions --- */

static void
sig_child(
	int  sig
	)
{
	pid_t  pid ;
	int  count ;
	
#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " SIG CHILD received.\n") ;
#endif

	while( ( pid = waitpid( -1 , NULL , WNOHANG)) == -1 && errno == EINTR)
	{
		errno = 0 ;
	}

	for( count = 0 ; count < num_of_listeners ; count ++)
	{
		(*listeners[count].exited)( listeners[count].self , pid) ;
	}
	
	/* reset */
	signal( SIGCHLD , sig_child) ;
}


/* --- global functions --- */

int
kik_sig_child_init(void)
{
	signal( SIGCHLD , sig_child) ;
	is_init = 1 ;

	return  1 ;
}

int
kik_sig_child_final(void)
{
	if( listeners)
	{
		free( listeners) ;
	}
	
	return  1 ;
}

int
kik_sig_child_suspend(void)
{
	if( is_init)
	{
		signal( SIGCHLD , SIG_IGN) ;
	}

	return  1 ;
}

int
kik_sig_child_resume(void)
{
	if( is_init)
	{
		signal( SIGCHLD , sig_child) ;
	}

	return  1 ;
}

int
kik_add_sig_child_listener(
	void *  self ,
	void (*exited)( void * , pid_t)
	)
{
	void *  p ;
	
	if( ( p = realloc( listeners , sizeof( *listeners) * (num_of_listeners + 1))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " realloc failed.\n") ;
	#endif
	
		return  0 ;
	}

	listeners = p ;
	
	listeners[num_of_listeners].self = self ;
	listeners[num_of_listeners].exited = exited ;
	
	num_of_listeners++ ;

	return  1 ;
}

int
kik_remove_sig_child_listener(
	void *  self ,
	void (*exited)( void * , pid_t)
	)
{
	int  count ;

	for( count = 0 ; count < num_of_listeners ; count ++)
	{
		if( listeners[count].self == self &&
			listeners[count].exited == exited)
		{
			listeners[count] = listeners[-- num_of_listeners] ;

			/*
			 * memory area of listener is not shrunk.
			 */

			return  1 ;
		}
	}

	return  0 ;
}

#endif  /* USE_WIN32API */
