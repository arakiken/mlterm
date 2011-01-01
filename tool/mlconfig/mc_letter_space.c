/*
 *	$Id$
 */

#include  "mc_letter_space.h"

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

static char *  new_letter_space = NULL;
static char *  old_letter_space = NULL;
static int is_changed;


/* --- static functions --- */

static gint
letter_space_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	free( new_letter_space);
	new_letter_space = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1) ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s letter_space is selected.\n" , new_letter_space) ;
#endif

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_letter_space_config_widget_new(void)
{
	char *  letter_spaces[] =
	{
		"5" ,
		"4" ,
		"3" ,
		"2" ,
		"1" ,
		"0" ,
	} ;

	new_letter_space = strdup( old_letter_space = mc_get_str_value( "letter_space")) ;
	is_changed = 0;

	return  mc_combo_new_with_width(_("Letter space (pixels)"), letter_spaces,
		sizeof(letter_spaces) / sizeof(letter_spaces[0]),
		new_letter_space, 0, letter_space_selected, NULL , 20);
}

void
mc_update_letter_space(void)
{
	if (strcmp(new_letter_space, old_letter_space)) is_changed = 1;

	if (is_changed)
	{
		mc_set_str_value( "letter_space" , new_letter_space) ;
		free( old_letter_space) ;
		old_letter_space = strdup( new_letter_space) ;
	}
}
