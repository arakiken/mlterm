/*
 *	$Id$
 */

#include  "x_display.h"

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_str.h>	/* strdup */

#include  "x_xim.h"
#include  "x_picture.h"


/* --- global functions --- */

x_display_t *
x_display_open(
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

	if( ( disp->name = strdup( name)) == NULL)
	{
		goto  error2 ;
	}
	
	if( ! x_window_manager_init( &disp->win_man , disp->display))
	{
		goto  error3 ;
	}

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

int
x_display_close(
	x_display_t *  disp
	)
{
	free( disp->name) ;
	x_window_manager_final( &disp->win_man) ;
	x_xim_display_closed( disp->display) ;
	x_picture_display_closed( disp->display) ;
	XCloseDisplay( disp->display) ;
	
	free( disp) ;

	return  1 ;
}

int
x_display_fd(
	x_display_t *  disp
	)
{
	return  XConnectionNumber( disp->display) ;
}
