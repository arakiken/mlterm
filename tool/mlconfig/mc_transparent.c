/*
 *	$Id$
 */

#include  "mc_transparent.h"

#include  <kiklib/kik_debug.h>
#include  <glib.h>

#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static int  is_checked = 0 ;


/* --- static functions --- */

static gint
button_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	is_checked = GTK_TOGGLE_BUTTON(widget)->active ;
	
#ifdef  __DEBUG
	if( is_checked)
	{
		kik_debug_printf( KIK_DEBUG_TAG " transparent on.\n") ;
	}
	else
	{
		kik_debug_printf( KIK_DEBUG_TAG " transparent off.\n") ;
	}
#endif

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_transparent_config_widget_new(
	int  is_checked_default
	)
{
	GtkWidget *  check ;

	is_checked = is_checked_default ;

	check = gtk_check_button_new_with_label( "transpa") ;

	if( is_checked)
	{
		gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(check) , TRUE) ;
	}
	
	gtk_signal_connect(GTK_OBJECT(check) , "toggled" , GTK_SIGNAL_FUNC(button_checked) , NULL) ;

	return  check ;
}

int
mc_is_transparent(void)
{
	return  is_checked ;
}
