/*
 *	$Id$
 */

#include  "x_window_manager.h"

#include  <string.h>		/* memset/memcpy */
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

static void
modmap_init(
	Display *  display ,
	x_modifier_mapping_t *  modmap
	)
{
	modmap->serial = 0 ;
	modmap->map = XGetModifierMapping( display) ;
}

static void
modmap_final(
	x_modifier_mapping_t *  modmap
	)
{
	if( modmap->map)
	{
		XFreeModifiermap( modmap->map);
	}
}


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

	if( ( win_man->gc = x_gc_new( display)) == NULL)
	{
		return  0 ;
	}
	
	win_man->group_leader = XCreateSimpleWindow( win_man->display,
						     win_man->my_window,
						     0, 0, 1, 1, 0, 0, 0) ;
	win_man->icon_path = NULL;
	win_man->icon = None ;
	win_man->mask = None ;
	win_man->cardinal = NULL ;

	win_man->roots = NULL ;
	win_man->num_of_roots = 0 ;

	win_man->selection_owner = NULL ;

	modmap_init( display, &(win_man->modmap)) ;

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

	x_gc_delete( win_man->gc) ;

	modmap_final( &(win_man->modmap)) ;

	if(  win_man->group_leader)
	{
		XDestroyWindow( win_man->display, win_man->group_leader) ;
	}

	free(  win_man->icon_path) ;

	if( win_man->icon)
	{
		XFreePixmap( win_man->display, win_man->icon) ;
	}
	if( win_man->mask)
	{
		XFreePixmap( win_man->display, win_man->mask) ;
	}

	free( win_man->cardinal) ;

	for( count = 0 ; count < win_man->num_of_roots ; count ++)
	{
		x_window_unmap( win_man->roots[count]) ;
		x_window_final( win_man->roots[count]) ;
	}

	free( win_man->roots) ;

	return  1 ;
}

int
x_window_manager_show_root(
	x_window_manager_t *  win_man ,
	x_window_t *  root ,
	int  x ,
	int  y ,
	int  hint ,
	char *  app_name
	)
{
	void *  p ;

	if( ( p = realloc( win_man->roots ,
			sizeof( x_window_t*) * (win_man->num_of_roots + 1))) == NULL)
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
	root->gc = win_man->gc ;
	root->x = x ;
	root->y = y ;

	if( app_name)
	{
		root->app_name = app_name ;
	}

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


/*
 * Folloing functions called from x_window.c
 */
 
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

	win_man->selection_owner->is_sel_owner = 0 ;

	if( win_man->selection_owner->selection_cleared)
	{
		(*win_man->selection_owner->selection_cleared)( win_man->selection_owner) ;
	}

	win_man->selection_owner = NULL ;

	return  1 ;
}


XID
x_window_manager_get_group(
	x_window_manager_t *  win_man
	)
{
	return  win_man->group_leader ;
}


XModifierKeymap *
x_window_manager_get_modifier_mapping(
	x_window_manager_t *  win_man
	)
{
	return  win_man->modmap.map ;
}

void
x_window_manager_update_modifier_mapping(
	x_window_manager_t *  win_man ,
	u_int  serial
	)
{
	if( serial != win_man->modmap.serial)
	{
		modmap_final( &(win_man->modmap)) ;
		win_man->modmap.map = XGetModifierMapping( win_man->display) ;
		win_man->modmap.serial = serial ;
	}
}

int x_window_manager_set_icon(
	x_window_t *  win,
	char *  icon_path
	)
{
	x_window_manager_t *  win_man ;

	win_man = win->win_man ;

	if( !icon_path && !win_man->icon_path)
	{
		/* dont't need icon at all? */
		return  0 ;
	}

	if( !win_man->icon_path)
	{
		x_window_t  dummy = {NULL};

		/* register the default icon */
		if(!(win_man->icon_path = strdup( icon_path)))
		{
			return  0 ;
		}
		dummy.my_window = win_man->group_leader ;
		dummy.display = win_man->display ;
		dummy.icon_pix = None ;
		dummy.icon_mask = None ;
		dummy.icon_card = NULL ;
		x_window_set_icon_from_file( &dummy, icon_path);

		win_man->icon = dummy.icon_pix ;
		win_man->mask = dummy.icon_mask ;
		win_man->cardinal = dummy.icon_card ;

	}

	if( !icon_path || strcmp( icon_path, win_man->icon_path) ==0)
	{
		x_window_remove_icon( win) ;
		/* use a default icon from window manager */
		return x_window_set_icon( win,
					  win_man->icon,
					  win_man->mask,
					  win_man->cardinal) ;
	}
	else
	{
		/* load new icon from "icon_path" */
		return x_window_set_icon_from_file( win, icon_path);
	}
}
