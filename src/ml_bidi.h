/*
 *	$Id$
 */

#ifndef  __ML_BIDI_H__
#define  __ML_BIDI_H__


#include  <kiklib/kik_types.h>		/* u_int */

#include  "ml_char.h"


typedef struct  ml_bidi_state
{
	u_int16_t *  visual_order ;
	u_int16_t  size ;
	int8_t  base_is_rtl ;
	
} ml_bidi_state_t ;


ml_bidi_state_t *  ml_bidi_new(void) ;

int  ml_bidi_delete( ml_bidi_state_t *  state) ;

int  ml_bidi_reset( ml_bidi_state_t *  state) ;

int  ml_bidi( ml_bidi_state_t *  state , ml_char_t *  src , u_int  size , int  cursor_pos) ;


#endif
