/*
 *	$Id$
 */

#ifndef  __X_COLOR_MANAGER_H__
#define  __X_COLOR_MANAGER_H__


#include  <kiklib/kik_types.h>		/* u_long/u_short */
#include  <ml_color.h>

#include  "x_color.h"
#include  "x_color_custom.h"


typedef struct  x_color_manager
{
	Display *  display ;
	int  screen ;

	int8_t  is_loaded[MAX_COLORS] ;
	x_color_t  colors[MAX_COLORS] ;
	x_color_t  highlighted_colors[MAX_COLORS] ;

	char *  fg_color ;
	char *  bg_color ;

	x_color_custom_t *  color_custom ;

	u_int8_t  fade_ratio ;
	int8_t  is_reversed ;

} x_color_manager_t ;


x_color_manager_t *  x_color_manager_new( Display *  display , int  screen ,
	x_color_custom_t *  color_custom , char *  fg_color , char *  bg_color) ;

int  x_color_manager_delete( x_color_manager_t *  color_man) ;

int  x_color_manager_set_fg_color( x_color_manager_t *  color_man , char *  fg_color) ;

int  x_color_manager_set_bg_color( x_color_manager_t *  color_man , char *  bg_color) ;

x_color_t *  x_get_color( x_color_manager_t *  color_man , ml_color_t  color) ;

char *  x_get_fg_color_name( x_color_manager_t *  color_man) ;

char *  x_get_bg_color_name( x_color_manager_t *  color_man) ;

int  x_color_manager_fade( x_color_manager_t *  color_man , u_int  fade_ratio) ;

int  x_color_manager_unfade( x_color_manager_t *  color_man) ;

int  x_color_manager_reverse_video( x_color_manager_t *  color_man) ;

int  x_color_manager_restore_video( x_color_manager_t *  color_man) ;


#endif
