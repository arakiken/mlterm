/*
 *	$Id$
 */

#ifndef  __MC_COMBO_H__
#define  __MC_COMBO_H__


#include  <kiklib/kik_types.h>
#include  <gtk/gtk.h>
#include  <glib.h>

#define MC_COMBO_LABEL_WIDTH 1
#define MC_COMBO_TOTAL_WIDTH 2


GtkWidget * mc_combo_new( char *  label_name , char **  item_names ,
	u_int  item_num , char *  selected_item_name , int  is_readonly ,
	gint (*callback)(GtkWidget * , gpointer) , gpointer  data) ;

GtkWidget * mc_combo_new_with_width(char * label_name, char ** item_names,
	u_int item_num, char * selected_item_name, int is_readonly,
	gint (*callback)(GtkWidget *, gpointer), gpointer data,
	int entry_width);


#endif
