/*
 *	$Id$
 */

#include  "mc_fontsize.h"

#include  <stdio.h>		/* sprintf */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>
#include  <kiklib/kik_mem.h>	/* alloca */
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
	kik_debug_printf( KIK_DEBUG_TAG " %s font size is selected.\n" , selected_fontsize) ;
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
	
	if( ( fontlist = alloca( sizeof(char*) * (max_fontsize - min_fontsize + 1))) == NULL)
	{
		return  NULL ;
	}

	if( ( sizes = alloca( ( max_fontsize - min_fontsize + 1) * 10)) == NULL)
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

	return  mc_combo_new( "Font size" , fontlist , max_fontsize - min_fontsize + 1 ,
		fontsize , 1 , fontsize_selected , NULL) ;
}

u_int
mc_get_fontsize(void)
{
	u_int  fontsize ;
	
	/* this never fails */
	if( ! kik_str_to_uint( &fontsize , selected_fontsize))
	{
		fontsize = 16 ;
	}
	
	return  fontsize ;
}
