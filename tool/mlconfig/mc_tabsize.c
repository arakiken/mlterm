/*
 *	$Id$
 */

#include  "mc_tabsize.h"

#include  <string.h>
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

static char *  new_tabsize ;
static char *  old_tabsize ;


/* --- static functions --- */

static gint
tabsize_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	new_tabsize = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s tabsize is selected.\n" , new_tabsize) ;
#endif

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_tabsize_config_widget_new(void)
{
	char *  tabsizes[] =
	{
		"8" ,
		"4" ,
		"2" ,
	} ;

	new_tabsize = old_tabsize = mc_get_str_value( "tabsize") ;

	return  mc_combo_new_with_width( _("Tab width (columns)"), tabsizes,
		sizeof(tabsizes) / sizeof(tabsizes[0]),
		new_tabsize, 0, tabsize_selected, NULL, 80);
}

void
mc_update_tabsize(
	int  save
	)
{
	if( save)
	{
		mc_set_str_value( "tabsize" , new_tabsize , save) ;
	}
	else
	{
		if( strcmp( new_tabsize , old_tabsize) != 0)
		{
			mc_set_str_value( "tabsize" , new_tabsize , save) ;
			free( old_tabsize) ;
			old_tabsize = strdup( new_tabsize) ;
		}
	}
}
