/*
 *	update: <2001/11/11(18:01:06)>
 *	$Id$
 */

#ifndef  __MKF_UCS4_JISX0208_H__
#define  __MKF_UCS4_JISX0208_H__


#include  <kiklib/kik_types.h>	/* u_xxx */

#include  "mkf_char.h"


int  mkf_map_jisx0208_1983_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  jis) ;

int  mkf_map_nec_ext_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  nec_ext) ;

int  mkf_map_ucs4_to_jisx0208_1983( mkf_char_t *  jis , u_int32_t  ucs4_code) ;

int  mkf_map_ucs4_to_nec_ext( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;


#endif
