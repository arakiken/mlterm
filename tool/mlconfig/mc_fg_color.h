/*
 *	update: <2001/11/14(01:19:06)>
 *	$Id$
 */

#ifndef  __MC_FG_COLOR_H__
#define  __MC_FG_COLOR_H__


#include  <gtk/gtk.h>
#include  <ml_color.h>


GtkWidget *  mc_fg_color_config_widget_new( ml_color_t color) ;

ml_color_t  mc_get_fg_color(void) ;


#endif
