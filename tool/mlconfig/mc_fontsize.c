/*
 *	$Id$
 */

#include  "mc_encoding.h"

#include  <stdio.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>
#include  <glib.h>

#include  "mc_combo.h"

#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  selected_fontsize ;


/* --- static functions  --- */

static gint
fontsize_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	selected_fontsize = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s encoding is selected.\n" , selected_fontsize) ;
#endif

	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_fontsize_config_widget_new(
	char * fontsize ,
	u_int  min_fontsize ,
	u_int  max_fontsize
	)
{
	char **  fontlist ;
	char *  sizes ;
	int  counter ;
	
	if((fontlist = alloca(sizeof(char*) * (max_fontsize - min_fontsize + 1))) == NULL)
	{
		return  NULL ;
	}

	if((sizes = alloca((max_fontsize - min_fontsize + 1) * 10)) == NULL)
	{
		return  NULL ;
	}

	for( counter = min_fontsize ; counter <= max_fontsize ; counter++)
	{
		sprintf( sizes , "%d" , counter) ;
		fontlist[counter - min_fontsize] = sizes ;
		sizes += 10 ;
	}

	if( fontsize == NULL)
	{
		selected_fontsize = fontlist[0] ;
	}
	else
	{
		selected_fontsize = fontsize ;
	}

	return  mc_combo_new( "Font Size" , fontlist , max_fontsize - min_fontsize + 1 ,
		fontsize , 1 , fontsize_selected , NULL) ;
}

char *
mc_get_fontsize(void)
{
	return  selected_fontsize ;
}
