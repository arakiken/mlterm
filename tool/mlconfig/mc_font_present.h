/*
 *	$Id$
 */

#ifndef  __MC_FONT_PRESENT_H__
#define  __MC_FONT_PRESENT_H__


#include  <gtk/gtk.h>
#include  <x_font.h>


GtkWidget *  mc_font_present_config_widget_new( x_font_present_t  font_present) ;

x_font_present_t  mc_get_font_present(void) ;


#endif
