/*
 *	$Id$
 */

#include  "x_window_manager.h"

#include  <string.h>		/* memset/memcpy */
#include  <X11/Xlib.h>
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_conf_io.h>


#if  0
#define  __DEBUG
#endif


/* --- static functions --- */

#ifdef  __DEBUG
static int
error_handler(
	Display *  display ,
	XErrorEvent *  event
	)
{
	char  buffer[1024] ;

	XGetErrorText( display , event->error_code , buffer , 1024) ;

	kik_error_printf( "%s\n" , buffer) ;
	
	abort() ;

	return  1 ;
}

static int
ioerror_handler(
	Display *  display
	)
{
	kik_error_printf( "X IO Error.\n") ;
	abort() ;

	return  1 ;
}
#endif


/* --- global functions --- */

int
x_window_manager_init(
	x_window_manager_t *  win_man ,
	Display *  display
	)
{
	win_man->display = display ;
	win_man->screen = DefaultScreen( win_man->display) ;
	win_man->my_window = DefaultRootWindow( win_man->display) ;

	win_man->roots = NULL ;
	win_man->num_of_roots = 0 ;

	win_man->selection_owner = NULL ;

#ifdef  __DEBUG
	XSetErrorHandler( error_handler) ;
	XSetIOErrorHandler( ioerror_handler) ;
#endif

	return  1 ;
}

int
x_window_manager_final(
	x_window_manager_t *  win_man
	)
{
	int  count ;

	for( count = 0 ; count < win_man->num_of_roots ; count ++)
	{
		x_window_unmap( win_man->roots[count]) ;
		x_window_final( win_man->roots[count]) ;
	}

	if( win_man->roots)
	{
		free( win_man->roots) ;
	}

	return  1 ;
}

int
x_window_manager_show_root(
	x_window_manager_t *  win_man ,
	x_window_t *  root ,
	int  x ,
	int  y ,
	int  hint
	)
{
	void *  p ;

	if( ( p = realloc( win_man->roots , sizeof( x_window_t*) * (win_man->num_of_roots + 1))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " realloc failed.\n") ;
	#endif
	
		return  0 ;
	}

	win_man->roots = p ;

	root->win_man = win_man ;
	root->parent = NULL ;
	root->parent_window = win_man->my_window ;
	root->display = win_man->display ;
	root->screen = win_man->screen ;
	root->x = x ;
	root->y = y ;
	
	win_man->roots[win_man->num_of_roots++] = root ;

	x_window_show( root , hint) ;
	
	return  1 ;
}

int
x_window_manager_remove_root(
	x_window_manager_t *  win_man ,
	x_window_t *  root
	)
{
	int  count ;
	
	for( count = 0 ; count < win_man->num_of_roots ; count ++)
	{
		if( win_man->roots[count] == root)
		{
			x_window_unmap( win_man->roots[count]) ;
			x_window_final( root) ;

			win_man->num_of_roots -- ;
			
			if( count == win_man->num_of_roots)
			{
				memset( &win_man->roots[count] , 0 , sizeof( win_man->roots[0])) ;
			}
			else
			{
				memcpy( &win_man->roots[count] , 
					&win_man->roots[win_man->num_of_roots] ,
					sizeof( win_man->roots[0])) ;
			}

			return  1 ;
		}
	}

	return  0 ;
}

int
x_window_manager_own_selection(
	x_window_manager_t *  win_man ,
	x_window_t *  win
	)
{
	if( win_man->selection_owner)
	{
		x_window_manager_clear_selection( win_man , win_man->selection_owner) ;
	}

	win_man->selection_owner = win ;

	return  1 ;
}

int
x_window_manager_clear_selection(
	x_window_manager_t *  win_man ,
	x_window_t *  win
	)
{
	if( win_man->selection_owner == NULL || win_man->selection_owner != win)
	{
		return  0 ;
	}
	
	if( win_man->selection_owner->selection_cleared)
	{
		(*win_man->selection_owner->selection_cleared)( win_man->selection_owner , NULL) ;
	}
	
	win_man->selection_owner = NULL ;

	return  1 ;
}

void
x_window_manager_idling(
	x_window_manager_t *  win_man
	)
{
	int  count ;

	for( count = 0 ; count < win_man->num_of_roots ; count ++)
	{
		x_window_idling( win_man->roots[count]) ;
	}
}

int
x_window_manager_receive_next_event(
	x_window_manager_t *  win_man
	)
{
	if( XEventsQueued( win_man->display , QueuedAfterReading))
	{
		XEvent  event ;
		int  count ;

		do
		{
			XNextEvent( win_man->display , &event) ;

			if( ! XFilterEvent( &event , None))
			{
				for( count = 0 ; count < win_man->num_of_roots ; count ++)
				{
					x_window_receive_event( win_man->roots[count] , &event) ;
				}
			}
		}
		while( XEventsQueued( win_man->display , QueuedAfterReading)) ;
		
		XFlush( win_man->display) ;

		return  1 ;
	}
	else
	{
		/* events should be flushed before waiting in select() */
		XFlush( win_man->display) ;

		return  0 ;
	}	
}
