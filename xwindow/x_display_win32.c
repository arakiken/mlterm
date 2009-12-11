/*
 *	$Id$
 */

#include  "x_display.h"

#include  <stdio.h>		/* sprintf */
#include  <string.h>		/* memset/memcpy */
#ifndef  USE_WIN32API
#include  <fcntl.h>		/* open */
#endif
#include  <unistd.h>		/* close */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>

#include  "x_window.h"
#include  "x_gdiobj_pool.h"


#define  DISP_IS_INITED   (_disp.display)


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static x_display_t  _disp ;	/* Singleton */
static Display  _display ;

static x_display_t *  _opened_disp ;	/* for x_get_opened_displays */


/* --- static functions --- */

static LRESULT CALLBACK window_proc(
  	HWND hwnd,
  	UINT msg,
  	WPARAM wparam,
  	LPARAM lparam
  	)
{
	XEvent  event ;
	int  count ;

	event.window = hwnd ;
	event.msg = msg ;
	event.wparam = wparam ;
	event.lparam = lparam ;

	for( count = 0 ; count < _disp.num_of_roots ; count ++)
	{
		int  val ;

		val = x_window_receive_event( _disp.roots[count] , &event) ;
		if( val == 1)
		{
			return  0 ;
		}
		else if( val == -1)
		{
			break ;
		}
	}

	return  DefWindowProc( hwnd, msg, wparam, lparam) ;
}

#ifndef  USE_WIN32API
static void
hide_console(void)
{
	HWND  conwin ;
	char  app_name[40] ;

	sprintf( app_name, "mlterm%08x", (unsigned int)GetCurrentThreadId()) ;

	LockWindowUpdate( GetDesktopWindow()) ;
	
/*
 * "-Wl,-subsystem,console" is specified in xwindow/Makefile, so AllocConsole()
 * is not necessary.
 */
#if  0
	AllocConsole() ;
#endif

	SetConsoleTitle( app_name) ;
	while( ( conwin = FindWindow( NULL, app_name)) == NULL)
	{
		Sleep( 40) ;
	}
	ShowWindowAsync( conwin, SW_HIDE);
	LockWindowUpdate( NULL) ;
}
#endif


/* --- global functions --- */

x_display_t *
x_display_open(
	char *  name
	)
{
	WNDCLASS  wc ;
	int  fd ;

	if( DISP_IS_INITED)
	{
		/* Already opened. */
		return  &_disp ;
	}

	_display.hinst = GetModuleHandle(NULL) ;
	
  	/* Prepare window class */
  	ZeroMemory( &wc , sizeof(WNDCLASS)) ;
  	wc.lpfnWndProc = window_proc ;
	wc.style = CS_HREDRAW | CS_VREDRAW ;
  	wc.hInstance = _display.hinst ;
  	wc.hIcon = 0 ;
  	wc.hCursor = LoadCursor(NULL,IDC_ARROW) ;
  	wc.hbrBackground = 0 ;
  	wc.lpszClassName = "MLTERM" ;

	if( ! RegisterClass(&wc))
	{
		MessageBox(NULL,"Failed to register class", NULL, MB_ICONSTOP) ;

		return  NULL ;
	}

	_disp.screen = 0 ;
	_disp.my_window = None ;
	_disp.group_leader = None ;

	_disp.icon_path = NULL;
	_disp.icon = None ;
	_disp.mask = None ;
	_disp.cardinal = NULL ;
 
	_disp.roots = NULL ;
	_disp.num_of_roots = 0 ;

	_disp.selection_owner = NULL ;

#ifdef  USE_WIN32API
	fd = -1 ;
#else
	if( ( fd = open( "/dev/windows" , O_NONBLOCK , 0)) == -1)
	{
		return  NULL ;
	}
#endif

	if( ( _disp.gc = x_gc_new( &_display)) == NULL)
	{
		return  NULL ;
	}

	memset( &_disp.modmap , 0 , sizeof( _disp.modmap)) ;
	
	x_gdiobj_pool_init() ;

#ifndef  USE_WIN32API
	hide_console() ;
#endif

	/* _disp is initialized successfully. */
	_display.fd = fd ;
	_disp.display = &_display ;

	return  &_disp ;
}

int
x_display_close(
	x_display_t *  disp
	)
{
	int  count ;

	if( ! DISP_IS_INITED || disp != &_disp)
	{
		return  0 ;
	}

	x_gc_delete( disp->gc) ;

	free( disp->icon_path);

#ifndef  USE_WIN32GUI
	if( disp->icon)
	{
		XFreePixmap( disp->display, disp->icon) ;
	}
	if( disp->mask)
	{
		XFreePixmap( disp->display, disp->mask) ;
	}
#endif

	free( disp->cardinal) ;

	for( count = 0 ; count < disp->num_of_roots ; count ++)
	{
		x_window_unmap( disp->roots[count]) ;
		x_window_final( disp->roots[count]) ;
	}

	free( disp->roots) ;

	if( _disp.display->fd != -1)
	{
		close( _disp.display->fd) ;
	}

	_disp.display = NULL ;

	x_gdiobj_pool_final() ;

	return  1 ;
}

int
x_display_close_all(void)
{
	/* Not supported */
	return  0 ;
}

x_display_t **
x_get_opened_displays(
	u_int *  num
	)
{
	if( ! DISP_IS_INITED)
	{
		*num = 0 ;
		
		return  NULL ;
	}

	_opened_disp = &_disp ;
	*num = 1 ;

	return  &_opened_disp ;
}

int
x_display_fd(
	x_display_t *  disp
	)
{
	return  disp->display->fd ;
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

	if( ( p = realloc( disp->roots , sizeof( x_window_t*) * (disp->num_of_roots + 1))) == NULL)
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

	return  x_window_show( root , hint) ;
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

/*
 * <Return value>
 *  0: Receive WM_QUIT
 *  1: Receive other messages.
 */
int
x_display_receive_next_event(
	x_display_t *  disp
	)
{
	MSG  msg ;

#ifdef  USE_WIN32API
	/* 0: WM_QUIT, -1: Error */
	if( GetMessage( &msg , NULL , 0 , 0) <= 0)
	{
		return  0 ;
	}
	
	TranslateMessage( &msg) ;
	DispatchMessage( &msg) ;
#else
	while( PeekMessage( &msg , NULL , 0 , 0 , PM_REMOVE))
	{
		if( msg.message == WM_QUIT)
		{
			return  0 ;
		}
		
		TranslateMessage( &msg) ;
		DispatchMessage( &msg) ;
	}
#endif

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
	/* dummy */
}

int x_display_set_icon(
	x_window_t *  win,
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
