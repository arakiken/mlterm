/*
 *	$Id$
 */

#ifndef  __MC_FONT_PRESENT_H__
#define  __MC_FONT_PRESENT_H__


#include  <gtk/gtk.h>
#include  <ml_font.h>


GtkWidget *  mc_font_present_config_widget_new( ml_font_present_t  font_present) ;

ml_font_present_t  mc_get_font_present(void) ;


#endif
