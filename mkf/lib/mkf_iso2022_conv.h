/*
 *	$Id$
 */

#ifndef  __MKF_ISO2022_CONV_H__
#define  __MKF_ISO2022_CONV_H__


#include  "mkf_conv.h"


typedef struct  mkf_iso2022_conv
{
	mkf_conv_t  conv ;
	
	mkf_charset_t *  gl ;
	mkf_charset_t *  gr ;
	
	mkf_charset_t  g0 ;
	mkf_charset_t  g1 ;
	mkf_charset_t  g2 ;
	mkf_charset_t  g3 ;
	
} mkf_iso2022_conv_t ;


size_t  mkf_iso2022_illegal_char( mkf_conv_t *  conv , u_char *  dst , size_t  dst_size , int *  is_full ,
		mkf_char_t *  ch) ;


#endif
