/*
 *	$Id$
 */

#ifndef  __ML_COLOR_CUSTOM_H__
#define  __ML_COLOR_CUSTOM_H__


#include  "ml_color.h"


typedef struct ml_color_custom
{
	struct
	{
		u_short  red ;
		u_short  green ;
		u_short  blue ;
		
	} * rgbs[MAX_ACTUAL_COLORS] ;

} ml_color_custom_t ;


int  ml_color_custom_init( ml_color_custom_t *  color_custom) ;

int  ml_color_custom_final( ml_color_custom_t *  color_custom) ;

int  ml_color_custom_read_conf( ml_color_custom_t *  color_custom , char *  filename) ;

int  ml_color_custom_set_rgb( ml_color_custom_t *  color_custom , ml_color_t  color ,
	u_short  red , u_short  green , u_short  blue) ;

int  ml_color_custom_get_rgb( ml_color_custom_t *  color_custom ,
	u_short *  red , u_short *  green , u_short *  blue , ml_color_t  color) ;


#endif
