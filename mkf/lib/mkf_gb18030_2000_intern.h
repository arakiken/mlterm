/*
 *	$Id$
 */

#ifndef  __MKF_GB18030_2000_INTERN_H__
#define  __MKF_GB18030_2000_INTERN_H__


#include  <kiklib/kik_types.h>	/* u_char */


int  mkf_decode_gb18030_2000_to_ucs4( u_char *  ucs4 , u_char *  gb18030) ;

int  mkf_encode_ucs4_to_gb18030_2000( u_char *  gb18030 , u_char *  ucs4) ;


#endif
