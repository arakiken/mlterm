/*
 *	$Id$
 */

#include  "mc_contrast.h"

#include  <kiklib/kik_debug.h>
#include  <glib.h>
#include  <c_intl.h>

#include  "mc_combo.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  selected_contrast ;
static int  is_changed ;


/* --- static functions --- */

static gint
contrast_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	selected_contrast = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	is_changed = 1 ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s contrast is selected.\n" , selected_contrast) ;
#endif

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_contrast_config_widget_new(
	char *  contrast
	)
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

	selected_contrast = contrast ;

	return  mc_combo_new_with_width(_("Contrast"), contrasts,
		sizeof(contrasts) / sizeof(contrasts[0]), 
		selected_contrast, 0, contrast_selected , NULL , 50);
}

char *
mc_get_contrast(void)
{
	if( ! is_changed)
	{
		return  NULL ;
	}
	
	is_changed = 0 ;
	
	return  selected_contrast ;
}
