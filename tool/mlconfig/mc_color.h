/*
 *	$Id$
 */

#ifndef  __MC_COLOR_H__
#define  __MC_COLOR_H__


#include  <gtk/gtk.h>


GtkWidget *  mc_fg_color_config_widget_new( void) ;

GtkWidget *  mc_bg_color_config_widget_new( void) ;

GtkWidget *  mc_sb_fg_color_config_widget_new( void) ;

GtkWidget *  mc_sb_bg_color_config_widget_new( void) ;

void  mc_update_fg_color(void) ;

void  mc_update_bg_color(void) ;

void  mc_update_sb_fg_color(void) ;

void  mc_update_sb_bg_color(void) ;


#endif
