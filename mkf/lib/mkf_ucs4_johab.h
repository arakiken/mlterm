/*
 *	update: <2001/11/11(18:01:21)>
 *	$Id$
 */

#ifndef  __MKF_UCS4_JOHAB_H__
#define  __MKF_UCS4_JOHAB_H__


#include  <kiklib/kik_types.h>	/* u_xxx */

#include  "mkf_char.h"


int  mkf_map_johab_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  johab) ;

int  mkf_map_ucs4_to_johab( mkf_char_t *  johab , u_int32_t  ucs4_code) ;


#endif
