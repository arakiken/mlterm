/*
 *	update: <2001/11/11(18:01:31)>
 *	$Id$
 */

#ifndef  __MKF_UCS4_KSC5601_H__
#define  __MKF_UCS4_KSC5601_H__


#include  <kiklib/kik_types.h>	/* u_xxx */

#include  "mkf_char.h"


int  mkf_map_ucs4_to_ksc5601_1987( mkf_char_t *  ks , u_int32_t  ucs4_code) ;

int  mkf_map_ksc5601_1987_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  ks) ;



#endif
