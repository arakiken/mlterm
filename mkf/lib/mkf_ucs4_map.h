/*
 *	$Id$
 */

#ifndef  __MKF_UCS4_MAP_H__
#define  __MKF_UCS4_MAP_H__


#include  <kiklib/kik_types.h>	/* u_xxx */

#include  "mkf_char.h"


#define  UCSBMP_IS_ALPHABET(ucs)	(0 <= (ucs) && (ucs) <= 33ff)
#define  UCSBMP_IS_CJK(ucs)		(0x3400 <= (ucs) && (ucs) <= 0x9fff)
#define  UCSBMP_IS_HANGUL(ucs)		(0xac00 <= (ucs) && (ucs) <= 0xd7ff)
#define  UCSBMP_IS_SURROGATE(ucs)	(0xd800 <= (ucs) && (ucs) <= 0xdfff)
#define  UCSBMP_IS_COMPAT(ucs)		(0xf900 <= (ucs) && (ucs) <= 0xfffd)


typedef  int  (*mkf_map_ucs4_to_func_t)( mkf_char_t * , u_int32_t) ;


int  mkf_map_ucs4_to_cs( mkf_char_t *  non_ucs , mkf_char_t *  ucs4 , mkf_charset_t  cs) ;

int  mkf_map_ucs4_to_with_funcs( mkf_char_t *  non_ucs , mkf_char_t *  ucs4 ,
	mkf_map_ucs4_to_func_t *  map_ucs4_to_funcs , size_t  list_size) ;
	
int  mkf_map_ucs4_to( mkf_char_t *  non_ucs , mkf_char_t *  ucs4) ;

int  mkf_map_ucs4_to_iso2022cs( mkf_char_t *  non_ucs , mkf_char_t *  ucs4) ;

int  mkf_map_to_ucs4( mkf_char_t *  ucs4 , mkf_char_t *  non_ucs) ;

int  mkf_map_via_ucs( mkf_char_t *  dst , mkf_char_t *  src , mkf_charset_t  cs) ;


#endif
