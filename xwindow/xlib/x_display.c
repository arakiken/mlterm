/*
 *	$Id$
 */

#include  "../x_display.h"

#include  <string.h>		/* memset/memcpy */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_file.h>	/* kik_file_set_cloexec */

#include  "../x_window.h"
#include  "../x_picture.h"
#include  "../x_imagelib.h"

#include  "x_xim.h"


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
	/*
	 * If <X11/Xproto.h> is included for 'X_OpenFont', typedef of BOOL and INT32
	 * is conflicted in <X11/Xmd.h>(included from <X11/Xproto.h> and
	 * <w32api/basetsd.h>(included from <windows.h>).
	 */
	if( event->error_code == 2 /* BadValue */
		&& event->request_code == 45 /* X_OpenFont */)
	{
		/*
		 * XXX Hack
		 * If BadValue error happens in XLoad(Query)Font function,
		 * mlterm doesn't stop.
		 */
		 
		kik_msg_printf( "XLoad(Query)Font function is failed.\n") ;

		/* ignored anyway */
		return  0 ;
	}
	else if( default_error_handler)
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

#ifdef  __CYGWIN__

/*
 * For Cygwin X.
 */

/*
 * NOGDI prevents <windows.h> from including <winspool.h> which uses
 * variables named 'Status' which is defined as int in Xlib.h.
 */
#define  NOGDI
#include  <windows.h>
#undef  NOGDI

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
	char *  name ,
	u_int  depth
	)
{
	x_display_t *  disp ;
	XVisualInfo  vinfo ;

	if( ( disp = calloc( 1 , sizeof( x_display_t))) == NULL)
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
	disp->width = DisplayWidth( disp->display , disp->screen) ;
	disp->height = DisplayHeight( disp->display , disp->screen) ;

	if( depth &&
	    XMatchVisualInfo( disp->display , disp->screen , depth , TrueColor , &vinfo) &&
	    vinfo.visual != DefaultVisual( disp->display , disp->screen) )
	{
		XSetWindowAttributes  s_attr ;
		Window  win ;

		disp->depth = depth ;
		disp->visual = vinfo.visual ;
		disp->colormap = XCreateColormap( disp->display ,
					disp->my_window , vinfo.visual , AllocNone) ;

		s_attr.background_pixel = BlackPixel(disp->display,disp->screen) ;
		s_attr.border_pixel = BlackPixel(disp->display,disp->screen) ;
		s_attr.colormap = disp->colormap ;
		win = XCreateWindow( disp->display , disp->my_window ,
				0 , 0 , 1 , 1 , 0 , disp->depth , InputOutput , disp->visual ,
				CWColormap | CWBackPixel | CWBorderPixel , &s_attr) ;
		if( ( disp->gc = x_gc_new( disp->display , win)) == NULL)
		{
			goto  error3 ;
		}
		XDestroyWindow( disp->display , win) ;
	}
	else
	{
		disp->depth = DefaultDepth( disp->display , disp->screen) ;
		disp->visual = DefaultVisual( disp->display , disp->screen) ;
		disp->colormap = DefaultColormap( disp->display , disp->screen) ;
		if( ( disp->gc = x_gc_new( disp->display , None)) == NULL)
		{
			goto  error3 ;
		}
	}

	disp->modmap.map = XGetModifierMapping( disp->display) ;

	default_error_handler = XSetErrorHandler( error_handler) ;
#ifdef  __DEBUG
	XSetIOErrorHandler( ioerror_handler) ;
	XSynchronize( disp->display , True) ;
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

	if( disp->modmap.map)
	{
		XFreeModifiermap( disp->modmap.map);
	}

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
	char *  disp_name ,
	u_int  depth
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

	if( ( disp = open_display( disp_name , depth)) == NULL)
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
	while( num_of_displays > 0)
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
	char *  app_name ,
	Window  parent_window
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

	if( parent_window)
	{
		root->parent_window = parent_window ;
	}
	else
	{
		root->parent_window = disp->my_window ;
	}

	root->gc = disp->gc ;
	root->x = x ;
	root->y = y ;

	if( app_name)
	{
		root->app_name = app_name ;
	}

	/*
	 * root is added to disp->roots before x_window_show() because
	 * x_display_get_group_leader() is called in x_window_show().
	 */
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
	u_int  count ;

	for( count = 0 ; count < disp->num_of_roots ; count ++)
	{
		if( disp->roots[count] == root)
		{
			/* Don't switching on or off screen in exiting. */
			x_window_unmap( root) ;

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
				
				if( count == 0)
				{
					/* Group leader is changed. */

				#if  0
					kik_debug_printf( KIK_DEBUG_TAG
						" Changing group_leader -> %x\n" ,
						disp->roots[0]->my_window) ;
				#endif
				
					for( count = 0 ; count < disp->num_of_roots ; count++)
					{
						x_window_reset_group( disp->roots[count]) ;
					}
				}
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
	x_display_t *  disp ,	/* NULL means all selection owner windows. */
	x_window_t *  win
	)
{
	if( disp == NULL)
	{
		u_int  count ;

		for( count = 0 ; count < num_of_displays ; count++)
		{
			x_display_clear_selection( displays[count] ,
				displays[count]->selection_owner) ;
		}

		return  1 ;
	}

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
		if( disp->modmap.map)
		{
			XFreeModifiermap( disp->modmap.map);
		}

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

/*
 * Called from x_imagelib.c alone.
 */
XVisualInfo *
x_display_get_visual_info(
	x_display_t *  disp
	)
{
	XVisualInfo  vinfo_template ;
	int  nitems ;

	vinfo_template.visualid = XVisualIDFromVisual( disp->visual) ;

	/* Return pointer to the first element of VisualInfo list. */
	return  XGetVisualInfo( disp->display , VisualIDMask , &vinfo_template , &nitems) ;
}

XID
x_display_get_group_leader(
	x_display_t *  disp
	)
{
	if( disp->num_of_roots > 0)
	{
		return  disp->roots[0]->my_window ;
	}
	else
	{
		return  None ;
	}
}
