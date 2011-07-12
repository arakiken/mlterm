/*
 *	$Id$
 */

#ifndef  __ML_BIDI_H__
#define  __ML_BIDI_H__


#include  <kiklib/kik_types.h>		/* u_int */

#include  "ml_char.h"


typedef enum
{
	BIDI_NORMAL_MODE = 0 ,
	BIDI_CMD_MODE_L = 1 ,
	BIDI_CMD_MODE_R = 2 ,
	BIDI_MODE_MAX ,

} ml_bidi_mode_t ;

typedef struct  ml_bidi_state
{
	u_int16_t *  visual_order ;
	u_int16_t  size ;
	
	int8_t  base_is_rtl ;
	int8_t  has_rtl ;
	int8_t  bidi_mode ;	/* Cache how visual_order is rendered. */
	
} ml_bidi_state_t ;


#ifdef  USE_FRIBIDI

ml_bidi_state_t *  ml_bidi_new(void) ;

int  ml_bidi_copy( ml_bidi_state_t *  dst , ml_bidi_state_t *  src) ;

int  ml_bidi_delete( ml_bidi_state_t *  state) ;

int  ml_bidi_reset( ml_bidi_state_t *  state) ;

int  ml_bidi( ml_bidi_state_t *  state , ml_char_t *  src , u_int  size , ml_bidi_mode_t  mode) ;

#endif	/* USE_FRIBIDI */

ml_bidi_mode_t  ml_get_bidi_mode( const char *  name) ;

char *  ml_get_bidi_mode_name( ml_bidi_mode_t  mode) ;


#endif
