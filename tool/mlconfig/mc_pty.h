/*
 *	$Id$
 */

#ifndef  __MC_PTY_H__
#define  __MC_PTY_H__


#include  <gtk/gtk.h>


GtkWidget *  mc_pty_config_widget_new( char *  my_pty , char *  pty_list) ;

char *  mc_get_pty_dev(void) ;


#endif
