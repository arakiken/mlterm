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
	return mc_combo_new_with_width( label_name , item_names , item_num , 
			     selected_item_name , is_readonly , callback ,
			     data , MC_COMBO_LABEL_WIDTH, MC_COMBO_TOTAL_WIDTH) ;
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
	int label_width,
	int total_width
	)
{
	GtkWidget *  table ;
	GtkWidget *  label ;
	GtkWidget *  combo ;
	GList *  items ;
	int  item_found ;
	int  count ;

	table = gtk_table_new(1 , total_width , TRUE);
	gtk_widget_show(table) ;

	label = gtk_label_new(label_name) ;
	gtk_widget_show(label) ;
	gtk_table_attach(GTK_TABLE(table) , label , 0 , label_width , 
			 0 , 1 , GTK_FILL|GTK_EXPAND , 0 , 2 , 0);

	items = NULL ;
	item_found = 0 ;
	for( count = 0 ; count < item_num ; count++)
	{
		if( strcmp( selected_item_name , item_names[count]) == 0)
		{
			items = g_list_prepend(items , item_names[count]) ;
			item_found = 1 ;
		}
		else
		{
			items = g_list_append(items , item_names[count]) ;
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
	
	gtk_widget_set_usize(GTK_WIDGET(GTK_COMBO(combo)->entry) , 10 , 0);
	gtk_table_attach(GTK_TABLE(table) , combo , label_width , total_width ,
			 0 , 1 , GTK_FILL|GTK_EXPAND|GTK_SHRINK , 0 , 2 , 0);

	return  table ;
}
