/*
 *	$Id$
 */

#ifndef  __MKF_UCS4_TCVN5712_1_H__
#define  __MKF_UCS4_TCVN5712_1_H__


#include  <kiklib/kik_types.h>	/* u_xxx */

#include  "mkf_char.h"


int  mkf_map_tcvn5712_1_1993_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  tcvn_code) ;

int  mkf_map_ucs4_to_tcvn5712_1_1993( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;


#endif
