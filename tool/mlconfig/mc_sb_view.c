/*
 *	$Id$
 */

#include  "mc_sb_view.h"

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

static char *  new_sb_view_name ;
static char *  old_sb_view_name ;


/* --- static functions --- */

static gint
sb_view_name_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	new_sb_view_name = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s sb_view_name is selected.\n" , new_sb_view_name) ;
#endif

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_sb_view_config_widget_new(void)
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

	new_sb_view_name = old_sb_view_name = mc_get_str_value( "scrollbar_view_name") ;

	return  mc_combo_new( _("View") , sb_view_names ,
		sizeof(sb_view_names) / sizeof(sb_view_names[0]) ,
		new_sb_view_name , 0 , sb_view_name_selected , NULL) ;
}

void
mc_update_sb_view_name(
	int  save
	)
{
	if( save)
	{
		mc_set_str_value( "scrollbar_view_name" , new_sb_view_name , save) ;
	}
	else
	{
		if( strcmp( new_sb_view_name , old_sb_view_name) != 0)
		{
			mc_set_str_value( "scrollbar_view_name" , new_sb_view_name , save) ;
			free( old_sb_view_name) ;
			old_sb_view_name = strdup( new_sb_view_name) ;
		}
	}
}
