/*
 *	$Id$
 */

#ifndef  __MKF_UCS4_GEORGIAN_PS_H__
#define  __MKF_UCS4_GEORGIAN_PS_H__


#include  <kiklib/kik_types.h>	/* u_xxx */

#include  "mkf_char.h"


int  mkf_map_georgian_ps_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  gp_code) ;

int  mkf_map_ucs4_to_georgian_ps( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;


#endif
