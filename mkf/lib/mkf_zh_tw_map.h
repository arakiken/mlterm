/*
 *	update: <2001/10/23(03:29:51)>
 *	$Id$
 */

#ifndef  __MKF_ZH_TW_MAP_H__
#define  __MKF_ZH_TW_MAP_H__


#include  "mkf_char.h"


int  mkf_map_ucs4_to_zh_tw( mkf_char_t *  zhtw , mkf_char_t *  ucs4) ;

int  mkf_map_big5_to_cns11643_1992( mkf_char_t *  cns , mkf_char_t *  big5) ;

int  mkf_map_cns11643_1992_1_to_big5( mkf_char_t *  big5 , mkf_char_t *  cns) ;

int  mkf_map_cns11643_1992_2_to_big5( mkf_char_t *  big5 , mkf_char_t *  cns) ;


#endif
