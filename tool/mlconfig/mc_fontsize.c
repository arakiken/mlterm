/*
 *	$Id$
 */

#include  "mc_fontsize.h"

#include  <kiklib/kik_debug.h>
#include  <glib.h>
#include  <c_intl.h>

#include  "mc_combo.h"
#include  "mc_io.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  new_fontsize ;
static char *  old_fontsize ;


/* --- static functions  --- */

static gint
fontsize_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	new_fontsize = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s font size is selected.\n" , new_fontsize) ;
#endif

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_fontsize_config_widget_new(void)
{
	char *  fontlist[] =
	{
		"6" , "7" , "8" , "9" , "10" ,
		"11" , "12" , "13" , "14" , "15" , "16" , "17" , "18" , "19" , "20" ,
		"21" , "22" , "23" , "24" , "25" , "26" , "27" , "28" , "29" , "30" ,
	} ;

	new_fontsize = old_fontsize = mc_get_str_value( "fontsize") ;

	return  mc_combo_new_with_width(_("Font size (pixels)"), fontlist ,
		sizeof(fontlist) / sizeof(fontlist[0]), new_fontsize , 1 ,
		fontsize_selected, NULL, 80) ;
}

void
mc_update_fontsize(
	int  save
	)
{
	if( save)
	{
		mc_set_str_value( "fontsize" , new_fontsize , save) ;
	}
	else
	{
		if( strcmp( new_fontsize , old_fontsize) != 0)
		{
			mc_set_str_value( "fontsize" , new_fontsize , save) ;
			old_fontsize = new_fontsize ;
		}
	}
}
