/*
 *	$Id$
 */

#ifndef  __X_FONT_CUSTOM_H__
#define  __X_FONT_CUSTOM_H__


#include  <kiklib/kik_map.h>
#include  <kiklib/kik_types.h>

#include  "x_font.h"


KIK_MAP_TYPEDEF( x_font_name , ml_font_t , char *) ;

typedef struct  x_font_config
{
	/* public(readonly) */
	x_font_present_t  font_present ;

	/* private */
	KIK_MAP( x_font_name) *  font_name_table ;
	KIK_MAP( x_font_name)  default_font_name_table ;

	u_int  ref_count ;

} x_font_config_t ;


int  x_set_font_size_range( u_int  min_font_size , u_int  max_font_size) ;

u_int  x_get_min_font_size(void) ;

u_int  x_get_max_font_size(void) ;

x_font_config_t *  x_font_config_new( x_font_present_t  font_present) ;

int  x_font_config_delete( x_font_config_t *  font_config) ;

x_font_config_t *  x_acquire_font_config( x_font_present_t  font_present) ;

int  x_release_font_config( x_font_config_t *  font_config) ;

int  x_configize_font_name( x_font_config_t *  font_config , ml_font_t  font ,
	char *  fontname , u_int  font_size) ;

int  x_configize_default_font_name( x_font_config_t *  font_config , ml_font_t  font ,
	char *  fontname , u_int  font_size) ;

char *  x_get_config_font_name( x_font_config_t *  font_config , u_int  font_size , ml_font_t  font) ;

char *  x_get_all_config_font_names( x_font_config_t *  font_config , u_int  font_size) ;


#endif
