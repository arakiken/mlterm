/*
 *	$Id$
 */

#include  "mc_logsize.h"

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>
#include  <glib.h>

#include  "mc_combo.h"

#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  logsizes[] =
{
	"128" ,
	"256" ,
	"512" ,
	"1024" ,
} ;

static char *  selected_logsize ;


/* --- static functions --- */

static gint
logsize_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	selected_logsize = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s logsize is selected.\n" , selected_logsize) ;
#endif

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_logsize_config_widget_new(
	char *  logsize
	)
{
	selected_logsize = logsize ;

	return  mc_combo_new( "Log size" , logsizes , sizeof(logsizes) / sizeof(logsizes[0]) ,
		selected_logsize , 0 , logsize_selected , NULL) ;
}

u_int
mc_get_logsize(void)
{
	u_int  logsize ;
	
	if( ! kik_str_to_uint( &logsize , selected_logsize))
	{
		kik_str_to_uint( &logsize , logsizes[0]) ;
	}
	
	return  logsize ;
}
