/*
 *	$Id$
 */

#include  "mc_brightness.h"

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>
#include  <glib.h>

#include  "mc_combo.h"

#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  brightnesss[] =
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

static char *  selected_brightness ;


/* --- static functions --- */

static gint
brightness_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	selected_brightness = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	
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
	selected_brightness = brightness ;

	return  mc_combo_new( "Brightness" , brightnesss , sizeof(brightnesss) / sizeof(brightnesss[0]) ,
		selected_brightness , 0 , brightness_selected , NULL) ;
}

u_int
mc_get_brightness(void)
{
	u_int  brightness ;
	
	if( ! kik_str_to_uint( &brightness , selected_brightness))
	{
		kik_str_to_uint( &brightness , brightnesss[0]) ;
	}
	
	return  brightness ;
}
