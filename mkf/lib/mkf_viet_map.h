/*
 *	update: <2001/10/23(03:29:19)>
 *	$Id$
 */

#ifndef  __MKF_VIET_MAP_H__
#define  __MKF_VIET_MAP_H__


#include  "mkf_char.h"


int  mkf_map_ucs4_to_viet( mkf_char_t *  viet , mkf_char_t *  ucs4) ;

int  mkf_map_viscii_to_tcvn5712_3_1993( mkf_char_t *  tcvn , mkf_char_t *  viscii) ;

int  mkf_map_tcvn5712_3_1993_to_viscii( mkf_char_t *  viscii , mkf_char_t *  tcvn) ;


#endif
