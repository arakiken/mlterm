/*
 *	$Id$
 */

#ifndef  __X_COLOR_CUSTOM_H__
#define  __X_COLOR_CUSTOM_H__


#include  <kiklib/kik_map.h>

#include  "x_color.h"


typedef struct x_rgb
{
	u_short  red ;
	u_short  green ;
	u_short  blue ;
	
} x_rgb_t ;

KIK_MAP_TYPEDEF( x_color_rgb , char * , x_rgb_t) ;

typedef struct x_color_custom
{
	KIK_MAP( x_color_rgb)  color_rgb_table ;
	
} x_color_custom_t ;


int  x_color_custom_init( x_color_custom_t *  color_custom) ;

int  x_color_custom_final( x_color_custom_t *  color_custom) ;

int  x_color_custom_read_conf( x_color_custom_t *  color_custom , char *  filename) ;

int  x_color_custom_set_rgb( x_color_custom_t *  color_custom , char *  color ,
	u_short  red , u_short  green , u_short  blue) ;

int  x_color_custom_get_rgb( x_color_custom_t *  color_custom ,
	u_short *  red , u_short *  green , u_short *  blue , char *  color) ;


#endif
