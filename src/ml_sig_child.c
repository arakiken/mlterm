/*
 *	$Id$
 */

#include  "ml_sig_child.h"

#include  <stdio.h>		/* NULL */
#include  <errno.h>		/* EINTR */
#include  <sys/wait.h>
#include  <signal.h>
#include  <kiklib/kik_debug.h>


#define  MAX_LISTENERS  10


typedef struct  sig_child_event_listener
{
	void *  self ;
	void (*exited)( void * , pid_t) ;
	
} sig_child_event_listener_t ;


/* --- static variables --- */

static sig_child_event_listener_t  listeners[MAX_LISTENERS] ;
static u_int  num_of_listeners ;


/* --- static functions --- */

static void
sig_child(
	int  sig
	)
{
	pid_t  pid ;
	int  counter ;
	
#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " SIG CHILD received.\n") ;
#endif

	while( ( pid = waitpid( -1 , NULL , WNOHANG)) == -1 && errno == EINTR)
	{
		errno = 0 ;
	}

	for( counter = 0 ; counter < num_of_listeners ; counter ++)
	{
		(*listeners[counter].exited)( listeners[counter].self , pid) ;
	}
	
	/* reset */
	signal( SIGCHLD , sig_child) ;
}


/* --- global functions --- */

int
ml_sig_child_init(void)
{
	signal( SIGCHLD , sig_child) ;

	return  1 ;
}

int
ml_sig_child_final(void)
{
	return  1 ;
}

int
ml_add_sig_child_listener(
	void *  self ,
	void (*exited)( void * , pid_t)
	)
{
	if( num_of_listeners == MAX_LISTENERS)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " full of SIG_CHILD listeners.\n") ;
	#endif
	
		return  0 ;
	}

	listeners[num_of_listeners].self = self ;
	listeners[num_of_listeners].exited = exited ;
	
	num_of_listeners++ ;

	return  1 ;
}

int
ml_remove_sig_child_listener(
	void *  self
	)
{
	int  counter ;

	for( counter = 0 ; counter < num_of_listeners ; counter ++)
	{
		if( listeners[counter].self == self)
		{
			listeners[counter] = listeners[-- num_of_listeners] ;

			return  1 ;
		}
	}

	return  0 ;
}
