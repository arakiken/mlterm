/*
 *	$Id$
 */

#ifndef  __X_FONT_CONFIG_H__
#define  __X_FONT_CONFIG_H__


#include  <kiklib/kik_map.h>
#include  <kiklib/kik_types.h>

#include  "x_font.h"


KIK_MAP_TYPEDEF( x_font_name , ml_font_t , char *) ;

typedef struct  x_font_config
{
	/* Public(readonly) */
	x_type_engine_t  type_engine ;
	x_font_present_t  font_present ;

	/*
	 * Private
	 * font_configs whose difference is only FONT_AA share these members.
	 */
	KIK_MAP( x_font_name)  font_name_table ;

	u_int  ref_count ;

} x_font_config_t ;


x_font_config_t *  x_acquire_font_config( x_type_engine_t  type_engine , x_font_present_t  font_present) ;

int  x_release_font_config( x_font_config_t *  font_config) ;

x_font_config_t *  x_font_config_new( x_type_engine_t  type_engine , x_font_present_t  font_present) ;

int  x_font_config_delete( x_font_config_t *  font_config) ;

int  x_customize_font_file( const char *  file, char *  key, char *  value, int  save) ;

char *  x_get_config_font_name( x_font_config_t *  font_config , u_int  font_size , ml_font_t  font) ;

char *  x_get_config_font_name2( const char *  file , u_int  font_size , char *  font_cs) ;

char *  x_get_all_config_font_names( x_font_config_t *  font_config , u_int  font_size) ;

char *  x_get_charset_name( mkf_charset_t  cs) ;


#endif
