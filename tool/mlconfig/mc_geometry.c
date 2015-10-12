/*
 *	$Id$
 */

#include  "mc_geometry.h"

#include  <kiklib/kik_str.h>
#include  <kiklib/kik_mem.h>		/* free */
#include  <kiklib/kik_debug.h>
#include  <glib.h>
#include  <c_intl.h>

#include  "mc_io.h"


#if  0
#define  __DEBUG
#endif


#define  MC_GEOMETRIES		2
#define  MC_GEOMETRY_COLUMNS	0
#define  MC_GEOMETRY_ROWS	1

#define  MAX_VALUE_LEN  4

#define  CHAR_WIDTH  10


/* --- static variables --- */

static char  new_values[MC_GEOMETRIES][MAX_VALUE_LEN + 1] ;	/* 0 - 9999 */
static char  old_values[MC_GEOMETRIES][MAX_VALUE_LEN + 1] ;	/* 0 - 9999 */
static int  is_changed ;

static char *  config_keys[MC_GEOMETRIES] =
{
	"cols" ,
	"rows" ,
} ;

static char *  labels[MC_GEOMETRIES] =
{
	N_("Columns") ,
	N_("Rows") ,
} ;


/* --- static functions --- */

static gint
geometry_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	gchar *  text ;

	text = gtk_editable_get_chars( GTK_EDITABLE(widget) , 0 , -1) ;
	if( strlen(text) <= MAX_VALUE_LEN)
	{
		strcpy( data , text) ;
	}
	g_free( text) ;

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_geometry_config_widget_new(void)
{
	GtkWidget *  hbox ;
	GtkWidget *  label ;
	GtkWidget *  entry ;
	char *  value ;
	int  count ;

	hbox = gtk_hbox_new( FALSE , 0) ;
	gtk_widget_show( hbox) ;

	for( count = 0 ; count < MC_GEOMETRIES ; count++)
	{
		label = gtk_label_new( labels[count]) ;
		gtk_widget_show( label) ;
		gtk_box_pack_start( GTK_BOX(hbox) , label , FALSE , FALSE , 5) ;

		value = mc_get_str_value( config_keys[count]) ;
		if( strlen(value) <= MAX_VALUE_LEN)
		{
			strcpy( new_values[count] , value) ;
			strcpy( old_values[count] , value) ;
		}

		entry = gtk_entry_new() ;
		gtk_entry_set_text( GTK_ENTRY(entry) , value) ;
		gtk_widget_show( entry) ;

		free(value) ;

	#if  GTK_CHECK_VERSION(2,90,0)
		gtk_entry_set_width_chars( GTK_ENTRY(entry) , MAX_VALUE_LEN) ;
	#else
		gtk_widget_set_size_request( entry , MAX_VALUE_LEN * CHAR_WIDTH , -1) ;
	#endif
		gtk_box_pack_start( GTK_BOX(hbox) , entry , FALSE , FALSE , 1) ;
		g_signal_connect( entry , "changed" , G_CALLBACK(geometry_selected) ,
			&new_values[count]) ;
	}

	return  hbox ;
}

void
mc_update_geometry(void)
{
	if( ! is_changed)
	{
		int  count ;

		for( count = 0 ; count < MC_GEOMETRIES ; count++)
		{
			if( strcmp( new_values[count] , old_values[count]))
			{
				is_changed = 1 ;
				break ;
			}
		}
	}

	if( is_changed)
	{
		char  value[MAX_VALUE_LEN + 1 + MAX_VALUE_LEN + 1] ;

		sprintf( value , "%sx%s" ,
				new_values[MC_GEOMETRY_COLUMNS] ,
				new_values[MC_GEOMETRY_ROWS]) ;
		mc_set_str_value( "geometry" , value) ;

		strcpy( old_values[MC_GEOMETRY_COLUMNS] , new_values[MC_GEOMETRY_COLUMNS]) ;
		strcpy( old_values[MC_GEOMETRY_ROWS] , new_values[MC_GEOMETRY_ROWS]) ;
	}
}
