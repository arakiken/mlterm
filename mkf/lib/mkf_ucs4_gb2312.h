/*
 *	$Id$
 */

#ifndef  __MKF_UCS4_GB2312_H__
#define  __MKF_UCS4_GB2312_H__


#include  <kiklib/kik_types.h>	/* u_xxx */

#include  "mkf_char.h"


int  mkf_map_gb2312_80_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  gb) ;

int  mkf_map_ucs4_to_gb2312_80( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;


#endif
