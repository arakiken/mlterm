/*
 *	$Id$
 */

#include  "mc_screen_ratio.h"

#include  <kiklib/kik_str.h>
#include  <kiklib/kik_mem.h>		/* free */
#include  <kiklib/kik_debug.h>
#include  <glib.h>
#include  <c_intl.h>

#include  "mc_combo.h"
#include  "mc_io.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  new_screen_width_ratio = NULL;
static char *  new_screen_height_ratio = NULL;
static char *  old_screen_width_ratio = NULL;
static char *  old_screen_height_ratio = NULL;
static int is_changed_width_ratio;
static int is_changed_height_ratio;


/* --- static functions --- */

static gint
screen_width_ratio_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	g_free( new_screen_width_ratio);
	new_screen_width_ratio = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1) ;
	
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
	g_free( new_screen_height_ratio);
	new_screen_height_ratio = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1) ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s screen_height_ratio is selected.\n" ,
		new_screen_height_ratio) ;
#endif

	return  1 ;
}

static GtkWidget *
config_widget_new(
	const char *  title ,
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
	new_screen_width_ratio = strdup( old_screen_width_ratio = mc_get_str_value( "screen_width_ratio")) ;
	is_changed_width_ratio = 0;

	return  config_widget_new(_("Width") , new_screen_width_ratio , screen_width_ratio_selected) ;
}

GtkWidget *
mc_screen_height_ratio_config_widget_new(void)
{
	new_screen_height_ratio = strdup( old_screen_height_ratio = mc_get_str_value( "screen_height_ratio")) ;
	is_changed_height_ratio = 0;

	return  config_widget_new(_("Height") , new_screen_height_ratio , screen_height_ratio_selected) ;
}

void
mc_update_screen_width_ratio(void)
{
	if (strcmp(new_screen_width_ratio, old_screen_width_ratio))
		is_changed_width_ratio = 1;

	if (is_changed_width_ratio)
	{
		mc_set_str_value( "screen_width_ratio" , new_screen_width_ratio) ;
		free( old_screen_width_ratio) ;
		old_screen_width_ratio = strdup( new_screen_width_ratio) ;
	}

}

void
mc_update_screen_height_ratio(void)
{
	if (strcmp(new_screen_height_ratio, old_screen_height_ratio))
		is_changed_height_ratio = 1;

	if (is_changed_height_ratio)
	{
		mc_set_str_value( "screen_height_ratio" , new_screen_height_ratio) ;
		free( old_screen_height_ratio) ;
		old_screen_height_ratio = strdup( new_screen_height_ratio) ;
	}
}
