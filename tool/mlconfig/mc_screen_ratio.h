/*
 *	$Id$
 */

#ifndef  __MC_SCREEN_RATIO_H__
#define  __MC_SCREEN_RATIO_H__


#include  <gtk/gtk.h>


GtkWidget *  mc_screen_width_ratio_config_widget_new( char *  ratio) ;

GtkWidget *  mc_screen_height_ratio_config_widget_new( char *  ratio) ;

char *  mc_get_screen_width_ratio(void) ;

char *  mc_get_screen_height_ratio(void) ;


#endif
