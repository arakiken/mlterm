/*
 *	$Id$
 */

#ifndef  __MKF_RU_MAP_H__
#define  __MKF_RU_MAP_H__


#include "mkf_char.h"


int  mkf_map_ucs4_to_ru( mkf_char_t *  ru , mkf_char_t *  ucs4) ;

int  mkf_map_koi8_r_to_iso8859_5_r( mkf_char_t *  iso8859 , mkf_char_t *  ru) ;

int  mkf_map_koi8_r_to_koi8_u( mkf_char_t *  koi8_u , mkf_char_t *  koi8_r) ;

int  mkf_map_koi8_u_to_koi8_r( mkf_char_t *  koi8_r , mkf_char_t *  koi8_u) ;


#endif
