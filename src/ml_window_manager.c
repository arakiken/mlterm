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


#define  MAX_ROOTS(win_man)  (sizeof((win_man)->roots) / sizeof((win_man)->roots[0]))

#define  FOREACH_ROOTS(win_man,counter) \
	for( (counter) = 0 ; (counter) < (win_man)->num_of_roots ; (counter) ++)


/* --- global functions --- */

int
ml_window_manager_init(
	ml_window_manager_t *  win_man ,
	char *  disp_name
	)
{
	if( ( win_man->display = XOpenDisplay( disp_name)) == NULL)
	{
		kik_msg_printf( " display %s couldn't be opened.\n" , disp_name) ;
		
		return  0 ;
	}
	
	win_man->screen = DefaultScreen( win_man->display) ;
	win_man->my_window = DefaultRootWindow( win_man->display) ;

	memset( &win_man->roots , 0 , sizeof( win_man->roots)) ;
	win_man->num_of_roots = 0 ;

	win_man->selection_owner = NULL ;
	
	ml_window_init_atom( win_man->display) ;

	ml_xim_init( win_man->display) ;
	
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
		ml_window_final( win_man->roots[counter].root) ;
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
	int  y
	)
{
	if( win_man->num_of_roots == MAX_ROOTS(win_man))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " full of root windows.\n") ;
	#endif
	
		return  0 ;
	}

	root->win_man = win_man ;
	root->parent = NULL ;
	root->parent_window = win_man->my_window ;
	root->display = win_man->display ;
	root->screen = win_man->screen ;
	
	win_man->roots[win_man->num_of_roots].root = root ;
	win_man->roots[win_man->num_of_roots].x = x ;
	win_man->roots[win_man->num_of_roots].y = y ;

	win_man->num_of_roots ++ ;

	ml_window_show( root , x , y) ;
	
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
		if( win_man->roots[counter].root == root)
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
		ml_window_idling( win_man->roots[counter].root) ;
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
					ml_window_receive_event( win_man->roots[counter].root , &event) ;
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
