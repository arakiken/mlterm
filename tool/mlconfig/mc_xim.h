/*
 *	update: <2001/11/16(15:27:24)>
 *	$Id$
 */

#ifndef  __MC_XIM_H__
#define  __MC_XIM_H__


#include  <gtk/gtk.h>


GtkWidget *  mc_xim_config_widget_new( char *  xim , char *  locale) ;

char *  mc_get_xim_name(void) ;

char *  mc_get_xim_locale(void) ;


#endif
