/*
 *	$Id$
 */

#include  "mc_line_space.h"

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>
#include  <glib.h>

#include  "mc_combo.h"

#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  line_spaces[] =
{
	"5" ,
	"4" ,
	"3" ,
	"2" ,
	"1" ,
	"0" ,
} ;

static char *  selected_line_space ;


/* --- static functions --- */

static gint
line_space_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	selected_line_space = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s line_space is selected.\n" , selected_line_space) ;
#endif

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_line_space_config_widget_new(
	char *  line_space
	)
{
	selected_line_space = line_space ;

	return  mc_combo_new( "Line space" , line_spaces , sizeof(line_spaces) / sizeof(line_spaces[0]) ,
		selected_line_space , 0 , line_space_selected , NULL) ;
}

u_int
mc_get_line_space(void)
{
	u_int  line_space ;
	
	if( ! kik_str_to_uint( &line_space , selected_line_space))
	{
		kik_str_to_uint( &line_space , line_spaces[0]) ;
	}
	
	return  line_space ;
}
