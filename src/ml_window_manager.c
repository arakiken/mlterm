/*
 *	$Id$
 */

#include  "ml_window_manager.h"

#include  <string.h>			/* memset/memcpy */
#include  <X11/Xlib.h>
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_conf_io.h>

#include  "ml_window_intern.h"
#include  "ml_xim.h"


#define  FOREACH_ROOTS(win_man,counter) \
	for( (counter) = 0 ; (counter) < (win_man)->num_of_roots ; (counter) ++)


/* --- global functions --- */

int
ml_window_manager_init(
	ml_window_manager_t *  win_man ,
	char *  disp_name ,
	int  use_xim
	)
{
	if( ( win_man->display = XOpenDisplay( disp_name)) == NULL)
	{
		kik_msg_printf( " display %s couldn't be opened.\n" , disp_name) ;
		
		return  0 ;
	}
	
	win_man->screen = DefaultScreen( win_man->display) ;
	win_man->my_window = DefaultRootWindow( win_man->display) ;

	win_man->roots = NULL ;
	win_man->num_of_roots = 0 ;

	win_man->selection_owner = NULL ;
	
	ml_window_init_atom( win_man->display) ;

	if( use_xim)
	{
		ml_xim_init( win_man->display) ;
	}
	else
	{
		ml_xim_init( NULL) ;
	}
	
	return  1 ;
}

int
ml_window_manager_final(
	ml_window_manager_t *  win_man
	)
{
	int  counter ;
	
	FOREACH_ROOTS(win_man,counter)
	{
		ml_window_final( win_man->roots[counter]) ;
	}

	if( win_man->roots)
	{
		free( win_man->roots) ;
	}

	ml_xim_final() ;

	XCloseDisplay( win_man->display) ;

	return  1 ;
}

int
ml_window_manager_show_root(
	ml_window_manager_t *  win_man ,
	ml_window_t *  root ,
	int  x ,
	int  y ,
	int  hint
	)
{
	void *  p ;

	if( ( p = realloc( win_man->roots , sizeof( ml_window_t*) * (win_man->num_of_roots + 1))) == NULL)
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

	ml_window_show( root , hint) ;
	
	return  1 ;
}

int
ml_window_manager_remove_root(
	ml_window_manager_t *  win_man ,
	ml_window_t *  root
	)
{
	int  counter ;
	
	FOREACH_ROOTS(win_man,counter)
	{
		if( win_man->roots[counter] == root)
		{
			ml_window_final( root) ;

			win_man->num_of_roots -- ;
			
			if( counter == win_man->num_of_roots)
			{
				memset( &win_man->roots[counter] , 0 , sizeof( win_man->roots[0])) ;
			}
			else
			{
				memcpy( &win_man->roots[counter] , 
					&win_man->roots[win_man->num_of_roots] ,
					sizeof( win_man->roots[0])) ;
			}

			return  1 ;
		}
	}

	return  0 ;
}

int
ml_window_manager_own_selection(
	ml_window_manager_t *  win_man ,
	ml_window_t *  win
	)
{
	if( win_man->selection_owner)
	{
		ml_window_manager_clear_selection( win_man , win_man->selection_owner) ;
	}

	win_man->selection_owner = win ;

	return  1 ;
}

int
ml_window_manager_clear_selection(
	ml_window_manager_t *  win_man ,
	ml_window_t *  win
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
ml_window_manager_idling(
	ml_window_manager_t *  win_man
	)
{
	int  counter ;

	FOREACH_ROOTS(win_man,counter)
	{
		ml_window_idling( win_man->roots[counter]) ;
	}
}

int
ml_window_manager_receive_next_event(
	ml_window_manager_t *  win_man
	)
{
	if( XEventsQueued( win_man->display , QueuedAfterReading))
	{
		XEvent  event ;
		int  counter ;

		do
		{
			XNextEvent( win_man->display , &event) ;

			if( ! XFilterEvent( &event , None))
			{
				FOREACH_ROOTS(win_man,counter)
				{
					ml_window_receive_event( win_man->roots[counter] , &event) ;
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
