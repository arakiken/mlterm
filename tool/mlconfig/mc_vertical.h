/*
 *	$Id$
 */

#ifndef  __MC_VERTICAL_H__
#define  __MC_VERTICAL_H__


#include  <gtk/gtk.h>
#include  <ml_logical_visual.h>


GtkWidget *  mc_vertical_config_widget_new( ml_vertical_mode_t  vertical_mode) ;

ml_vertical_mode_t  mc_get_vertical_mode(void) ;


#endif
