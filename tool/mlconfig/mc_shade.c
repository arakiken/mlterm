/*
 *	$Id$
 */

#include  "mc_shade.h"

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>
#include  <glib.h>

#include  "mc_combo.h"

#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  shade_ratios[] =
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

static char *  selected_shade_ratio ;


/* --- static functions --- */

static gint
shade_ratio_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	selected_shade_ratio = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s shade_ratio is selected.\n" , selected_shade_ratio) ;
#endif

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_shade_config_widget_new(
	char *  shade_ratio
	)
{
	selected_shade_ratio = shade_ratio ;

	return  mc_combo_new( "Shade ratio" , shade_ratios , sizeof(shade_ratios) / sizeof(shade_ratios[0]) ,
		selected_shade_ratio , 0 , shade_ratio_selected , NULL) ;
}

u_int
mc_get_shade_ratio(void)
{
	u_int  shade_ratio ;
	
	if( ! kik_str_to_uint( &shade_ratio , selected_shade_ratio))
	{
		kik_str_to_uint( &shade_ratio , shade_ratios[0]) ;
	}
	
	return  shade_ratio ;
}
