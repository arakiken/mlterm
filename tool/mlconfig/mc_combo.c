/*
 *	$Id$
 */

#include  "mc_combo.h"


/* --- global functions --- */

GtkWidget *
mc_combo_new(
	char *  label_name ,
	char **  item_names ,
	u_int  item_num ,
	char *  selected_item_name ,
	int  is_readonly ,
	gint (*callback)(GtkWidget * , gpointer) ,
	gpointer  data
	)
{
	GtkWidget *  hbox ;
	GtkWidget *  label ;
	GtkWidget *  combo ;
	GList *  items ;
	int  item_found ;
	int  counter ;

	hbox = gtk_hbox_new(FALSE , 5) ;
	gtk_widget_show(hbox) ;

	label = gtk_label_new(label_name) ;
	gtk_widget_show(label) ;
	gtk_box_pack_start(GTK_BOX(hbox) , label , TRUE , TRUE , 5) ;

	items = NULL ;
	item_found = 0 ;
	for( counter = 0 ; counter < item_num ; counter++)
	{
		if( strcmp( selected_item_name , item_names[counter]) == 0)
		{
			items = g_list_prepend(items , item_names[counter]) ;
			item_found = 1 ;
		}
		else
		{
			items = g_list_append(items , item_names[counter]) ;
		}
	}

	if( ! item_found && ! is_readonly)
	{
		items = g_list_prepend(items , selected_item_name) ;
	}
	
	combo = gtk_combo_new() ;
	gtk_widget_show(combo) ;
	
	gtk_combo_set_popdown_strings(GTK_COMBO(combo) , items) ;
	gtk_signal_connect(GTK_OBJECT(GTK_COMBO(combo)->entry) ,
		"changed" , GTK_SIGNAL_FUNC(callback) , data) ;

	if( is_readonly)
	{
		gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(combo)->entry) , FALSE) ;
	}
	
	gtk_box_pack_start(GTK_BOX(hbox) , combo , TRUE , TRUE , 2) ;

	return  hbox ;
}
