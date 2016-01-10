/*
 *	$Id$
 */

#ifndef  __ML_BIDI_H__
#define  __ML_BIDI_H__


#include  <kiklib/kik_types.h>		/* u_int */


typedef enum
{
	BIDI_NORMAL_MODE = 0 ,
	BIDI_ALWAYS_LEFT = 1 ,
	BIDI_ALWAYS_RIGHT = 2 ,
	BIDI_MODE_MAX ,

} ml_bidi_mode_t ;

typedef struct ml_bidi_state *  ml_bidi_state_t ;


ml_bidi_mode_t  ml_get_bidi_mode( const char *  name) ;

char *  ml_get_bidi_mode_name( ml_bidi_mode_t  mode) ;

#if ! defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_FRIBIDI)
int  ml_is_rtl_char( u_int32_t  code) ;
#else
#define  ml_is_rtl_char( code)  (0)
#endif


#endif
