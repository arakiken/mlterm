/*
 *	$Id$
 */

#include  "mc_sb_view.h"

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>
#include  <glib.h>

#include  "mc_combo.h"

#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  sb_view_names[] =
{
	"simple" ,
	"sample" ,
	"sample2" ,
	"athena" ,
	"motif" ,
	"mozmodern" ,
	"next" ,
} ;

static char *  selected_sb_view_name ;


/* --- static functions --- */

static gint
sb_view_name_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	selected_sb_view_name = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s sb_view_name is selected.\n" , selected_sb_view_name) ;
#endif

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_sb_view_config_widget_new(
	char *  sb_view_name
	)
{
	selected_sb_view_name = sb_view_name ;

	return  mc_combo_new( "View" , sb_view_names ,
		sizeof(sb_view_names) / sizeof(sb_view_names[0]) ,
		selected_sb_view_name , 0 , sb_view_name_selected , NULL) ;
}

char *
mc_get_sb_view_name(void)
{
	return  selected_sb_view_name ;
}
