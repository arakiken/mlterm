/*
 *	update: <2001/10/23(03:28:22)>
 *	$Id$
 */

#ifndef  __MKF_KO_KR_MAP_H__
#define  __MKF_KO_KR_MAP_H__


#include  "mkf_char.h"


int  mkf_map_ucs4_to_ko_kr( mkf_char_t *  kokr , mkf_char_t *  ucs4) ;

int  mkf_map_uhc_to_ksc5601_1987( mkf_char_t *  ksc , mkf_char_t *  uhc) ;

int  mkf_map_ksc5601_1987_to_uhc( mkf_char_t *  uhc , mkf_char_t *  ksc) ;

int  mkf_map_johab_to_uhc( mkf_char_t *  uhc , mkf_char_t *  johab) ;

int  mkf_map_uhc_to_johab( mkf_char_t *  johab , mkf_char_t *  uhc) ;


#endif
