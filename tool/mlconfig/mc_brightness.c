/*
 *	$Id$
 */

#include  "mc_brightness.h"

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

static char *  new_brightness ;
static char *  old_brightness ;
static int is_changed;


/* --- static functions --- */

static gint
brightness_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	new_brightness = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s brightness is selected.\n" , new_brightness) ;
#endif

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_brightness_config_widget_new(
	
	)
{
	char *  brightnesses[] =
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

	old_brightness = new_brightness = mc_get_str_value( "brightness") ;
	is_changed = 0;
	
	return  mc_combo_new_with_width(_("Brightness"), brightnesses,
		sizeof(brightnesses) / sizeof(brightnesses[0]), 
		new_brightness, 0,	brightness_selected , NULL , 50);
}

void
mc_update_brightness(void)
{
	if (strcmp(new_brightness, old_brightness)) is_changed = 1;

	if (is_changed)
	{
		mc_set_str_value( "brightness" , new_brightness) ;
		free( old_brightness) ;
		old_brightness = strdup( new_brightness) ;
	}
}
