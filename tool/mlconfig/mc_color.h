/*
 *	$Id$
 */

#ifndef  __MC_COLOR_H__
#define  __MC_COLOR_H__


#include  <gtk/gtk.h>

#include  "mc_io.h"


#define MC_COLOR_MODES 11

#define MC_COLOR_FG 0
#define MC_COLOR_BG 1
#define MC_COLOR_SBFG 2
#define MC_COLOR_SBBG 3
#define MC_COLOR_CURSOR_FG 4
#define MC_COLOR_CURSOR_BG 5
#define MC_COLOR_BD 6
#define MC_COLOR_IT 7
#define MC_COLOR_UL 8
#define MC_COLOR_BL 9
#define MC_COLOR_CO 10


GtkWidget *  mc_color_config_widget_new( int id) ;

GtkWidget *  mc_cursor_color_config_widget_new(void) ;

GtkWidget *  mc_substitute_color_config_widget_new(void) ;

GtkWidget *  mc_vtcolor_config_widget_new(void) ;

void  mc_update_color(int id) ;

void  mc_update_cursor_color(void) ;

void  mc_update_substitute_color(void) ;

void  mc_update_vtcolor( mc_io_t  io) ;


#endif
