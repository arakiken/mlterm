/*
 *	$Id$
 */

#ifndef  __ML_COLOR_CUSTOM_H__
#define  __ML_COLOR_CUSTOM_H__


#include  <kiklib/kik_map.h>

#include  "ml_color.h"


typedef struct ml_rgb
{
	u_short  red ;
	u_short  green ;
	u_short  blue ;
	
} ml_rgb_t ;

KIK_MAP_TYPEDEF( ml_color_rgb , char * , ml_rgb_t) ;

typedef struct ml_color_custom
{
	KIK_MAP( ml_color_rgb)  color_rgb_table ;
	
} ml_color_custom_t ;


int  ml_color_custom_init( ml_color_custom_t *  color_custom) ;

int  ml_color_custom_final( ml_color_custom_t *  color_custom) ;

int  ml_color_custom_read_conf( ml_color_custom_t *  color_custom , char *  filename) ;

int  ml_color_custom_set_rgb( ml_color_custom_t *  color_custom , char *  color ,
	u_short  red , u_short  green , u_short  blue) ;

int  ml_color_custom_get_rgb( ml_color_custom_t *  color_custom ,
	u_short *  red , u_short *  green , u_short *  blue , char *  color) ;


#endif
