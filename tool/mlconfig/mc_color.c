/*
 *	$Id$
 */

#include  "mc_color.h"

#include  <kiklib/kik_debug.h>
#include  <glib.h>
#include  <c_intl.h>

#include  "mc_combo.h"
#include  "mc_io.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  new_fg_color ;
static char *  new_bg_color ;
static char *  new_sb_fg_color ;
static char *  new_sb_bg_color ;
static char *  old_fg_color ;
static char *  old_bg_color ;
static char *  old_sb_fg_color ;
static char *  old_sb_bg_color ;


/* --- static functions --- */

static gint
fg_color_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	new_fg_color = gtk_entry_get_text(GTK_ENTRY(widget)) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s color is selected.\n" , new_fg_color) ;
#endif

	return  1 ;
}

static gint
bg_color_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	new_bg_color = gtk_entry_get_text(GTK_ENTRY(widget)) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s color is selected.\n" , new_bg_color) ;
#endif

	return  1 ;
}

static gint
sb_fg_color_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	new_sb_fg_color = gtk_entry_get_text(GTK_ENTRY(widget)) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s color is selected.\n" , new_sb_fg_color) ;
#endif

	return  1 ;
}

static gint
sb_bg_color_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	new_sb_bg_color = gtk_entry_get_text(GTK_ENTRY(widget)) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s color is selected.\n" , new_sb_bg_color) ;
#endif

	return  1 ;
}

static GtkWidget *
config_widget_new(
	char *  title ,
	char *  color ,
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
			color , 0 , color_selected , NULL) ;
}


/* --- global functions --- */

GtkWidget *
mc_fg_color_config_widget_new(void)
{
	new_fg_color = old_fg_color = mc_get_str_value( "fg_color") ;
	
	return  config_widget_new( _("Foreground color") , new_fg_color , fg_color_selected) ;
}

GtkWidget *
mc_bg_color_config_widget_new(void)
{
	new_bg_color = old_bg_color = mc_get_str_value( "bg_color") ;
	
	return  config_widget_new(_("Background color") , new_bg_color , bg_color_selected) ;
}

GtkWidget *
mc_sb_fg_color_config_widget_new(void)
{
	new_sb_fg_color = old_sb_fg_color = mc_get_str_value( "sb_fg_color") ;
	
	return  config_widget_new( _("Foreground color") , new_sb_fg_color , sb_fg_color_selected) ;
}

GtkWidget *
mc_sb_bg_color_config_widget_new(void)
{
	new_sb_bg_color = old_sb_bg_color = mc_get_str_value( "sb_bg_color") ;
	
	return  config_widget_new( _("Background color") , new_sb_bg_color , sb_bg_color_selected) ;
}

void
mc_update_fg_color(
	int  save
	)
{
	if( save)
	{
		mc_set_str_value( "fg_color" , new_fg_color , save) ;
	}
	else
	{
		old_fg_color = new_fg_color ;
		if( strcmp( new_fg_color , old_fg_color) != 0)
		{
			mc_set_str_value( "fg_color" , new_fg_color , save) ;
		}
	}
}

void
mc_update_bg_color(
	int  save
	)
{
	if( save)
	{
		mc_set_str_value( "bg_color" , new_bg_color , save) ;
	}
	else
	{
		old_bg_color = new_bg_color ;
		if( strcmp( new_bg_color , old_bg_color) != 0)
		{
			mc_set_str_value( "bg_color" , new_bg_color , save) ;
		}
	}
}

void
mc_update_sb_fg_color(
	int  save
	)
{
	if( save)
	{
		mc_set_str_value( "sb_fg_color" , new_sb_fg_color , save) ;
	}
	else
	{
		old_sb_fg_color = new_sb_fg_color ;
		if( strcmp( new_sb_fg_color , old_sb_fg_color) != 0)
		{
			mc_set_str_value( "sb_fg_color" , new_sb_fg_color , save) ;
		}
	}
}

void
mc_update_sb_bg_color(
	int  save
	)
{
	if( save)
	{
		mc_set_str_value( "sb_bg_color" , new_sb_bg_color , save) ;
	}
	else
	{
		if( strcmp( new_sb_bg_color , old_sb_bg_color) != 0)
		{
			mc_set_str_value( "sb_bg_color" , new_sb_bg_color , save) ;
			old_sb_bg_color = new_sb_bg_color ;
		}
	}
}
