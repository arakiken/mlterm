/*
 *	$Id$
 */

#include  "mc_logsize.h"

#include  <kiklib/kik_str.h>
#include  <stdlib.h>		/* free */
#include  <kiklib/kik_debug.h>
#include  <glib.h>
#include  <c_intl.h>

#include  "mc_combo.h"
#include  "mc_io.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  new_logsize = NULL;
static char *  old_logsize = NULL;
static int is_changed;


/* --- static functions --- */

static gint
logsize_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	free( new_logsize);
	new_logsize = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1) ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s logsize is selected.\n" , new_logsize) ;
#endif

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_logsize_config_widget_new(void)
{
	char *  logsizes[] =
	{
		"128" ,
		"256" ,
		"512" ,
		"1024" ,
	} ;

	new_logsize = strdup( old_logsize = mc_get_str_value( "logsize")) ;
	is_changed = 0;

	return  mc_combo_new_with_width(_("Backlog size (lines)"), logsizes,
		sizeof(logsizes) / sizeof(logsizes[0]),
		new_logsize, 0, logsize_selected, NULL, 80);
}

void
mc_update_logsize(void)
{
	if (strcmp(new_logsize, old_logsize)) is_changed = 1;

	if (is_changed)
	{
		mc_set_str_value( "logsize" , new_logsize) ;
		free( old_logsize) ;
		old_logsize = strdup( new_logsize) ;
	}
}
