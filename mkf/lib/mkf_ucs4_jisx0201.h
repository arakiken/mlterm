/*
 *	update: <2001/11/11(18:01:01)>
 *	$Id$
 */

#ifndef  __MKF_UCS4_JISX0201_H__
#define  __MKF_UCS4_JISX0201_H__


#include  <kiklib/kik_types.h>	/* u_xxx */

#include  "mkf_char.h"


int  mkf_map_jisx0201_kata_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  jis) ;

int  mkf_map_jisx0201_roman_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  jis) ;

int  mkf_map_ucs4_to_jisx0201_kata( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;

int  mkf_map_ucs4_to_jisx0201_roman( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;


#endif
