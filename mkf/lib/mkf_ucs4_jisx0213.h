/*
 *	update: <2001/11/11(18:01:14)>
 *	$Id$
 */

#ifndef  __MKF_UCS4_JISX0213_H__
#define  __MKF_UCS4_JISX0213_H__


#include  <kiklib/kik_types.h>	/* u_xxx */

#include  "mkf_char.h"


int  mkf_map_jisx0213_2000_1_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  jis) ;

int  mkf_map_jisx0213_2000_2_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  jis) ;

int  mkf_map_ucs4_to_jisx0213_2000_1( mkf_char_t *  jis , u_int32_t  ucs4_code) ;

int  mkf_map_ucs4_to_jisx0213_2000_2( mkf_char_t *  jis , u_int32_t  ucs4_code) ;


#endif
