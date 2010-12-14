/*
 *	$Id$
 */

#ifndef  __X_COLOR_CONFIG_H__
#define  __X_COLOR_CONFIG_H__


#include  <kiklib/kik_map.h>

#include  "x_color.h"


typedef struct x_rgb
{
	u_int8_t  red ;
	u_int8_t  green ;
	u_int8_t  blue ;
	u_int8_t  alpha ;
	
} x_rgb_t ;

KIK_MAP_TYPEDEF( x_color_rgb , char * , x_rgb_t) ;

typedef struct x_color_config
{
	KIK_MAP( x_color_rgb)  color_rgb_table ;
	
} x_color_config_t ;


int  x_color_config_init( x_color_config_t *  color_config) ;

int  x_color_config_final( x_color_config_t *  color_config) ;

int  x_color_config_set_rgb( x_color_config_t *  color_config , char *  color ,
	u_int8_t  red , u_int8_t  green , u_int8_t  blue , u_int8_t  alpha) ;

int  x_color_config_get_rgb( x_color_config_t *  color_config , u_int8_t *  red ,
	u_int8_t *  green , u_int8_t *  blue , u_int8_t *  alpha , char *  color) ;

int  x_customize_color_file( x_color_config_t *  color_config,
	char *  color, char *  rgb, int  save) ;


#endif
