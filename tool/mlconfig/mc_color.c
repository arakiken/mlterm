/*
 *	update: <2001/11/26(22:51:23)>
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
	"PRIVATE_FG" ,
	"PRIVATE_BG" ,
	
} ;


/* --- global functions --- */

GtkWidget *
mc_color_config_widget_new(
	ml_color_t  selected_color ,
	char *  title ,
	gint (*color_selected)(GtkWidget *,gpointer)
	)
{
	return  mc_combo_new( title , colors , sizeof(colors) / sizeof(colors[0]) ,
		colors[selected_color] , 1 , color_selected , NULL) ;
}

ml_color_t
mc_get_color(
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
