/*
 *	$Id$
 */

#ifndef  __MC_SB_H__
#define  __MC_SB_H__


#include  <gtk/gtk.h>
#include  <ml_sb_mode.h>


GtkWidget *  mc_sb_config_widget_new( ml_sb_mode_t  sb_mode) ;

ml_sb_mode_t  mc_get_sb_mode(void) ;


#endif
