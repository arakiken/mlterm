/*
 *	$Id$
 */

#ifndef  __MKF_UCS4_JISX0212_H__
#define  __MKF_UCS4_JISX0212_H__


#include  <kiklib/kik_types.h>	/* u_xxx */

#include  "mkf_char.h"


int  mkf_map_jisx0212_1990_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  jis) ;

int  mkf_map_ucs4_to_jisx0212_1990( mkf_char_t *  jis , u_int32_t  ucs4_code) ;


#endif
