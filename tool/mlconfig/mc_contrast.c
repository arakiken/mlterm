/*
 *	$Id$
 */

#include  "mc_contrast.h"

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

static char *  new_contrast ;
static char *  old_contrast ;


/* --- static functions --- */

static gint
contrast_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	new_contrast = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s contrast is selected.\n" , new_contrast) ;
#endif

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_contrast_config_widget_new(void)
{
	char *  contrasts[] =
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

	new_contrast = old_contrast = mc_get_str_value( "contrast") ;

	return  mc_combo_new_with_width(_("Contrast"), contrasts,
		sizeof(contrasts) / sizeof(contrasts[0]), 
		new_contrast, 0, contrast_selected , NULL , 50);
}

void
mc_update_contrast(
	int  save
	)
{
	if( save)
	{
		mc_set_str_value( "contrast" , new_contrast , save) ;
	}
	else
	{
		if( strcmp( new_contrast , old_contrast) != 0)
		{
			mc_set_str_value( "contrast" , new_contrast , save) ;
			free( old_contrast) ;
			old_contrast = strdup( new_contrast) ;
		}
	}
}
