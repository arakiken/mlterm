/*
 *	$Id$
 */

#ifndef  __MKF_UCS4_CNS11643_H__
#define  __MKF_UCS4_CNS11643_H__


#include  <kiklib/kik_types.h>	/* u_xxx */

#include  "mkf_char.h"


int  mkf_map_cns11643_1992_1_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  cns) ;

int  mkf_map_cns11643_1992_2_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  cns) ;

int  mkf_map_ucs4_to_cns11643_1992_1( mkf_char_t *  cns , u_int32_t  ucs4_code) ;

int  mkf_map_ucs4_to_cns11643_1992_2( mkf_char_t *  cns , u_int32_t  ucs4_code) ;


#endif
