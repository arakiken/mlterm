/*
 *	$Id$
 */

#include  "mc_line_space.h"

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

static char *  new_line_space = NULL;
static char *  old_line_space = NULL;
static int is_changed;


/* --- static functions --- */

static gint
line_space_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	free( new_line_space);
	new_line_space = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1) ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s line_space is selected.\n" , new_line_space) ;
#endif

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_line_space_config_widget_new(void)
{
	char *  line_spaces[] =
	{
		"5" ,
		"4" ,
		"3" ,
		"2" ,
		"1" ,
		"0" ,
	} ;

	new_line_space = strdup( old_line_space = mc_get_str_value( "line_space")) ;
	is_changed = 0;

	return  mc_combo_new_with_width(_("Line space (pixels)"), line_spaces,
		sizeof(line_spaces) / sizeof(line_spaces[0]),
		new_line_space, 0, line_space_selected, NULL , 20);
}

void
mc_update_line_space(void)
{
	if (strcmp(new_line_space, old_line_space)) is_changed = 1;

	if (is_changed)
	{
		mc_set_str_value( "line_space" , new_line_space) ;
		free( old_line_space) ;
		old_line_space = strdup( new_line_space) ;
	}
}
