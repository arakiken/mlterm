/*
 *	update: <2001/11/26(22:50:41)>
 *	$Id$
 */

#include  "mc_aa.h"

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
		kik_debug_printf( KIK_DEBUG_TAG " aa on.\n") ;
	}
	else
	{
		kik_debug_printf( KIK_DEBUG_TAG " aa off.\n") ;
	}
#endif

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_aa_config_widget_new(
	int  is_checked_default
	)
{
	GtkWidget *  check ;

	is_checked = is_checked_default ;

	check = gtk_check_button_new_with_label( "antialias") ;

	if( is_checked)
	{
		gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(check) , TRUE) ;
	}
	
	gtk_signal_connect(GTK_OBJECT(check) , "toggled" , GTK_SIGNAL_FUNC(button_checked) , NULL) ;

	return  check ;
}

int
mc_is_aa(void)
{
	return  is_checked ;
}
