/*
 *	$Id$
 */

#include  "mc_bidi.h"

#include  <kiklib/kik_str.h>
#include  <kiklib/kik_mem.h>		/* free */
#include  <kiklib/kik_debug.h>
#include  <glib.h>
#include  <c_intl.h>

#include  "mc_io.h"
#include  "mc_flags.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static GtkWidget *  entry ;
static char *  old_bidisep ;
static int  is_changed ;


/* --- static funcitons --- */

static void
set_str_value(
	const char *  value
	)
{
	char *  replaced ;

	if( ( replaced = kik_str_replace( value , "\\" , "\\\\")))
	{
		value = replaced ;
	}

	mc_set_str_value( "bidi_separators" , value) ;

	free( replaced) ;
}

static gint
toggled(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	gtk_widget_set_sensitive( entry ,
		gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(widget))) ;

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_bidi_config_widget_new(void)
{
	GtkWidget *  hbox ;
	GtkWidget *  check ;
	GtkWidget *  label ;

	hbox = gtk_hbox_new( FALSE , 0) ;
	gtk_widget_show( hbox) ;

	check = mc_flag_config_widget_new( MC_FLAG_BIDI) ;
	gtk_widget_show( check) ;
	gtk_box_pack_start( GTK_BOX(hbox) , check , FALSE , FALSE , 0) ;
	g_signal_connect( check , "toggled" , G_CALLBACK(toggled) , NULL) ;

	label = gtk_label_new( _("Separators")) ;
	gtk_widget_show( label) ;
	gtk_box_pack_start( GTK_BOX(hbox) , label , FALSE , FALSE , 5) ;

	entry = gtk_entry_new() ;
	old_bidisep = mc_get_str_value( "bidi_separators") ;
	gtk_entry_set_text( GTK_ENTRY(entry) , old_bidisep) ;
	gtk_widget_show( entry);
	gtk_box_pack_start( GTK_BOX(hbox) , entry , TRUE , TRUE , 1) ;
#if  GTK_CHECK_VERSION(2,12,0)
	gtk_widget_set_tooltip_text( entry , "Separator characters (ASCII only) to reorder every separated area by bidi algorithm respectively.") ;
#endif

	return hbox ;
}

void
mc_update_bidi(void)
{
	const char *  new_bidisep ;

	mc_update_flag_mode(MC_FLAG_BIDI) ;

	new_bidisep = gtk_entry_get_text( GTK_ENTRY( entry)) ;

	if( strcmp( new_bidisep , old_bidisep))
	{
		is_changed = 1 ;
	}

	if( is_changed)
	{
		free( old_bidisep) ;
		set_str_value( ( old_bidisep = strdup( new_bidisep))) ;
	}
}
