/*
 *	$Id$
 */

#include  "mc_logsize.h"

#include  <kiklib/kik_debug.h>
#include  <glib.h>
#include  <c_intl.h>

#include  "mc_combo.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  selected_logsize ;
static int  is_changed ;


/* --- static functions --- */

static gint
logsize_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	selected_logsize = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	is_changed = 1 ;
	
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
	char *  logsizes[] =
	{
		"128" ,
		"256" ,
		"512" ,
		"1024" ,
	} ;

	selected_logsize = logsize ;

	return  mc_combo_new_with_width(_("Backlog size (lines)"), logsizes,
		sizeof(logsizes) / sizeof(logsizes[0]),
		selected_logsize, 0, logsize_selected, NULL, 80);
}

char *
mc_get_logsize(void)
{
	if( ! is_changed)
	{
		return  NULL ;
	}
	
	is_changed = 0 ;
	
	return  selected_logsize ;
}
