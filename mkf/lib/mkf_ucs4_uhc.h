/*
 *	$Id$
 */

#ifndef  __MKF_UCS4_UHC_H__
#define  __MKF_UCS4_UHC_H__


#include  <kiklib/kik_types.h>	/* u_xxx */

#include  "mkf_char.h"


int  mkf_map_uhc_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  ks) ;

int  mkf_map_ucs4_to_uhc( mkf_char_t *  ks , u_int32_t  ucs4_code) ;


#endif
