/*
 *	$Id$
 */

#ifndef  __MC_SCREEN_RATIO_H__
#define  __MC_SCREEN_RATIO_H__


#include  <kiklib/kik_types.h>
#include  <gtk/gtk.h>


GtkWidget *  mc_screen_width_ratio_config_widget_new( char *  ratio) ;

GtkWidget *  mc_screen_height_ratio_config_widget_new( char *  ratio) ;

u_int  mc_get_screen_width_ratio(void) ;

u_int  mc_get_screen_height_ratio(void) ;


#endif
