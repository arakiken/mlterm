/*
 *	$Id$
 */

#include  "mc_screen_ratio.h"

#include  <kiklib/kik_debug.h>
#include  <glib.h>
#include  <c_intl.h>

#include  "mc_combo.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  selected_screen_width_ratio ;
static char *  selected_screen_height_ratio ;
static int  width_is_changed ;
static int  height_is_changed ;


/* --- static functions --- */

static gint
screen_width_ratio_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	selected_screen_width_ratio = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	width_is_changed = 1 ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s screen_width_ratio is selected.\n" ,
		selected_screen_width_ratio) ;
#endif

	return  1 ;
}

static gint
screen_height_ratio_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	selected_screen_height_ratio = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	height_is_changed = 1 ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s screen_height_ratio is selected.\n" ,
		selected_screen_height_ratio) ;
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
mc_screen_width_ratio_config_widget_new(
	char *  ratio
	)
{
	selected_screen_width_ratio = ratio ;

	return  config_widget_new(_("Width") , ratio , screen_width_ratio_selected) ;
}

GtkWidget *
mc_screen_height_ratio_config_widget_new(
	char *  ratio
	)
{
	selected_screen_height_ratio = ratio ;

	return  config_widget_new(_("Height") , ratio , screen_height_ratio_selected) ;
}

char *
mc_get_screen_width_ratio(void)
{
	if( ! width_is_changed)
	{
		return  NULL ;
	}
	
	width_is_changed = 0 ;
	
	return  selected_screen_width_ratio ;
}

char *
mc_get_screen_height_ratio(void)
{
	if( ! height_is_changed)
	{
		return  NULL ;
	}
	
	height_is_changed = 0 ;
	
	return  selected_screen_height_ratio ;
}
