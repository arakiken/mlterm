/*
 *	update: <2001/11/26(22:50:48)>
 *	$Id$
 */

#include  "mc_bg_color.h"

#include  <kiklib/kik_debug.h>
#include  <glib.h>

#include  "mc_color.h"
#include  "mc_combo.h"

#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static ml_color_t  selected_bg_color = 0 ;


/* --- static functions --- */

static gint
color_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	selected_bg_color = mc_get_color( gtk_entry_get_text(GTK_ENTRY(widget))) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %d color is selected.\n" , selected_bg_color) ;
#endif

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_bg_color_config_widget_new(
	ml_color_t  color
	)
{
	selected_bg_color = color ;
	
	return  mc_color_config_widget_new( color , "BG Color" , color_selected) ;
}

ml_color_t
mc_get_bg_color(void)
{
	return  selected_bg_color ;
}
