/*
 *	$Id$
 */

#include <string.h>
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
	return mc_combo_new_with_width(label_name, item_names, item_num,
				       selected_item_name, is_readonly,
				       callback, data, 0);
}

GtkWidget *
mc_combo_new_with_width(
	char *  label_name ,
	char **  item_names ,
	u_int  item_num ,
	char *  selected_item_name ,
	int  is_readonly ,
	gint (*callback)(GtkWidget * , gpointer) ,
	gpointer  data,
	int entry_width
	)
{
	GtkWidget * hbox;
	GtkWidget * label;
	GtkWidget * combo;
	GList * items;
	int item_found;
	int count;

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_widget_show(hbox);

	label = gtk_label_new(label_name) ;
	gtk_widget_show(label) ;
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

	items = NULL;
	item_found = 0;
	for(count = 0; count < item_num; count++)
	{
	    if(strcmp(selected_item_name, item_names[count]) == 0)
	    {
		items = g_list_prepend(items, item_names[count]);
		item_found = 1;
	    }
	    else
	    {
		items = g_list_append(items, item_names[count]);
	    }
	}

	if(!item_found && !is_readonly)
	{
	    items = g_list_prepend(items, selected_item_name);
	}
	
	combo = gtk_combo_new();
	gtk_widget_show(combo);
	
	gtk_combo_set_popdown_strings(GTK_COMBO(combo), items);
	gtk_signal_connect(GTK_OBJECT(GTK_COMBO(combo)->entry),
		"changed", GTK_SIGNAL_FUNC(callback), data);

	if( is_readonly)
	{
	    gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(combo)->entry), FALSE);
	}
	
	if (entry_width) {
	    gtk_widget_set_usize(GTK_WIDGET(GTK_COMBO(combo)->entry),
				 entry_width , 0);
	    gtk_box_pack_start(GTK_BOX(hbox), combo, FALSE, FALSE, 0);
	} else {
	    gtk_box_pack_start(GTK_BOX(hbox), combo, TRUE, TRUE, 0);
	}

	return hbox;
}
