/*
 *	update: <2001/11/26(22:53:32)>
 *	$Id$
 */

#include  "mc_tabsize.h"

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>
#include  <glib.h>

#include  "mc_combo.h"

#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  tabsizes[] =
{
	"8" ,
	"4" ,
	"2" ,
} ;

static char *  selected_tabsize ;


/* --- static functions --- */

static gint
tabsize_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	selected_tabsize = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	
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
	selected_tabsize = tabsize ;

	return  mc_combo_new( "Tab Size" , tabsizes , sizeof(tabsizes) / sizeof(tabsizes[0]) ,
		selected_tabsize , 0 , tabsize_selected , NULL) ;
}

u_int
mc_get_tabsize(void)
{
	u_int  tabsize ;
	
	if( ! kik_str_to_int( &tabsize , selected_tabsize))
	{
		kik_str_to_int( &tabsize , tabsizes[0]) ;
	}
	
	return  tabsize ;
}
