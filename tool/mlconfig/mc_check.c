/*
 *	$Id$
 */

#include  "mc_check.h"

#include  <kiklib/kik_debug.h>
#include  <glib.h>


#if  0
#define  __DEBUG
#endif


/* --- global functions --- */

GtkWidget *
mc_check_config_widget_new(
	char *  label ,
	int  is_checked
	)
{
	GtkWidget *  check ;

	check = gtk_check_button_new_with_label( label) ;

	if( is_checked)
	{
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(check) , TRUE) ;
	}
	
	return  check ;
}
