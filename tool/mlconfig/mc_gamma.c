/*
 *	$Id$
 */

#include  "mc_gamma.h"

#include  <kiklib/kik_debug.h>
#include  <glib.h>
#include  <c_intl.h>

#include  "mc_combo.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  selected_gamma ;
static int  is_changed ;


/* --- static functions --- */

static gint
gamma_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	selected_gamma = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	is_changed = 1 ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s gamma is selected.\n" , selected_gamma) ;
#endif

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_gamma_config_widget_new(
	char *  gamma
	)
{
	char *  gammas[] =
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

	selected_gamma = gamma ;

	return  mc_combo_new_with_width(_("Gamma"), gammas,
		sizeof(gammas) / sizeof(gammas[0]), 
		selected_gamma, 0, gamma_selected , NULL , 50);
}

char *
mc_get_gamma(void)
{
	if( ! is_changed)
	{
		return  NULL ;
	}
	
	is_changed = 0 ;
	
	return  selected_gamma ;
}
