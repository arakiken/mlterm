/*
 *	$Id$
 */

#include  "mc_fade.h"

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>
#include  <glib.h>

#include  "mc_combo.h"

#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  fade_ratios[] =
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

static char *  selected_fade_ratio ;


/* --- static functions --- */

static gint
fade_ratio_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	selected_fade_ratio = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s fade_ratio is selected.\n" , selected_fade_ratio) ;
#endif

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_fade_config_widget_new(
	char *  fade_ratio
	)
{
	selected_fade_ratio = fade_ratio ;

	return  mc_combo_new( "Fade ratio" , fade_ratios , sizeof(fade_ratios) / sizeof(fade_ratios[0]) ,
		selected_fade_ratio , 0 , fade_ratio_selected , NULL) ;
}

u_int
mc_get_fade_ratio(void)
{
	u_int  fade_ratio ;
	
	if( ! kik_str_to_uint( &fade_ratio , selected_fade_ratio))
	{
		kik_str_to_uint( &fade_ratio , fade_ratios[0]) ;
	}
	
	return  fade_ratio ;
}
