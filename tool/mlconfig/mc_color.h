/*
 *	$Id$
 */

#ifndef  __MC_COLOR_H__
#define  __MC_COLOR_H__


#include  <gtk/gtk.h>
#include  <ml_color.h>


GtkWidget *  mc_fg_color_config_widget_new( ml_color_t  color) ;

GtkWidget *  mc_bg_color_config_widget_new( ml_color_t  color) ;

ml_color_t  mc_get_fg_color(void) ;

ml_color_t  mc_get_bg_color(void) ;


#endif
