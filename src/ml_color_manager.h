/*
 *	$Id$
 */

#ifndef  __ML_COLOR_MANAGER_H__
#define  __ML_COLOR_MANAGER_H__


#include  <kiklib/kik_types.h>		/* u_long/u_short */

#include  "ml_color.h"


typedef struct  ml_rgb
{
	u_short  red ;
	u_short  green ;
	u_short  blue ;

} ml_rgb_t ;

typedef struct  ml_xcolor
{
	x_color_t  xcolor ;

	int8_t  is_loaded ;
	
} ml_xcolor_t ;

typedef struct  ml_color_manager
{
	Display *  display ;
	int  screen ;

	ml_xcolor_t  xcolors[MAX_ACTUAL_COLORS] ;
	ml_rgb_t *  rgbs[MAX_ACTUAL_COLORS] ;

} ml_color_manager_t ;


int  ml_color_manager_init( ml_color_manager_t *  color_man , Display *  display , int  screen) ;

int  ml_color_manager_final( ml_color_manager_t *  color_man) ;

int  ml_color_manager_read_conf( ml_color_manager_t *  color_man , char *  filename) ;

int  ml_color_manager_set_rgb( ml_color_manager_t *  color_man , ml_color_t  color ,
	u_short  red , u_short  green , u_short  blue) ;

int  ml_color_manager_load( ml_color_manager_t *  color_man) ;

int  ml_color_manager_change_rgb( ml_color_manager_t *  color_man , ml_color_t  color ,
	u_short  red , u_short  green , u_short  blue) ;
	
ml_color_table_t  ml_color_table_new( ml_color_manager_t *  color_man ,
	ml_color_t  fg_color , ml_color_t  bg_color) ;

int  ml_get_color_rgb( ml_color_manager_t *  color_man , u_short *  red ,
	u_short *  green , u_short *  blue , ml_color_t  color) ;


#endif
