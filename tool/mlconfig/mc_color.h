/*
 *	update: <2001/11/14(01:18:54)>
 *	$Id$
 */

#ifndef  __MC_COLOR_H__
#define  __MC_COLOR_H__


#include  <gtk/gtk.h>
#include  <ml_color.h>


GtkWidget *  mc_color_config_widget_new( ml_color_t  selected_color ,
	char *  title ,gint (*color_selected)(GtkWidget *,gpointer)) ;

ml_color_t  mc_get_color( char *  name) ;


#endif
