/*
 *	$Id$
 */

#ifndef  __MKF_UCS4_BIG5_H__
#define  __MKF_UCS4_BIG5_H__


#include  <kiklib/kik_types.h>	/* u_xxx */

#include  "mkf_char.h"


#define  IS_BASIC_BIG5(code) \
	( ( 0xa140 <= (code) && (code) <= 0xa3bf) || \
		( 0xa440 <= (code) && (code) <= 0xc67e) || \
		( 0xc6a1 <= (code) && (code) <= 0xc8fe) || \
		( 0xc940 <= (code) && (code) <= 0xf9d5) )


int  mkf_map_big5_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  big5) ;

int  mkf_map_big5hkscs_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  big5) ;

int  mkf_map_ucs4_to_big5( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;

int  mkf_map_ucs4_to_big5hkscs( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;


#endif
