/*
 *	$Id$
 */

#include  "mc_sb_view.h"

#include  <kiklib/kik_debug.h>
#include  <glib.h>
#include  <ml_intl.h>

#include  "mc_combo.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  selected_sb_view_name ;
static int  is_changed ;


/* --- static functions --- */

static gint
sb_view_name_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	selected_sb_view_name = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	is_changed = 1 ;
	
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
	char *  sb_view_names[] =
	{
		"simple" ,
		"sample" ,
		"sample2" ,
		"athena" ,
		"motif" ,
		"mozmodern" ,
		"next" ,
	} ;

	selected_sb_view_name = sb_view_name ;

	return  mc_combo_new( _("View") , sb_view_names ,
		sizeof(sb_view_names) / sizeof(sb_view_names[0]) ,
		selected_sb_view_name , 0 , sb_view_name_selected , NULL) ;
}

char *
mc_get_sb_view_name(void)
{
	if( ! is_changed)
	{
		return  NULL ;
	}
	
	is_changed = 0 ;
	
	return  selected_sb_view_name ;
}
