/*
 *	$Id$
 */

#include <kiklib/kik_str.h>
#include  "mc_combo.h"


/* --- global functions --- */

GtkWidget *
mc_combo_new(
	const char *  label_name ,
	char **  item_names ,
	u_int  item_num ,
	char *  selected_item_name ,
	int  is_readonly ,
	gint (*callback_changed)(GtkWidget * , gpointer) ,
	gpointer  data
	)
{
	return mc_combo_new_full(label_name, item_names, item_num,
	                 selected_item_name, is_readonly,
			 callback_changed, NULL, data, 0);
}

GtkWidget *
mc_combo_new_with_width(
	const char *  label_name ,
	char **  item_names ,
	u_int  item_num ,
	char *  selected_item_name ,
	int  is_readonly ,
	gint (*callback_changed)(GtkWidget * , gpointer) ,
	gpointer  data ,
	int  entry_width
	)
{
	return mc_combo_new_full(label_name, item_names, item_num,
	                 selected_item_name, is_readonly,
			 callback_changed, NULL, data, entry_width);
}

GtkWidget *
mc_combo_new_full(
	const char *  label_name ,
	char **  item_names ,
	u_int  item_num ,
	char *  selected_item_name ,
	int  is_readonly ,
	gint (*callback_changed)(GtkWidget * , gpointer) ,
	gint (*callback_map)(GtkWidget * , gpointer) ,
	gpointer  data ,
	int  entry_width
	)
{
	GtkWidget * hbox;
	GtkWidget * label;
	GtkWidget * combo;
	int item_found;
	u_int count;

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_widget_show(hbox);

	label = gtk_label_new(label_name) ;
	gtk_widget_show(label) ;
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

#if  GTK_CHECK_VERSION(2,90,0)
	combo = gtk_combo_box_text_new_with_entry();
#else
	combo = gtk_combo_box_entry_new_text();
#endif
	gtk_widget_show(combo);

	item_found = 0;
	for(count = 0; count < item_num; count++)
	{
	    if(strcmp(selected_item_name, item_names[count]) == 0)
	    {
	    #if  GTK_CHECK_VERSION(2,90,0)
	        gtk_combo_box_text_prepend_text( GTK_COMBO_BOX_TEXT(combo) , item_names[count]) ;
	    #else
	        gtk_combo_box_prepend_text( GTK_COMBO_BOX(combo), item_names[count]);
	    #endif
		item_found = 1;
	    }
	    else
	    {
	    #if  GTK_CHECK_VERSION(2,90,0)
	        gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(combo) , item_names[count]) ;
	    #else
	        gtk_combo_box_append_text( GTK_COMBO_BOX(combo), item_names[count]);
	    #endif
	    }
	}

	if(!item_found && !is_readonly)
	{
	#if  GTK_CHECK_VERSION(2,90,0)
	    gtk_combo_box_text_prepend_text( GTK_COMBO_BOX_TEXT(combo) , selected_item_name) ;
	#else
	    gtk_combo_box_prepend_text( GTK_COMBO_BOX(combo), selected_item_name);
	#endif
	}

	gtk_combo_box_set_active( GTK_COMBO_BOX(combo) , 0) ;

	if( callback_changed)
	{
		g_signal_connect( gtk_bin_get_child( GTK_BIN(combo)),
			"changed", G_CALLBACK(callback_changed), data);
	}

	if( callback_map)
	{
		g_signal_connect( gtk_bin_get_child( GTK_BIN(combo)),
			"map", G_CALLBACK(callback_map), data) ;
	}

	if( is_readonly)
	{
	    gtk_editable_set_editable( GTK_EDITABLE(gtk_bin_get_child( GTK_BIN(combo))), FALSE);
	}
	
	if( entry_width)
	{
	    gtk_widget_set_size_request( gtk_bin_get_child( GTK_BIN(combo)), entry_width, -1);
	    gtk_box_pack_start(GTK_BOX(hbox), combo, FALSE, FALSE, 0);
	}
	else
	{
	    gtk_box_pack_start(GTK_BOX(hbox), combo, TRUE, TRUE, 0);
	}

	return hbox;
}
