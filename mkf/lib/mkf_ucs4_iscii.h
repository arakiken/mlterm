/*
 *	$Id$
 */

#ifndef  __MKF_UCS4_ISCII_H__
#define  __MKF_UCS4_ISCII_H__


#include  <kiklib/kik_types.h>	/* u_xxx */

#include  "mkf_char.h"


int  mkf_set_iscii_lang_for_ucs_mapping( mkf_iscii_lang_t  lang) ;

int  mkf_map_iscii_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  iscii_code) ;

int  mkf_map_ucs4_to_iscii( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;


#endif
