/*
 *	$Id$
 */

#ifndef  __MC_COLOR_H__
#define  __MC_COLOR_H__


#include  <gtk/gtk.h>

#define MC_COLOR_MODES 4

#define MC_COLOR_FG 0
#define MC_COLOR_BG 1
#define MC_COLOR_SBFG 2
#define MC_COLOR_SBBG 3

GtkWidget *mc_color_config_widget_new(int id);
void mc_update_color(int id);

#endif
