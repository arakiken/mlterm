/*
 *	$Id$
 */

#include  "x_display.h"

#include  <string.h>		/* memset/memcpy */
#include  <X11/Xproto.h>	/* X_OpenFont */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_file.h>	/* kik_file_set_cloexec */

#include  "x_window.h"
#include  "x_xim.h"
#include  "x_picture.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static u_int  num_of_displays ;
static x_display_t **  displays ;
static int (*default_error_handler)( Display * , XErrorEvent *) ;


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

	kik_msg_printf( "%s\n" , buffer) ;

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

#else	/* __DEBUG */

static int
error_handler(
	Display *  display ,
	XErrorEvent *  event
	)
{
#if  defined(BadValue) && defined(X_OpenFont)
	if( event->error_code == BadValue && event->request_code == X_OpenFont)
	{
		/*
		 * XXX Hack
		 * If BadValue error happens in XLoad(Query)Font function,
		 * mlterm doesn't stop.
		 */
		 
		kik_error_printf( KIK_DEBUG_TAG "XLoad(Query)Font function is failed.\n") ;

		/* ignored anyway */
		return  0 ;
	}
	else
#endif
	if( default_error_handler)
	{
		return  (*default_error_handler)( display , event) ;
	}
	else
	{
		exit( 1) ;

		return  1 ;
	}
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

#ifdef  __CYGWIN__

/*
 * For Cygwin X.
 */

#include  <windows.h>

static void
hide_console(void)
{
	HWND  conwin ;
	char  app_name[40] ;

	sprintf( app_name, "mlterm%08x", (unsigned int)GetCurrentThreadId()) ;

	LockWindowUpdate( GetDesktopWindow()) ;
	
	SetConsoleTitle( app_name) ;
	while( ( conwin = FindWindow( NULL, app_name)) == NULL)
	{
		Sleep( 40) ;
	}
	ShowWindowAsync( conwin, SW_HIDE);
	LockWindowUpdate( NULL) ;
}

#endif	/* __CYGWIN__ */

static x_display_t *
open_display(
	char *  name
	)
{
	x_display_t *  disp ;

	if( ( disp = malloc( sizeof( x_display_t))) == NULL)
	{
		return  NULL ;
	}
	
	if( ( disp->display = XOpenDisplay( name)) == NULL)
	{
		kik_msg_printf( " display %s couldn't be opened.\n" , name) ;

		goto  error1 ;
	}

	/* set close-on-exec flag on the socket connected to X. */
	kik_file_set_cloexec( XConnectionNumber( disp->display));

	if( ( disp->name = strdup( name)) == NULL)
	{
		goto  error2 ;
	}
	
	disp->screen = DefaultScreen( disp->display) ;
	disp->my_window = DefaultRootWindow( disp->display) ;

	if( ( disp->gc = x_gc_new( disp->display)) == NULL)
	{
		goto  error3 ;
	}
	
	disp->group_leader = XCreateSimpleWindow( disp->display,
						     disp->my_window,
						     0, 0, 1, 1, 0, 0, 0) ;
	disp->icon_path = NULL;
	disp->icon = None ;
	disp->mask = None ;
	disp->cardinal = NULL ;

	disp->roots = NULL ;
	disp->num_of_roots = 0 ;

	disp->selection_owner = NULL ;

	modmap_init( disp->display, &(disp->modmap)) ;

	memset( disp->cursors , 0 , sizeof( disp->cursors)) ;

	default_error_handler = XSetErrorHandler( error_handler) ;
#ifdef  __DEBUG
	XSetIOErrorHandler( ioerror_handler) ;
#endif

	x_xim_display_opened( disp->display) ;
	x_picture_display_opened( disp->display) ;
	
#ifdef  DEBUG
	kik_debug_printf( "X connection opened.\n") ;
#endif

	return  disp ;

error3:
	free( disp->name) ;

error2:
	XCloseDisplay( disp->display) ;

error1:
	free( disp) ;

	return  NULL ;
}

static int
close_display(
	x_display_t *  disp
	)
{
	int  count ;

	free( disp->name) ;

	x_gc_delete( disp->gc) ;

	modmap_final( &(disp->modmap)) ;

	if(  disp->group_leader)
	{
		XDestroyWindow( disp->display, disp->group_leader) ;
	}

	free(  disp->icon_path) ;

	if( disp->icon)
	{
		XFreePixmap( disp->display, disp->icon) ;
	}
	if( disp->mask)
	{
		XFreePixmap( disp->display, disp->mask) ;
	}

	free( disp->cardinal) ;

	for( count = 0 ; count < (sizeof(disp->cursors)/sizeof(disp->cursors[0])) ; count++)
	{
		if( disp->cursors[count])
		{
			XFreeCursor( disp->display , disp->cursors[count]) ;
		}
	}

	for( count = 0 ; count < disp->num_of_roots ; count ++)
	{
		x_window_unmap( disp->roots[count]) ;
		x_window_final( disp->roots[count]) ;
	}

	free( disp->roots) ;

	x_xim_display_closed( disp->display) ;
	x_picture_display_closed( disp->display) ;
	XCloseDisplay( disp->display) ;
	
	free( disp) ;

	return  1 ;
}


/* --- global functions --- */

x_display_t *
x_display_open(
	char *  disp_name
	)
{
	int  count ;
	x_display_t *  disp ;
	void *  p ;

	for( count = 0 ; count < num_of_displays ; count ++)
	{
		if( strcmp( displays[count]->name , disp_name) == 0)
		{
			return  displays[count] ;
		}
	}

	if( ( disp = open_display( disp_name)) == NULL)
	{
		return  NULL ;
	}

	if( ( p = realloc( displays , sizeof( x_display_t*) * (num_of_displays + 1))) == NULL)
	{
		x_display_close( disp) ;

		return  NULL ;
	}

	displays = p ;
	displays[num_of_displays ++] = disp ;

#ifdef  __CYGWIN__
	/*
	 * For Cygwin X
	 */
	hide_console() ;
#endif

	return  disp ;
}

int
x_display_close(
	x_display_t *  disp
	)
{
	int  count ;

	for( count = 0 ; count < num_of_displays ; count ++)
	{
		if( displays[count] == disp)
		{
			close_display( displays[count]) ;
			displays[count] = displays[-- num_of_displays] ;

		#ifdef  DEBUG
			kik_debug_printf( "X connection closed.\n") ;
		#endif

			return  1 ;
		}
	}
	
	return  0 ;
}

int
x_display_close_all(void)
{
	while( num_of_displays >0 )
	{
		close_display( displays[-- num_of_displays]) ;
	}

	free( displays) ;

	displays = NULL ;

	return  1 ;
}

x_display_t **
x_get_opened_displays(
	u_int *  num
	)
{
	*num = num_of_displays ;

	return  displays ;
}

int
x_display_fd(
	x_display_t *  disp
	)
{
	return  XConnectionNumber( disp->display) ;
}

int
x_display_show_root(
	x_display_t *  disp ,
	x_window_t *  root ,
	int  x ,
	int  y ,
	int  hint ,
	char *  app_name
	)
{
	void *  p ;

	if( ( p = realloc( disp->roots ,
			sizeof( x_window_t*) * (disp->num_of_roots + 1))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " realloc failed.\n") ;
	#endif

		return  0 ;
	}

	disp->roots = p ;

	root->disp = disp ;
	root->parent = NULL ;
	root->parent_window = disp->my_window ;
	root->gc = disp->gc ;
	root->x = x ;
	root->y = y ;

	if( app_name)
	{
		root->app_name = app_name ;
	}

	disp->roots[disp->num_of_roots++] = root ;

	x_window_show( root , hint) ;

	return  1 ;
}

int
x_display_remove_root(
	x_display_t *  disp ,
	x_window_t *  root
	)
{
	int  count ;

	for( count = 0 ; count < disp->num_of_roots ; count ++)
	{
		if( disp->roots[count] == root)
		{
			x_window_unmap( disp->roots[count]) ;
			x_window_final( root) ;

			disp->num_of_roots -- ;

			if( count == disp->num_of_roots)
			{
				memset( &disp->roots[count] , 0 , sizeof( disp->roots[0])) ;
			}
			else
			{
				memcpy( &disp->roots[count] ,
					&disp->roots[disp->num_of_roots] ,
					sizeof( disp->roots[0])) ;
			}

			return  1 ;
		}
	}

	return  0 ;
}

void
x_display_idling(
	x_display_t *  disp
	)
{
	int  count ;

	for( count = 0 ; count < disp->num_of_roots ; count ++)
	{
		x_window_idling( disp->roots[count]) ;
	}
}

int
x_display_receive_next_event(
	x_display_t *  disp
	)
{
	if( XEventsQueued( disp->display , QueuedAfterReading))
	{
		XEvent  event ;
		int  count ;

		do
		{
			XNextEvent( disp->display , &event) ;

			if( ! XFilterEvent( &event , None))
			{
				for( count = 0 ; count < disp->num_of_roots ; count ++)
				{
					x_window_receive_event( disp->roots[count] , &event) ;
				}
			}
		}
		while( XEventsQueued( disp->display , QueuedAfterReading)) ;
	}

	/*
	 * If XEventsQueued() return 0, this should be done because events
	 * should be flushed before waiting in select().
	 */
	XFlush( disp->display) ;
	
	return  1 ;
}


/*
 * Folloing functions called from x_window.c
 */
 
int
x_display_own_selection(
	x_display_t *  disp ,
	x_window_t *  win
	)
{
	if( disp->selection_owner)
	{
		x_display_clear_selection( disp , disp->selection_owner) ;
	}

	disp->selection_owner = win ;

	return  1 ;
}

int
x_display_clear_selection(
	x_display_t *  disp ,
	x_window_t *  win
	)
{
	if( disp->selection_owner == NULL || disp->selection_owner != win)
	{
		return  0 ;
	}

	disp->selection_owner->is_sel_owner = 0 ;

	if( disp->selection_owner->selection_cleared)
	{
		(*disp->selection_owner->selection_cleared)( disp->selection_owner) ;
	}

	disp->selection_owner = NULL ;

	return  1 ;
}


XID
x_display_get_group(
	x_display_t *  disp
	)
{
	return  disp->group_leader ;
}


XModifierKeymap *
x_display_get_modifier_mapping(
	x_display_t *  disp
	)
{
	return  disp->modmap.map ;
}

void
x_display_update_modifier_mapping(
	x_display_t *  disp ,
	u_int  serial
	)
{
	if( serial != disp->modmap.serial)
	{
		modmap_final( &(disp->modmap)) ;
		disp->modmap.map = XGetModifierMapping( disp->display) ;
		disp->modmap.serial = serial ;
	}
}

Cursor
x_display_get_cursor(
	x_display_t *  disp ,
	u_int  shape
	)
{
	int  idx ;
	
	/*
	 * XXX
	 * cursor[0] == XC_xterm / cursor[1] == XC_sb_v_double_arrow / cursor[2] == XC_left_ptr
	 * Mlterm uses only these shapes.
	 */
	 
	if( shape == XC_xterm)
	{
		idx = 0 ;
	}
	else if( shape == XC_sb_v_double_arrow)
	{
		idx = 1 ;
	}
	else if( shape == XC_left_ptr)
	{
		idx = 2 ;
	}
	else
	{
		return  None ;
	}
	
	if( ! disp->cursors[idx])
	{
		disp->cursors[idx] = XCreateFontCursor( disp->display , shape) ;
	}

	return  disp->cursors[idx] ;
}

int
x_display_set_icon(
	x_window_t *  win ,
	char *  icon_path
	)
{
	x_display_t *  disp ;

	disp = win->disp ;

	if( !icon_path && !disp->icon_path)
	{
		/* dont't need icon at all? */
		return  0 ;
	}

	if( !disp->icon_path)
	{
		x_window_t  dummy = {NULL};

		/* register the default icon */
		if(!(disp->icon_path = strdup( icon_path)))
		{
			return  0 ;
		}
		dummy.my_window = disp->group_leader ;
		dummy.disp = disp ;
		dummy.icon_pix = None ;
		dummy.icon_mask = None ;
		dummy.icon_card = NULL ;
		x_window_set_icon_from_file( &dummy, icon_path);

		disp->icon = dummy.icon_pix ;
		disp->mask = dummy.icon_mask ;
		disp->cardinal = dummy.icon_card ;

	}

	if( !icon_path || strcmp( icon_path, disp->icon_path) ==0)
	{
		x_window_remove_icon( win) ;
		/* use a default icon from window manager */
		return x_window_set_icon( win,
					  disp->icon,
					  disp->mask,
					  disp->cardinal) ;
	}
	else
	{
		/* load new icon from "icon_path" */
		return x_window_set_icon_from_file( win, icon_path);
	}
}
