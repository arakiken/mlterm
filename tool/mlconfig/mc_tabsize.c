/*
 *	$Id$
 */

#include  "mc_tabsize.h"

#include  <kiklib/kik_debug.h>
#include  <glib.h>

#include  "mc_combo.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  selected_tabsize ;
static int  is_changed ;


/* --- static functions --- */

static gint
tabsize_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	selected_tabsize = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	is_changed = 1 ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s tabsize is selected.\n" , selected_tabsize) ;
#endif

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_tabsize_config_widget_new(
	char *  tabsize
	)
{
	char *  tabsizes[] =
	{
		"8" ,
		"4" ,
		"2" ,
	} ;

	selected_tabsize = tabsize ;

	return  mc_combo_new_with_width( "Tab width (columns)" , tabsizes ,
		sizeof(tabsizes) / sizeof(tabsizes[0]) ,
		selected_tabsize , 0 , tabsize_selected , NULL , 3 , 4) ;
}

char *
mc_get_tabsize(void)
{
	if( ! is_changed)
	{
		return  NULL ;
	}
	
	is_changed = 0 ;
	
	return  selected_tabsize ;
}
