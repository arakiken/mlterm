/*
 *	$Id$
 */

#ifndef  __X_COLOR_MANAGER_H__
#define  __X_COLOR_MANAGER_H__


#include  <kiklib/kik_types.h>		/* u_long/u_short */
#include  <ml_color.h>

#include  "x_color.h"
#include  "x_color_config.h"
#include  "x_color_cache.h"


typedef struct  x_color_manager
{
	x_color_cache_t *  color_cache ;
	
	/* for fg, bg, cursor_fg and cursor_bg */
	struct sys_color
	{
		x_color_t  xcolor ;
		int8_t  is_loaded ;
		
		char *  name ;
	
	} sys_colors[4] ;
	
	int8_t  is_reversed ;

} x_color_manager_t ;


x_color_manager_t *  x_color_manager_new( Display *  display , int  screen ,
	x_color_config_t *  color_config , char *  fg_color , char *  bg_color ,
	char *  cursor_fg_color , char *  cursor_bg_color) ;

int  x_color_manager_delete( x_color_manager_t *  color_man) ;

int  x_color_manager_set_fg_color( x_color_manager_t *  color_man , char *  name) ;

int  x_color_manager_set_bg_color( x_color_manager_t *  color_man , char *  name) ;

int  x_color_manager_set_cursor_fg_color( x_color_manager_t *  color_man , char *  name) ;

int  x_color_manager_set_cursor_bg_color( x_color_manager_t *  color_man , char *  name) ;

char *  x_color_manager_get_fg_color( x_color_manager_t *  color_man) ;

char *  x_color_manager_get_bg_color( x_color_manager_t *  color_man) ;

char *  x_color_manager_get_cursor_fg_color( x_color_manager_t *  color_man) ;

char *  x_color_manager_get_cursor_bg_color( x_color_manager_t *  color_man) ;

x_color_t *  x_get_xcolor( x_color_manager_t *  color_man , ml_color_t  color) ;

int  x_color_manager_fade( x_color_manager_t *  color_man , u_int  fade_ratio) ;

int  x_color_manager_unfade( x_color_manager_t *  color_man) ;

int  x_color_manager_reverse_video( x_color_manager_t *  color_man) ;

int  x_color_manager_restore_video( x_color_manager_t *  color_man) ;

int  x_color_manager_adjust_cursor_fg( x_color_manager_t *  color_man) ;

int  x_color_manager_adjust_cursor_bg( x_color_manager_t *  color_man) ;

int  x_color_manager_unload( x_color_manager_t *  color_man) ;


#endif
