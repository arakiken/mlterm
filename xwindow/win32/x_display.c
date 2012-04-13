/*
 *	$Id$
 */

#include  "../x_display.h"

#include  <stdio.h>		/* sprintf */
#include  <string.h>		/* memset/memcpy */
#include  <kiklib/kik_def.h>	/* USE_WIN32API */
#ifndef  USE_WIN32API
#include  <fcntl.h>		/* open */
#endif
#include  <unistd.h>		/* close */
#include  <kiklib/kik_file.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_dialog.h>

#include  "../x_window.h"

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

static int
dialog(
	kik_dialog_style_t  style ,
	char *  msg
	)
{
	if( style == KIK_DIALOG_OKCANCEL)
	{
		if( MessageBoxA( NULL , msg , "" , MB_OKCANCEL) == IDOK)
		{
			return  1 ;
		}
	}
	else if( style == KIK_DIALOG_ALERT)
	{
		MessageBoxA( NULL , msg , "" , MB_ICONSTOP) ;
	}
	else
	{
		return  -1 ;
	}

	return  0 ;
}


/* --- global functions --- */

x_display_t *
x_display_open(
	char *  disp_name ,	/* Ignored */
	u_int  depth		/* Ignored */
	)
{
#ifndef  UTF16_IME_CHAR
	WNDCLASS  wc ;
#else
	WNDCLASSW  wc ;
#endif
	int  fd ;

	if( DISP_IS_INITED)
	{
		/* Already opened. */
		return  &_disp ;
	}

	/* Callback should be set before kik_dialog() is called. */
	kik_dialog_set_callback( dialog) ;

	_display.hinst = GetModuleHandle(NULL) ;

  	/* Prepare window class */
  	ZeroMemory( &wc , sizeof(WNDCLASS)) ;
  	wc.lpfnWndProc = window_proc ;
	wc.style = CS_HREDRAW | CS_VREDRAW ;
  	wc.hInstance = _display.hinst ;
  	wc.hIcon = LoadIcon( _display.hinst , "MLTERM_ICON") ;
  	wc.hCursor = LoadCursor(NULL,IDC_ARROW) ;
  	wc.hbrBackground = 0 ;
	wc.lpszClassName = __("MLTERM") ;

	if( ! RegisterClass(&wc))
	{
		kik_dialog( KIK_DIALOG_ALERT , "Failed to register class") ;

		return  NULL ;
	}

	_disp.width = GetSystemMetrics( SM_CXSCREEN) ;
	_disp.height = GetSystemMetrics( SM_CYSCREEN) ;
	_disp.depth = 24 ;

#ifdef  USE_WIN32API
	fd = -1 ;
#else
	if( ( fd = open( "/dev/windows" , O_NONBLOCK , 0)) == -1)
	{
		return  NULL ;
	}
	
	kik_file_set_cloexec( fd) ;
#endif

	if( ( _disp.gc = x_gc_new( &_display , None)) == NULL)
	{
		return  NULL ;
	}

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
	char *  app_name ,
	Window  parent_window	/* Ignored */
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
	
#endif	/* USE_WIN32API */

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

XID
x_display_get_group_leader(
	x_display_t *  disp
	)
{
	return  None ;
}
