/*
 *	$Id$
 */

#include  "mc_wall_pic.h"

#include  <glib.h>


/* --- static functions --- */

static GtkWidget *  entry ;
static int  is_changed ;


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

	is_changed = 1 ;
	
	return  TRUE ;
}

static gint
button_clicked(
	GtkObject *  object
	)
{
	GtkWidget *  file_sel ;

	file_sel = gtk_file_selection_new( "Wallpaper") ;
	gtk_widget_show( GTK_WIDGET(file_sel)) ;

	gtk_signal_connect_object( GTK_OBJECT( GTK_FILE_SELECTION(file_sel)->ok_button) ,
		"clicked" , GTK_SIGNAL_FUNC(file_sel_ok_clicked) , GTK_OBJECT(file_sel)) ;

	gtk_signal_connect_object( GTK_OBJECT( GTK_FILE_SELECTION(file_sel)->cancel_button) ,
		"clicked" , GTK_SIGNAL_FUNC(file_sel_cancel_clicked) , GTK_OBJECT(file_sel)) ;
		
	return  TRUE ;
}


/* --- global functions --- */

GtkWidget *
mc_wall_pic_config_widget_new(
	char *  wall_pic
	)
{
	GtkWidget *  hbox ;
	GtkWidget *  button ;
	GtkWidget *  label ;
	
	hbox = gtk_hbox_new( FALSE , 5) ;
	gtk_widget_show(hbox) ;
 
#if 0
	label = gtk_label_new( "Picture") ;
	gtk_widget_show( label) ;
	gtk_box_pack_start(GTK_BOX(hbox) , label , FALSE , FALSE , 5) ;
#endif
	
	entry = gtk_entry_new() ;
	gtk_widget_show( entry) ;
	gtk_box_pack_start( GTK_BOX(hbox) , entry , TRUE , TRUE , 2) ;
	gtk_entry_set_text( GTK_ENTRY(entry) , wall_pic) ;
	
	button = gtk_button_new_with_label( " Select ") ;
	gtk_widget_show(button) ;
	gtk_signal_connect(GTK_OBJECT(button) , "clicked" , GTK_SIGNAL_FUNC(button_clicked) , NULL) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , FALSE , FALSE , 0) ;

	return  hbox ;
}

int
mc_wall_pic_ischanged(void)
{
    return is_changed;
}

char *
mc_get_wall_pic(void)
{
    char *  wall_pic ;

    if( *( wall_pic = gtk_entry_get_text( GTK_ENTRY(entry))) == '\0')
	wall_pic = "none" ;
    is_changed = 0 ;
    return  wall_pic ;
}
