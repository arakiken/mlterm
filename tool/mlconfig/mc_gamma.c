/*
 *	$Id$
 */

#include  "mc_gamma.h"

#include  <string.h>
#include  <stdlib.h>		/* free */
#include  <kiklib/kik_debug.h>
#include  <glib.h>
#include  <c_intl.h>

#include  "mc_combo.h"
#include  "mc_io.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  new_gamma ;
static char *  old_gamma ;
static int is_changed;


/* --- static functions --- */

static gint
gamma_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	new_gamma = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s gamma is selected.\n" , new_gamma) ;
#endif

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_gamma_config_widget_new(void)
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

	new_gamma = old_gamma = mc_get_str_value( "gamma") ;
	is_changed = 0;

	return  mc_combo_new_with_width(_("Gamma"), gammas,
		sizeof(gammas) / sizeof(gammas[0]), 
		new_gamma, 0, gamma_selected , NULL , 50);
}

void
mc_update_gamma(void)
{
	if (strcmp(new_gamma, old_gamma)) is_changed = 1;

	if (is_changed)
	{
		mc_set_str_value( "gamma" , new_gamma) ;
		free( old_gamma) ;
		old_gamma = strdup( new_gamma) ;
	}
}
