/*
 *	$Id$
 */

#include  "mc_brightness.h"

#include  <kiklib/kik_debug.h>
#include  <glib.h>

#include  "mc_combo.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  selected_brightness ;
static int  is_changed ;


/* --- static functions --- */

static gint
brightness_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	selected_brightness = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	is_changed = 1 ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s brightness is selected.\n" , selected_brightness) ;
#endif

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_brightness_config_widget_new(
	char *  brightness
	)
{
	char *  brightnesss[] =
	{
		"100" ,
		"90" ,
		"80" ,
		"70" ,
		"60" ,
		"50" ,
		"40" ,
		"30" ,
		"20" ,
		"10" ,
	} ;

	selected_brightness = brightness ;

	return  mc_combo_new_with_width( "Wall picture brightness" , brightnesss ,
		sizeof(brightnesss) / sizeof(brightnesss[0]) , selected_brightness , 0 ,
		brightness_selected , NULL , 3 , 4) ;
}

char *
mc_get_brightness(void)
{
	if( ! is_changed)
	{
		return  NULL ;
	}
	
	is_changed = 0 ;
	
	return  selected_brightness ;
}
