/*
 *	$Id$
 */

#include  "mc_color.h"

#include  <kiklib/kik_debug.h>
#include  <glib.h>

#include  "mc_combo.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  selected_fg_color ;
static char *  selected_bg_color ;
static char *  selected_sb_fg_color ;
static char *  selected_sb_bg_color ;
static int  fg_is_changed ;
static int  bg_is_changed ;
static int  sb_fg_is_changed ;
static int  sb_bg_is_changed ;


/* --- static functions --- */

static gint
fg_color_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	selected_fg_color = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	fg_is_changed = 1 ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s color is selected.\n" , selected_fg_color) ;
#endif

	return  1 ;
}

static gint
bg_color_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	selected_bg_color = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	bg_is_changed = 1 ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s color is selected.\n" , selected_bg_color) ;
#endif

	return  1 ;
}

static gint
sb_fg_color_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	selected_sb_fg_color = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	sb_fg_is_changed = 1 ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s color is selected.\n" , selected_sb_fg_color) ;
#endif

	return  1 ;
}

static gint
sb_bg_color_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	selected_sb_bg_color = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	sb_bg_is_changed = 1 ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s color is selected.\n" , selected_sb_bg_color) ;
#endif

	return  1 ;
}

static GtkWidget *
config_widget_new(
	char *  title ,
	char *  selected_color ,
	gint (*color_selected)(GtkWidget *,gpointer)
	)
{
	char *  colors[] =
	{
		"black" ,
		"red" ,
		"green" ,
		"yellow" ,
		"blue" ,
		"magenta" ,
		"cyan" ,
		"white" ,
		"gray" ,
		"lightgray" ,
		"pink" ,
		"brown" ,

	} ;
	
	return  mc_combo_new( title , colors , sizeof(colors) / sizeof(colors[0]) ,
			selected_color , 0 , color_selected , NULL) ;
}


/* --- global functions --- */

GtkWidget *
mc_fg_color_config_widget_new(
	char *  color
	)
{
	selected_fg_color = color ;
	
	return  config_widget_new( "Foreground color" , color , fg_color_selected) ;
}

GtkWidget *
mc_bg_color_config_widget_new(
	char *  color
	)
{
	selected_bg_color = color ;
	
	return  config_widget_new( "" , color , bg_color_selected) ;
}

GtkWidget *
mc_sb_fg_color_config_widget_new(
	char *  color
	)
{
	selected_sb_fg_color = color ;
	
	return  config_widget_new( "Foreground color" , color , sb_fg_color_selected) ;
}

GtkWidget *
mc_sb_bg_color_config_widget_new(
	char *  color
	)
{
	selected_sb_bg_color = color ;
	
	return  config_widget_new( "Background color" , color , sb_bg_color_selected) ;
}

char *
mc_get_fg_color(void)
{
	if( ! fg_is_changed)
	{
		return  NULL ;
	}
	
	fg_is_changed = 0 ;
	
	return  selected_fg_color ;
}

int mc_bg_color_ischanged(void)
{
    return bg_is_changed;
}

char *
mc_get_bg_color(void)
{
	bg_is_changed = 0 ;
	
	return  selected_bg_color ;
}

char *
mc_get_sb_fg_color(void)
{
	if( ! sb_fg_is_changed)
	{
		return  NULL ;
	}
	
	sb_fg_is_changed = 0 ;
	
	return  selected_sb_fg_color ;
}

char *
mc_get_sb_bg_color(void)
{
	if( ! sb_bg_is_changed)
	{
		return  NULL ;
	}
	
	sb_bg_is_changed = 0 ;
	
	return  selected_sb_bg_color ;
}
