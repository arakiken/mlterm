/*
 *	$Id$
 */

#include <kiklib/kik_str.h>
#include  "mc_combo.h"


#define  CHAR_WIDTH  10


/* --- global functions --- */

GtkWidget *
mc_combo_new(
	const char *  label_name ,
	char **  item_names ,
	u_int  item_num ,
	char *  selected_item_name ,
	int  is_readonly ,
	GtkWidget **  entry
	)
{
	return mc_combo_new_with_width(label_name , item_names , item_num ,
	                 selected_item_name , is_readonly , 0 , entry) ;
}

GtkWidget *
mc_combo_new_with_width(
	const char *  label_name ,
	char **  item_names ,
	u_int  item_num ,
	char *  selected_item_name ,
	int  is_readonly ,
	int  entry_width ,
	GtkWidget **  entry
	)
{
	GtkWidget *  hbox ;
	GtkWidget *  label ;
	GtkWidget *  combo ;
	int item_found ;
	u_int count ;

	hbox = gtk_hbox_new(FALSE, 0) ;
	gtk_widget_show(hbox) ;

	label = gtk_label_new(label_name) ;
	gtk_widget_show(label) ;
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5) ;

#if  GTK_CHECK_VERSION(2,90,0)
	combo = gtk_combo_box_text_new_with_entry() ;
#else
	combo = gtk_combo_box_entry_new_text() ;
#endif
	gtk_widget_show(combo) ;

	item_found = 0 ;
	for( count = 0 ; count < item_num ; count++)
	{
		if(strcmp(selected_item_name, item_names[count]) == 0)
		{
		#if  GTK_CHECK_VERSION(2,90,0)
			gtk_combo_box_text_prepend_text( GTK_COMBO_BOX_TEXT(combo) ,
				item_names[count]) ;
		#else
			gtk_combo_box_prepend_text( GTK_COMBO_BOX(combo) , item_names[count]) ;
		#endif
			item_found = 1 ;
		}
		else
		{
		#if  GTK_CHECK_VERSION(2,90,0)
			gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(combo) ,
				item_names[count]) ;
		#else
			gtk_combo_box_append_text( GTK_COMBO_BOX(combo) , item_names[count]) ;
		#endif
		}
	}

	if( ! item_found && ! is_readonly)
	{
	#if  GTK_CHECK_VERSION(2,90,0)
		gtk_combo_box_text_prepend_text( GTK_COMBO_BOX_TEXT(combo) , selected_item_name) ;
	#else
		gtk_combo_box_prepend_text( GTK_COMBO_BOX(combo) , selected_item_name) ;
	#endif
	}

	gtk_combo_box_set_active( GTK_COMBO_BOX(combo) , 0) ;

	if( entry)
	{
		*entry = gtk_bin_get_child( GTK_BIN(combo)) ;
	}

	if( is_readonly)
	{
		gtk_editable_set_editable( GTK_EDITABLE(gtk_bin_get_child( GTK_BIN(combo))) ,
			FALSE) ;
	}
	
	if( entry_width)
	{
	#if  GTK_CHECK_VERSION(2,90,0)
		gint  width_chars ;

		if( entry_width < CHAR_WIDTH)
		{
			width_chars = 1 ;
		}
		else
		{
			width_chars = entry_width / CHAR_WIDTH ;
		}

		gtk_entry_set_width_chars( gtk_bin_get_child( GTK_BIN(combo)) , width_chars) ;
	#else
		gtk_widget_set_size_request( gtk_bin_get_child( GTK_BIN(combo)) ,
			entry_width , -1) ;
	#endif
		gtk_box_pack_start( GTK_BOX(hbox) , combo , FALSE , FALSE , 0) ;
	}
	else
	{
		gtk_box_pack_start( GTK_BOX(hbox) , combo , TRUE , TRUE , 0) ;
	}

	return  hbox ;
}
