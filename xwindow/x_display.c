/*
 *	$Id$
 */

#include  "x_display.h"

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_str.h>	/* strdup */

#include  "x_xim.h"
#include  "x_picture.h"


/* --- static variables --- */

static u_int  num_of_displays ;
static x_display_t **  displays ;


/* --- static functions --- */

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

static int
close_display(
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
			close_display( disp) ;
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
	int  count ;
	
	for( count = 0 ; count < num_of_displays ; count ++)
	{
		x_display_close( displays[count]) ;
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
