/*
 *	$Id$
 */

#include  "mc_screen_ratio.h"

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

static char *  new_screen_width_ratio ;
static char *  new_screen_height_ratio ;
static char *  old_screen_width_ratio ;
static char *  old_screen_height_ratio ;


/* --- static functions --- */

static gint
screen_width_ratio_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	new_screen_width_ratio = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s screen_width_ratio is selected.\n" ,
		new_screen_width_ratio) ;
#endif

	return  1 ;
}

static gint
screen_height_ratio_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	new_screen_height_ratio = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s screen_height_ratio is selected.\n" ,
		new_screen_height_ratio) ;
#endif

	return  1 ;
}

static GtkWidget *
config_widget_new(
	char *  title ,
	char *  ratio ,
	gint (*ratio_selected)(GtkWidget *,gpointer)
	)
{
	char *  screen_ratios[] =
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

	return  mc_combo_new_with_width(title ,screen_ratios,
		sizeof(screen_ratios) / sizeof(screen_ratios[0]),
		ratio, 0, ratio_selected, NULL, 80);
}


/* --- global functions --- */

GtkWidget *
mc_screen_width_ratio_config_widget_new(void)
{
	new_screen_width_ratio = old_screen_width_ratio = mc_get_str_value( "screen_width_ratio") ;

	return  config_widget_new(_("Width") , new_screen_width_ratio , screen_width_ratio_selected) ;
}

GtkWidget *
mc_screen_height_ratio_config_widget_new(void)
{
	new_screen_height_ratio = old_screen_height_ratio = mc_get_str_value( "screen_height_ratio") ;

	return  config_widget_new(_("Height") , new_screen_height_ratio , screen_height_ratio_selected) ;
}

void
mc_update_screen_width_ratio(
	int  save
	)
{
	if( save)
	{
		mc_set_str_value( "screen_width_ratio" , new_screen_width_ratio , save) ;
	}
	else
	{
		if( strcmp( new_screen_width_ratio , old_screen_width_ratio) != 0)
		{
			mc_set_str_value( "screen_width_ratio" , new_screen_width_ratio , save) ;
			free( old_screen_width_ratio) ;
			old_screen_width_ratio = strdup( new_screen_width_ratio) ;
		}
	}
}

void
mc_update_screen_height_ratio(
	int  save
	)
{
	if( save)
	{
		mc_set_str_value( "screen_height_ratio" , new_screen_height_ratio , save) ;
	}
	else
	{
		if( strcmp( new_screen_height_ratio , old_screen_height_ratio) != 0)
		{
			mc_set_str_value( "screen_height_ratio" , new_screen_height_ratio , save) ;
			free( old_screen_height_ratio) ;
			old_screen_height_ratio = strdup( new_screen_height_ratio) ;
		}
	}
}
