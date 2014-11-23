/*
 *	$Id$
 */

#include  "mc_wall_pic.h"

#include  <kiklib/kik_str.h>
#include  <kiklib/kik_mem.h>		/* free */
#include  <glib.h>
#include  <c_intl.h>

#include  "mc_io.h"


/* --- static functions --- */

static GtkWidget *  entry ;
static char *  old_wall_pic = NULL;
static int is_changed;


/* --- static functions --- */

static gint
button_clicked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	GtkWidget *  dialog ;

	dialog = gtk_file_chooser_dialog_new( "Wall Paper" , NULL ,
			GTK_FILE_CHOOSER_ACTION_OPEN ,
			GTK_STOCK_CANCEL , GTK_RESPONSE_CANCEL ,
			GTK_STOCK_OPEN , GTK_RESPONSE_ACCEPT , NULL) ;

	if( gtk_dialog_run( GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		gchar *  filename ;

		filename = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(dialog)) ;
		gtk_entry_set_text( GTK_ENTRY(entry) , filename) ;
		g_free( filename) ;
	}

	gtk_widget_destroy( dialog) ;

	return  TRUE ;
}


/* --- global functions --- */

GtkWidget *
mc_wall_pic_config_widget_new(void)
{
	GtkWidget *  hbox ;
	GtkWidget *  button ;
	char *  wall_pic ;

	wall_pic = mc_get_str_value( "wall_picture") ;
	
	hbox = gtk_hbox_new( FALSE , 0) ;
 
#if 0
	label = gtk_label_new(_("Picture")) ;
	gtk_widget_show( label) ;
	gtk_box_pack_start(GTK_BOX(hbox) , label , FALSE , FALSE , 5) ;
#endif
	
	entry = gtk_entry_new() ;
	gtk_widget_show( entry) ;
	gtk_box_pack_start( GTK_BOX(hbox) , entry , TRUE , TRUE , 2) ;
	gtk_entry_set_text( GTK_ENTRY(entry) , wall_pic) ;
	
	button = gtk_button_new_with_label( _("Select")) ;
	gtk_widget_show(button) ;
	g_signal_connect(button , "clicked" , G_CALLBACK(button_clicked) , NULL) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , FALSE , FALSE , 0) ;

	old_wall_pic = wall_pic ;
	is_changed = 0 ;

	return  hbox ;
}

void
mc_update_wall_pic(void)
{
	char *  new_wall_pic ;

	new_wall_pic = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);

	if( strcmp( old_wall_pic , new_wall_pic) != 0) is_changed = 1;

	if( is_changed)
	{
		mc_set_str_value( "wall_picture" , new_wall_pic) ;
		g_free( old_wall_pic) ;
		old_wall_pic = new_wall_pic ;
	}else{
		g_free( new_wall_pic);
	}
}

void
mc_wall_pic_none(void)
{
	g_free( old_wall_pic);
	old_wall_pic = strdup( "");

	mc_set_str_value( "wall_picture" , "") ;
}

