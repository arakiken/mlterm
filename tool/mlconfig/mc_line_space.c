/*
 *	$Id$
 */

#include  "mc_line_space.h"

#include  <kiklib/kik_debug.h>
#include  <glib.h>
#include  <c_intl.h>

#include  "mc_combo.h"
#include  "mc_io.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  new_line_space ;
static char *  old_line_space ;


/* --- static functions --- */

static gint
line_space_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	new_line_space = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	
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

	new_line_space = old_line_space = mc_get_str_value( "line_space") ;

	return  mc_combo_new_with_width(_("Line space (pixels)"), line_spaces,
		sizeof(line_spaces) / sizeof(line_spaces[0]),
		new_line_space, 0, line_space_selected, NULL , 80);
}

void
mc_update_line_space(
	int  save
	)
{
	if( save)
	{
		mc_set_str_value( "line_space" , new_line_space , save) ;
	}
	else
	{
		if( strcmp( new_line_space , old_line_space) != 0)
		{
			mc_set_str_value( "line_space" , new_line_space , save) ;
			old_line_space = new_line_space ;
		}
	}
}
