/*
 *	$Id$
 */

#include  "mc_wall_pic.h"

#include  <string.h>
#include  <stdlib.h>		/* free */
#include  <glib.h>
#include  <c_intl.h>

#include  "mc_io.h"


/* --- static functions --- */

static GtkWidget *  entry ;
static char *  old_wall_pic ;


/* --- static functions --- */

static gint
file_sel_cancel_clicked(
	GtkObject *  object
	)
{
	gtk_widget_destroy( GTK_WIDGET(object)) ;
	
	return  FALSE ;
}

static gint
file_sel_ok_clicked(
	GtkObject *  object
	)
{
	gtk_entry_set_text( GTK_ENTRY(entry) ,
		gtk_file_selection_get_filename( GTK_FILE_SELECTION( object))) ;
	gtk_widget_destroy( GTK_WIDGET(object)) ;

	return  TRUE ;
}

static gint
button_clicked(
	GtkObject *  object
	)
{
	GtkWidget *  file_sel ;

	file_sel = gtk_file_selection_new( "Wall Paper") ;
	gtk_widget_show( GTK_WIDGET(file_sel)) ;

	gtk_signal_connect_object( GTK_OBJECT( GTK_FILE_SELECTION(file_sel)->ok_button) ,
		"clicked" , GTK_SIGNAL_FUNC(file_sel_ok_clicked) , GTK_OBJECT(file_sel)) ;

	gtk_signal_connect_object( GTK_OBJECT( GTK_FILE_SELECTION(file_sel)->cancel_button) ,
		"clicked" , GTK_SIGNAL_FUNC(file_sel_cancel_clicked) , GTK_OBJECT(file_sel)) ;
		
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
	
	hbox = gtk_hbox_new( FALSE , 5) ;
	gtk_widget_show(hbox) ;
 
#if 0
	label = gtk_label_new(_("Picture")) ;
	gtk_widget_show( label) ;
	gtk_box_pack_start(GTK_BOX(hbox) , label , FALSE , FALSE , 5) ;
#endif
	
	entry = gtk_entry_new() ;
	gtk_widget_show( entry) ;
	gtk_box_pack_start( GTK_BOX(hbox) , entry , TRUE , TRUE , 2) ;
	gtk_entry_set_text( GTK_ENTRY(entry) , wall_pic) ;
	
	button = gtk_button_new_with_label( _(" Select ")) ;
	gtk_widget_show(button) ;
	gtk_signal_connect(GTK_OBJECT(button) , "clicked" , GTK_SIGNAL_FUNC(button_clicked) , NULL) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , FALSE , FALSE , 0) ;

	old_wall_pic = wall_pic ;

	return  hbox ;
}

void
mc_update_wall_pic(
	int  save
	)
{
	char *  new_wall_pic ;

	if( *( new_wall_pic = gtk_entry_get_text( GTK_ENTRY(entry))) == '\0')
	{
		new_wall_pic = "none" ;
	}

	if( save)
	{
		mc_set_str_value( "wall_picture" , new_wall_pic , save) ;
	}
	else
	{
		if( strcmp( old_wall_pic , new_wall_pic) != 0)
		{
			mc_set_str_value( "wall_picture" , new_wall_pic , save) ;
			free( old_wall_pic) ;
			old_wall_pic = strdup( new_wall_pic) ;
		}
	}
}
