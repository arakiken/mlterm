/*
 *	$Id$
 */

#ifndef  __MC_BGTYPE_H__
#define  __MC_BGTYPE_H__


#include  <gtk/gtk.h>


GtkWidget *mc_bgtype_config_widget_new(char *, GtkWidget *, GtkWidget *);

int mc_bgtype_ischanged(void);

char *mc_get_bgtype_mode(void);


#endif
