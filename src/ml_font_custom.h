/*
 *	$Id$
 */

#ifndef  __ML_FONT_CUSTOM_H__
#define  __ML_FONT_CUSTOM_H__


#include  <kiklib/kik_map.h>
#include  <kiklib/kik_types.h>

#include  "ml_font.h"


KIK_MAP_TYPEDEF( ml_font_name , ml_font_attr_t , char *) ;

typedef struct  ml_font_custom
{
	KIK_MAP( ml_font_name) *  font_name_table ;

	/* public(readonly) */
	u_int  min_font_size ;
	u_int  max_font_size ;

} ml_font_custom_t ;


int  ml_font_custom_init( ml_font_custom_t *  font_custom , u_int  min_font_size ,
	u_int  max_font_size) ;

int  ml_font_custom_final( ml_font_custom_t *  font_custom) ;

int  ml_font_custom_read_conf( ml_font_custom_t *  font_custom , char *  filename) ;

int  ml_set_font_name( ml_font_custom_t *  font_custom , ml_font_attr_t  fontattr ,
	char *  fontname , u_int  font_size) ;

char *  ml_get_font_name_for_attr( ml_font_custom_t *  font_custom , u_int  font_size ,
	ml_font_attr_t  attr) ;

u_int  ml_get_all_font_names( ml_font_custom_t *  font_custom ,char ***  font_names , u_int  font_size) ;


#endif
