/*
 *	update: <2001/11/11(18:00:57)>
 *	$Id$
 */

#ifndef  __MKF_UCS4_ISO8859_H__
#define  __MKF_UCS4_ISO8859_H__


#include  <kiklib/kik_types.h>	/* u_xxx */

#include  "mkf_char.h"


int  mkf_map_ucs4_to_iso8859_1_r( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;

int  mkf_map_ucs4_to_iso8859_2_r( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;

int  mkf_map_ucs4_to_iso8859_3_r( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;

int  mkf_map_ucs4_to_iso8859_4_r( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;

int  mkf_map_ucs4_to_iso8859_5_r( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;

int  mkf_map_ucs4_to_iso8859_6_r( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;

int  mkf_map_ucs4_to_iso8859_7_r( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;

int  mkf_map_ucs4_to_iso8859_8_r( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;

int  mkf_map_ucs4_to_iso8859_9_r( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;

int  mkf_map_ucs4_to_iso8859_10_r( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;

int  mkf_map_ucs4_to_tis620_2533( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;

int  mkf_map_ucs4_to_iso8859_13_r( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;

int  mkf_map_ucs4_to_iso8859_14_r( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;

int  mkf_map_ucs4_to_iso8859_15_r( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;

int  mkf_map_ucs4_to_iso8859_16_r( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;

int  mkf_map_ucs4_to_tcvn5712_3_1993( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;

int  mkf_map_iso8859_1_r_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  iso8859_code) ;

int  mkf_map_iso8859_2_r_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  iso8859_code) ;

int  mkf_map_iso8859_3_r_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  iso8859_code) ;

int  mkf_map_iso8859_4_r_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  iso8859_code) ;

int  mkf_map_iso8859_5_r_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  iso8859_code) ;

int  mkf_map_iso8859_6_r_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  iso8859_code) ;

int  mkf_map_iso8859_7_r_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  iso8859_code) ;

int  mkf_map_iso8859_8_r_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  iso8859_code) ;

int  mkf_map_iso8859_9_r_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  iso8859_code) ;

int  mkf_map_iso8859_10_r_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  iso8859_code) ;

int  mkf_map_tis620_2533_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  tis620_code) ;

int  mkf_map_iso8859_13_r_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  iso8859_code) ;

int  mkf_map_iso8859_14_r_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  iso8859_code) ;

int  mkf_map_iso8859_15_r_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  iso8859_code) ;

int  mkf_map_iso8859_16_r_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  iso8859_code) ;

int  mkf_map_tcvn5712_3_1993_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  tcvn_code) ;


#endif
