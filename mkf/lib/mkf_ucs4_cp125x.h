/*
 *	$Id$
 */

#ifndef  __MKF_UCS4_CP125X_H__
#define  __MKF_UCS4_CP125X_H__


#include  <kiklib/kik_types.h>	/* u_xxx */

#include  "mkf_char.h"


int  mkf_map_cp1250_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  cp_code) ;

int  mkf_map_cp1251_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  cp_code) ;

int  mkf_map_cp1252_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  cp_code) ;

int  mkf_map_cp1253_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  cp_code) ;

int  mkf_map_cp1254_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  cp_code) ;

int  mkf_map_cp1255_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  cp_code) ;

int  mkf_map_cp1256_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  cp_code) ;

int  mkf_map_cp1257_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  cp_code) ;

int  mkf_map_cp1258_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  cp_code) ;

int  mkf_map_cp874_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  cp_code) ;


int  mkf_map_ucs4_to_cp1250( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;

int  mkf_map_ucs4_to_cp1251( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;

int  mkf_map_ucs4_to_cp1252( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;

int  mkf_map_ucs4_to_cp1253( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;

int  mkf_map_ucs4_to_cp1254( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;

int  mkf_map_ucs4_to_cp1255( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;

int  mkf_map_ucs4_to_cp1256( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;

int  mkf_map_ucs4_to_cp1257( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;

int  mkf_map_ucs4_to_cp1258( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;

int  mkf_map_ucs4_to_cp874( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;


#endif
