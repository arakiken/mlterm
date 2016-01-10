/*
 *	$Id$
 */

#ifndef  __ML_GSUB_H__
#define  __ML_GSUB_H__


#include  <kiklib/kik_types.h>		/* u_int/u_char */

#include  "ml_font.h"
#include  "ml_char.h"
#include  "ml_bidi.h"


typedef enum ml_gsub_attr
{
	GSUB_SCRIPT = 0 ,
	GSUB_FEATURES = 1 ,
	MAX_GSUB_ATTRS = 2 ,

} ml_gsub_attr_t ;

typedef struct  ml_gsub_state
{
	void *  term ;
	u_int8_t *  num_of_chars_array ;
	u_int16_t  size ;

	int8_t  has_gsub ;

} *  ml_gsub_state_t ;


void  ml_set_gsub_attr( char *  value , ml_gsub_attr_t  attr) ;

void  ml_gsub_set_shape_func(
	u_int  (*func1)( void * , u_int32_t * , u_int , u_int32_t * , u_int32_t * , u_int ,
		const char * , const char *) ,
	void *  (*func2)( void * , ml_font_t)) ;

u_int  ml_gsub_shape( void *  font , u_int32_t *  gsub , u_int32_t  gsub_len , u_int32_t *  cmap ,
		u_int32_t *  src , u_int32_t  src_len) ;

void *  ml_gsub_get_font( void *  term , ml_font_t  font) ;

ml_gsub_state_t  ml_gsub_new( void) ;

int  ml_gsub_delete( ml_gsub_state_t  state) ;

int  ml_gsub( ml_gsub_state_t  state , ml_char_t *  src , u_int  src_len) ;

int  ml_gsub_reset( ml_gsub_state_t  state) ;

int  ml_gsub_copy( ml_gsub_state_t  dst , ml_gsub_state_t  src , int  optimize) ;


#endif
