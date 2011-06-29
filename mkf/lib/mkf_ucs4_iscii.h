/*
 *	$Id$
 */

#ifndef  __MKF_UCS4_ISCII_H__
#define  __MKF_UCS4_ISCII_H__


#include  <kiklib/kik_types.h>	/* u_xxx */

#include  "mkf_char.h"


int  mkf_map_iscii_assamese_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  iscii_code) ;

int  mkf_map_iscii_bengali_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  iscii_code) ;

int  mkf_map_iscii_gujarati_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  iscii_code) ;

int  mkf_map_iscii_hindi_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  iscii_code) ;

int  mkf_map_iscii_kannada_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  iscii_code) ;

int  mkf_map_iscii_malayalam_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  iscii_code) ;

int  mkf_map_iscii_oriya_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  iscii_code) ;

int  mkf_map_iscii_punjabi_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  iscii_code) ;

int  mkf_map_iscii_roman_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  iscii_code) ;

int  mkf_map_iscii_tamil_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  iscii_code) ;

int  mkf_map_iscii_telugu_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  iscii_code) ;

int  mkf_map_ucs4_to_iscii( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;


#endif
