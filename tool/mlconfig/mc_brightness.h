/*
 *	$Id$
 */

#ifndef  __MC_BRIGHTNESS_H__
#define  __MC_BRIGHTNESS_H__


#include  <kiklib/kik_types.h>
#include  <gtk/gtk.h>


GtkWidget *  mc_brightness_config_widget_new( char *  brightness) ;

u_int  mc_get_brightness(void) ;


#endif
