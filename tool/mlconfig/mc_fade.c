/*
 *	$Id$
 */

#include  "mc_fade.h"

#include  <kiklib/kik_debug.h>
#include  <glib.h>
#include  <c_intl.h>

#include  "mc_combo.h"
#include  "mc_io.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  new_fade_ratio ;
static char *  old_fade_ratio ;


/* --- static functions --- */

static gint
fade_ratio_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	new_fade_ratio = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s fade_ratio is selected.\n" , new_fade_ratio) ;
#endif

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_fade_config_widget_new(void)
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

	new_fade_ratio = old_fade_ratio = mc_get_str_value( "fade_ratio") ;

	return  mc_combo_new_with_width(_("Fade ratio on unfocus"), fade_ratios,
		sizeof(fade_ratios) / sizeof(fade_ratios[0]),
		new_fade_ratio, 0, fade_ratio_selected, NULL, 80);
}

void
mc_update_fade_ratio(
	int  save
	)
{
	if( save)
	{
		mc_set_str_value( "fade_ratio" , new_fade_ratio , save) ;
	}
	else
	{
		if( strcmp( new_fade_ratio , old_fade_ratio) != 0)
		{
			mc_set_str_value( "fade_ratio" , new_fade_ratio , save) ;
			old_fade_ratio = new_fade_ratio ;
		}
	}
}
