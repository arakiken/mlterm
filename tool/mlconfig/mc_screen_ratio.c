/*
 *	$Id$
 */

#include  "mc_screen_ratio.h"

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>
#include  <glib.h>

#include  "mc_combo.h"

#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  screen_ratios[] =
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

static char *  selected_screen_width_ratio ;
static char *  selected_screen_height_ratio ;


/* --- static functions --- */

static gint
screen_width_ratio_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	selected_screen_width_ratio = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	
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
	return  mc_combo_new( title , screen_ratios , sizeof(screen_ratios) / sizeof(screen_ratios[0]) ,
		ratio , 0 , ratio_selected , NULL) ;
}


/* --- global functions --- */

GtkWidget *
mc_screen_width_ratio_config_widget_new(
	char *  ratio
	)
{
	selected_screen_width_ratio = ratio ;

	return  config_widget_new( "Width ratio" , ratio , screen_width_ratio_selected) ;
}

GtkWidget *
mc_screen_height_ratio_config_widget_new(
	char *  ratio
	)
{
	selected_screen_height_ratio = ratio ;

	return  config_widget_new( "Height ratio" , ratio , screen_height_ratio_selected) ;
}

u_int
mc_get_screen_width_ratio(void)
{
	u_int  ratio ;
	
	if( ! kik_str_to_uint( &ratio , selected_screen_width_ratio))
	{
		kik_str_to_uint( &ratio , screen_ratios[0]) ;
	}
	
	return  ratio ;
}

u_int
mc_get_screen_height_ratio(void)
{
	u_int  ratio ;
	
	if( ! kik_str_to_uint( &ratio , selected_screen_height_ratio))
	{
		kik_str_to_uint( &ratio , screen_ratios[0]) ;
	}
	
	return  ratio ;
}
