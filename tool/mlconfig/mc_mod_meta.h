/*
 *	$Id$
 */

#ifndef  __MC_MOD_META_H__
#define  __MC_MOD_META_H__


#include  <gtk/gtk.h>
#include  <ml_mod_meta_mode.h>


GtkWidget *  mc_mod_meta_config_widget_new( ml_mod_meta_mode_t  mod_meta_mode) ;

ml_mod_meta_mode_t  mc_get_mod_meta_mode(void) ;


#endif
