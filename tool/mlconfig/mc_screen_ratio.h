/*
 *	$Id$
 */

#ifndef  __MC_SCREEN_RATIO_H__
#define  __MC_SCREEN_RATIO_H__


#include  <gtk/gtk.h>


GtkWidget *  mc_screen_width_ratio_config_widget_new(void) ;

GtkWidget *  mc_screen_height_ratio_config_widget_new(void) ;

void  mc_update_screen_width_ratio(void) ;

void  mc_update_screen_height_ratio(void) ;


#endif
