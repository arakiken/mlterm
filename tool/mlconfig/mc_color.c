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

/*
 * !!! Notice !!!
 * the order should be the same as ml_color_t in ml_color.h
 */
static char *  colors[] =
{
	"BLACK" ,
	"RED" ,
	"GREEN" ,
	"YELLOW" ,
	"BLUE" ,
	"MAGENTA" ,
	"CYAN" ,
	"WHITE" ,
	"GRAY" ,
	"LIGHTGRAY" ,
	"PINK" ,
	"BROWN" ,
	"PRIVATE_FG" ,
	"PRIVATE_BG" ,
	
} ;

static ml_color_t  selected_fg_color ;
static ml_color_t  selected_bg_color ;
static ml_color_t  selected_sb_fg_color ;
static ml_color_t  selected_sb_bg_color ;


/* --- static functions --- */

static ml_color_t
get_color(
	char *  name
	)
{
	int  counter ;
	
	for( counter = 0 ; counter < sizeof( colors) / sizeof( colors[0]) ; counter ++)
	{
		if( strcmp( name , colors[counter]) == 0)
		{
			return  counter ;
		}
	}
	
	return  -1 ;
}

static gint
fg_color_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	selected_fg_color = get_color( gtk_entry_get_text(GTK_ENTRY(widget))) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %d color is selected.\n" , selected_fg_color) ;
#endif

	return  1 ;
}

static gint
bg_color_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	selected_bg_color = get_color( gtk_entry_get_text(GTK_ENTRY(widget))) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %d color is selected.\n" , selected_bg_color) ;
#endif

	return  1 ;
}

static gint
sb_fg_color_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	selected_sb_fg_color = get_color( gtk_entry_get_text(GTK_ENTRY(widget))) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %d color is selected.\n" , selected_sb_fg_color) ;
#endif

	return  1 ;
}

static gint
sb_bg_color_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	selected_sb_bg_color = get_color( gtk_entry_get_text(GTK_ENTRY(widget))) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %d color is selected.\n" , selected_sb_bg_color) ;
#endif

	return  1 ;
}

static GtkWidget *
config_widget_new(
	char *  title ,
	ml_color_t  selected_color ,
	gint (*color_selected)(GtkWidget *,gpointer)
	)
{
	return  mc_combo_new( title , colors , sizeof(colors) / sizeof(colors[0]) ,
		colors[selected_color] , 1 , color_selected , NULL) ;
}


/* --- global functions --- */

GtkWidget *
mc_fg_color_config_widget_new(
	ml_color_t  color
	)
{
	selected_fg_color = color ;
	
	return  config_widget_new( "FG color" , color , fg_color_selected) ;
}

GtkWidget *
mc_bg_color_config_widget_new(
	ml_color_t  color
	)
{
	selected_bg_color = color ;
	
	return  config_widget_new( "BG color" , color , bg_color_selected) ;
}

GtkWidget *
mc_sb_fg_color_config_widget_new(
	ml_color_t  color
	)
{
	selected_sb_fg_color = color ;
	
	return  config_widget_new( "FG color" , color , sb_fg_color_selected) ;
}

GtkWidget *
mc_sb_bg_color_config_widget_new(
	ml_color_t  color
	)
{
	selected_sb_bg_color = color ;
	
	return  config_widget_new( "BG color" , color , sb_bg_color_selected) ;
}

ml_color_t
mc_get_fg_color(void)
{
	return  selected_fg_color ;
}

ml_color_t
mc_get_bg_color(void)
{
	return  selected_bg_color ;
}

ml_color_t
mc_get_sb_fg_color(void)
{
	return  selected_sb_fg_color ;
}

ml_color_t
mc_get_sb_bg_color(void)
{
	return  selected_sb_bg_color ;
}
