/*
 *	$Id$
 */

#ifndef  __MC_FONTSIZE_H__
#define  __MC_FONTSIZE_H__


#include  <gtk/gtk.h>
#include  <kiklib/kik_types.h>


GtkWidget *  mc_fontsize_config_widget_new( char * fontsize , u_int  min , u_int  max) ;

u_int  mc_get_fontsize(void) ;


#endif
