/*
 *	$Id$
 */

#ifndef  __MC_WALL_PIC_H__
#define  __MC_WALL_PIC_H__


#include  <gtk/gtk.h>


GtkWidget *  mc_wall_pic_config_widget_new( char *  wall_pic) ;

int mc_wall_pic_ischanged(void);

char *  mc_get_wall_pic(void) ;


#endif
