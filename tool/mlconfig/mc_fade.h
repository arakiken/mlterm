/*
 *	$Id$
 */

#ifndef  __MC_FADE_H__
#define  __MC_FADE_H__


#include  <kiklib/kik_types.h>
#include  <gtk/gtk.h>


GtkWidget *  mc_fade_config_widget_new( char *  fade_ratio) ;

u_int  mc_get_fade_ratio(void) ;


#endif
