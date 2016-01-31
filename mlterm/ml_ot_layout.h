/*
 *	$Id$
 */

#ifndef  __ML_OT_LAYOUT_H__
#define  __ML_OT_LAYOUT_H__


#include  <kiklib/kik_types.h>		/* u_int/u_char */

#include  "ml_font.h"
#include  "ml_char.h"


typedef enum ml_ot_layout_attr
{
	OT_SCRIPT = 0 ,
	OT_FEATURES = 1 ,
	MAX_OT_ATTRS = 2 ,

} ml_ot_layout_attr_t ;

typedef struct  ml_ot_layout_state
{
	void *  term ;
	u_int8_t *  num_of_chars_array ;
	u_int16_t  size ;

	int8_t  substituted ;

} *  ml_ot_layout_state_t ;


void  ml_set_ot_layout_attr( char *  value , ml_ot_layout_attr_t  attr) ;

void  ml_ot_layout_set_shape_func(
	u_int  (*func1)( void * , u_int32_t * , u_int , int8_t * , u_int8_t * ,
		u_int32_t * , u_int32_t * , u_int , const char * , const char *) ,
	void *  (*func2)( void * , ml_font_t)) ;

u_int  ml_ot_layout_shape( void *  font , u_int32_t *  shaped , u_int32_t  shaped_len ,
		int8_t *  offsets , u_int8_t *  widths , u_int32_t *  cmapped ,
		u_int32_t *  src , u_int32_t  src_len) ;

void *  ml_ot_layout_get_font( void *  term , ml_font_t  font) ;

ml_ot_layout_state_t  ml_ot_layout_new( void) ;

int  ml_ot_layout_delete( ml_ot_layout_state_t  state) ;

int  ml_ot_layout( ml_ot_layout_state_t  state , ml_char_t *  src , u_int  src_len) ;

int  ml_ot_layout_reset( ml_ot_layout_state_t  state) ;

int  ml_ot_layout_copy( ml_ot_layout_state_t  dst , ml_ot_layout_state_t  src , int  optimize) ;


#endif
