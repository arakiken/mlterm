/*
 *	$Id$
 */

#ifndef  __MC_MOD_META_H__
#define  __MC_MOD_META_H__


#include  <gtk/gtk.h>
#include  <x_mod_meta_mode.h>


GtkWidget *  mc_mod_meta_config_widget_new( x_mod_meta_mode_t  mod_meta_mode) ;

x_mod_meta_mode_t  mc_get_mod_meta_mode(void) ;


#endif
