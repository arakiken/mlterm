/*
 *	update: <2001/11/11(18:01:25)>
 *	$Id$
 */

#ifndef  __MKF_UCS4_KOI8_H__
#define  __MKF_UCS4_KOI8_H__


#include  <kiklib/kik_types.h>	/* u_xxx */

#include  "mkf_char.h"


int  mkf_map_koi8_r_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  koi8_code) ;

int  mkf_map_koi8_u_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  koi8_code) ;

int  mkf_map_ucs4_to_koi8_r( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;

int  mkf_map_ucs4_to_koi8_u( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;


#endif
