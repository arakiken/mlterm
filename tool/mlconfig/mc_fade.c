/*
 *	$Id$
 */

#include  "mc_fade.h"

#include  <kiklib/kik_debug.h>
#include  <glib.h>

#include  "mc_combo.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  selected_fade_ratio ;
static int  is_changed ;


/* --- static functions --- */

static gint
fade_ratio_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	selected_fade_ratio = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	is_changed = 1 ;
	
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
	char *  fade_ratios[] =
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

	selected_fade_ratio = fade_ratio ;

	return  mc_combo_new_with_width("Fade ratio on unfocus", fade_ratios,
		sizeof(fade_ratios) / sizeof(fade_ratios[0]),
		selected_fade_ratio, 0, fade_ratio_selected, NULL, 80);
}

char *
mc_get_fade_ratio(void)
{
	if( ! is_changed)
	{
		return  NULL ;
	}
	
	is_changed = 0 ;
	
	return  selected_fade_ratio ;
}
