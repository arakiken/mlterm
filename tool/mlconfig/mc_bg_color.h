/*
 *	$Id$
 */

#ifndef  __MC_BG_COLOR_H__
#define  __MC_BG_COLOR_H__


#include  <gtk/gtk.h>
#include  <ml_color.h>


GtkWidget *  mc_bg_color_config_widget_new( ml_color_t  color) ;

ml_color_t  mc_get_bg_color(void) ;


#endif
