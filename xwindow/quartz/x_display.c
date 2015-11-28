/*
 *	$Id$
 */

#include  "../x_display.h"

#include  <stdio.h>		/* sprintf */
#include  <string.h>		/* memset/memcpy */
#include  <kiklib/kik_def.h>	/* USE_WIN32API */
#include  <kiklib/kik_file.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_dialog.h>

#include  "x.h"
#include  "../x_window.h"
#include  "../x_picture.h"
#include  "cocoa.h"


#define  DISP_IS_INITED   (_disp.display)


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static x_display_t  _disp ;	/* Singleton */
static Display  _display ;
static x_display_t *  opened_disp = &_disp ;


/* --- static functions --- */

static int
dialog(
	kik_dialog_style_t  style ,
	char *  msg
	)
{
#if  0
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
#endif

	return  0 ;
}


/* --- global functions --- */

x_display_t *
x_display_open(
	char *  disp_name ,	/* Ignored */
	u_int  depth		/* Ignored */
	)
{
	if( DISP_IS_INITED)
	{
		/* Already opened. */
		return  &_disp ;
	}

	/* Callback should be set before kik_dialog() is called. */
	kik_dialog_set_callback( dialog) ;

#if  0
	_disp.width = GetSystemMetrics( SM_CXSCREEN) ;
	_disp.height = GetSystemMetrics( SM_CYSCREEN) ;
#endif
	_disp.depth = 32 ;

	/* _disp is initialized successfully. */
	_display.fd = -1 ;
	_disp.display = &_display ;

	return  &_disp ;
}

int
x_display_close(
	x_display_t *  disp
	)
{
	if( disp == &_disp)
	{
		return  x_display_close_all() ;
	}
	else
	{
		return  0 ;
	}
}

int
x_display_close_all(void)
{
	u_int  count ;

	if( ! DISP_IS_INITED)
	{
		return  0 ;
	}

	x_picture_display_closed( _disp.display) ;

	x_gc_delete( _disp.gc) ;

	for( count = 0 ; count < _disp.num_of_roots ; count ++)
	{
		x_window_unmap( _disp.roots[count]) ;
		x_window_final( _disp.roots[count]) ;
	}

	free( _disp.roots) ;

	for( count = 0 ; count < (sizeof(_disp.cursors)/sizeof(_disp.cursors[0])) ; count++)
	{
		if( _disp.cursors[count])
		{
		#if  0
			CloseHandle( _disp.cursors[count]) ;
		#endif
		}
	}

	_disp.display = NULL ;

	return  1 ;
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

	*num = 1 ;

	return  &opened_disp ;
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

	if( x_window_show( root , hint))
	{
		if( disp->num_of_roots > 1)
		{
			window_alloc( root) ;
		}

		return  1 ;
	}
	else
	{
		return  0 ;
	}
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
	u_int  count ;

	for( count = 0 ; count < _disp.num_of_roots ; count ++)
	{
		x_window_idling( _disp.roots[count]) ;
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

Cursor
x_display_get_cursor(
	x_display_t *  disp ,
	u_int  shape
	)
{
#if  0
	int  idx ;
	LPCTSTR  name ;

	/*
	 * XXX
	 * cursor[0] == XC_xterm / cursor[1] == XC_left_ptr / cursor[2] == not used
	 * Mlterm uses only these shapes.
	 */

	if( shape == XC_xterm)
	{
		idx = 0 ;
		name = IDC_IBEAM ;
	}
	else if( shape == XC_left_ptr)
	{
		idx = 1 ;
		name = IDC_ARROW ;	/* already loaded in x_display_open() */
	}
	else
	{
		return  None ;
	}

	if( ! disp->cursors[idx])
	{
		disp->cursors[idx] = LoadCursor( NULL , name) ;
	}

	return  disp->cursors[idx] ;
#else
	return  None ;
#endif
}

XID
x_display_get_group_leader(
	x_display_t *  disp
	)
{
	return  None ;
}
