/*
 *	$Id$
 */

#ifndef  __X_FONT_CUSTOM_H__
#define  __X_FONT_CUSTOM_H__


#include  <kiklib/kik_map.h>
#include  <kiklib/kik_types.h>

#include  "x_font.h"


KIK_MAP_TYPEDEF( x_font_name , ml_font_t , char *) ;

typedef struct  x_font_custom
{
	KIK_MAP( x_font_name) *  font_name_table ;
	KIK_MAP( x_font_name)  default_font_name_table ;
	char *  default_font_name_cache ;

	/* public(readonly) */
	u_int  min_font_size ;
	u_int  max_font_size ;

} x_font_custom_t ;


int  x_font_custom_init( x_font_custom_t *  font_custom , u_int  min_font_size ,
	u_int  max_font_size) ;

int  x_font_custom_final( x_font_custom_t *  font_custom) ;

int  x_font_custom_read_conf( x_font_custom_t *  font_custom , char *  filename) ;

int  x_set_font_name( x_font_custom_t *  font_custom , ml_font_t  font ,
	char *  fontname , u_int  font_size) ;

char *  x_get_font_name( x_font_custom_t *  font_custom , u_int  font_size , ml_font_t  font) ;

char *  x_get_font_name_r( x_font_custom_t *  font_custom , u_int  font_size , ml_font_t  font); 

u_int  x_get_all_font_names( x_font_custom_t *  font_custom ,char ***  font_names , u_int  font_size) ;


#endif
