/*
 *	$Id$
 */

#ifndef  __MC_BEL_H__
#define  __MC_BEL_H__


#include  <gtk/gtk.h>
#include  <ml_bel_mode.h>


GtkWidget *  mc_bel_config_widget_new( ml_bel_mode_t  bel_mode) ;

ml_bel_mode_t  mc_get_bel_mode(void) ;


#endif
