/*
 *	$Id$
 */

#include  "mc_fontsize.h"

#include  <kiklib/kik_debug.h>
#include  <glib.h>

#include  "mc_combo.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  selected_fontsize ;
static int  is_changed ;


/* --- static functions  --- */

static gint
fontsize_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	selected_fontsize = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	is_changed = 1 ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s font size is selected.\n" , selected_fontsize) ;
#endif

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_fontsize_config_widget_new(
	char *  fontsize
	)
{
	char *  fontlist[] =
	{
		"6" , "7" , "8" , "9" , "10" ,
		"11" , "12" , "13" , "14" , "15" , "16" , "17" , "18" , "19" , "20" ,
		"21" , "22" , "23" , "24" , "25" , "26" , "27" , "28" , "29" , "30" ,
	} ;
	
	if( fontsize == NULL)
	{
		selected_fontsize = fontlist[0] ;
	}
	else
	{
		selected_fontsize = fontsize ;
	}

	return  mc_combo_new_with_width("Font size (pixels)", fontlist,
		sizeof(fontlist) / sizeof(fontlist[0]), fontsize, 1,
		fontsize_selected, NULL, 80) ;
}

char *
mc_get_fontsize(void)
{
	if( ! is_changed)
	{
		return  NULL ;
	}
	
	is_changed = 0 ;
	
	return  selected_fontsize ;
}
