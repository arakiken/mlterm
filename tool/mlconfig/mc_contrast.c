/*
 *	$Id$
 */

#include  "mc_contrast.h"

#include  <kiklib/kik_str.h>
#include  <kiklib/kik_mem.h>		/* free */
#include  <kiklib/kik_debug.h>
#include  <glib.h>
#include  <c_intl.h>

#include  "mc_combo.h"
#include  "mc_io.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  new_contrast = NULL;
static char *  old_contrast = NULL;
static int is_changed;


/* --- static functions --- */

static gint
contrast_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	free( new_contrast);
	new_contrast = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1) ;
	
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

	new_contrast = strdup( old_contrast = mc_get_str_value( "contrast")) ;
	is_changed = 0;

	return  mc_combo_new_with_width(_("Contrast"), contrasts,
		sizeof(contrasts) / sizeof(contrasts[0]), 
		new_contrast, 0, contrast_selected , NULL , 50);
}

void
mc_update_contrast(void)
{
	if (strcmp(new_contrast, old_contrast)) is_changed = 1;

	if (is_changed)
	{
		mc_set_str_value( "contrast" , new_contrast) ;
		free( old_contrast) ;
		old_contrast = strdup( new_contrast) ;
	}
}
