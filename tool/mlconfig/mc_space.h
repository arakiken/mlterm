/*
 *	$Id$
 */

#ifndef  __MC_SPACE_H__
#define  __MC_SPACE_H__


#include  <gtk/gtk.h>


#define  MC_SPACES  2
#define  MC_SPACE_LINE    0
#define  MC_SPACE_LETTER  1


GtkWidget *  mc_space_config_widget_new( int  id) ;

void  mc_update_space( int  id) ;


#endif
