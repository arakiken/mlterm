/*
 *	$Id$
 */

#ifndef  __MC_SHADE_H__
#define  __MC_SHADE_H__


#include  <kiklib/kik_types.h>
#include  <gtk/gtk.h>


GtkWidget *  mc_shade_config_widget_new( char *  shade_ratio) ;

u_int  mc_get_shade_ratio(void) ;


#endif
