/*
 *	$Id$
 */

#ifndef  __MC_COLOR_H__
#define  __MC_COLOR_H__


#include  <gtk/gtk.h>


GtkWidget *  mc_fg_color_config_widget_new( char *  color) ;

GtkWidget *  mc_bg_color_config_widget_new( char *  color) ;

GtkWidget *  mc_sb_fg_color_config_widget_new( char *  color) ;

GtkWidget *  mc_sb_bg_color_config_widget_new( char *  color) ;

char *  mc_get_fg_color(void) ;

char *  mc_get_bg_color(void) ;

char *  mc_get_sb_fg_color(void) ;

char *  mc_get_sb_bg_color(void) ;


#endif
