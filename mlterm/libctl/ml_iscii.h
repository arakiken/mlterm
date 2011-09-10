/*
 *	$Id$
 */

#ifndef  __CTL_ML_ISCII_H__
#define  __CTL_ML_ISCII_H__


#include  "../ml_iscii.h"

#include  <mkf/mkf_charset.h>
#include  "../ml_char.h"


struct  ml_iscii_state
{
	u_int8_t *  num_of_chars_array ;
	u_int16_t  size ;

	int8_t  has_iscii ;

} ;


u_int  ml_iscii_shape( mkf_charset_t  cs , u_char *  dst , size_t  dst_size , u_char *  src) ;

ml_iscii_state_t  ml_iscii_new( void) ;

int  ml_iscii_delete( ml_iscii_state_t  state) ;

int  ml_iscii( ml_iscii_state_t  state , ml_char_t *  src , u_int  src_len) ;


#endif
