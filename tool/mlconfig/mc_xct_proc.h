/*
 *	$Id$
 */

#ifndef  __MC_XCT_PROC_H__
#define  __MC_XCT_PROC_H__


#include  <gtk/gtk.h>

#include  <ml_xct_proc_mode.h>


GtkWidget *  mc_xct_proc_config_widget_new( ml_xct_proc_mode_t  xct_proc_mode) ;

ml_xct_proc_mode_t  mc_get_xct_proc_mode(void) ;


#endif
