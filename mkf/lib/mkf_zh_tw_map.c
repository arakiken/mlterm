/*
 *	$Id$
 */

#include  "mkf_zh_tw_map.h"

#include  <kiklib/kik_debug.h>

#include  "mkf_ucs4_map.h"
#include  "mkf_ucs4_usascii.h"
#include  "mkf_ucs4_cns11643.h"
#include  "mkf_ucs4_big5.h"


/* --- static variables --- */

static mkf_map_ucs4_to_func_t  map_ucs4_to_funcs[] =
{
	mkf_map_ucs4_to_us_ascii ,
	mkf_map_ucs4_to_big5 ,
	mkf_map_ucs4_to_cns11643_1992_1 ,
	mkf_map_ucs4_to_cns11643_1992_2 ,
	mkf_map_ucs4_to_big5hkscs ,
} ;


/* --- global functions --- */

int
mkf_map_ucs4_to_zh_tw(
	mkf_char_t *  zhtw ,
	mkf_char_t *  ucs4
	)
{
	return  mkf_map_ucs4_to_with_funcs( zhtw , ucs4 , map_ucs4_to_funcs ,
		sizeof( map_ucs4_to_funcs) / sizeof( map_ucs4_to_funcs[0])) ;
}

int
mkf_map_big5_to_cns11643_1992(
	mkf_char_t *  cns ,
	mkf_char_t *  big5
	)
{
	u_int16_t  big5_code ;
	u_int32_t  ucs4_code ;

	mkf_char_t  ch ;

	big5_code = mkf_char_to_int( big5) ;

	if( mkf_map_big5_to_ucs4( &ch , big5_code) == 0)
	{
		return  0 ;
	}

	ucs4_code = mkf_char_to_int( &ch) ;

	if( ( 0xa140 <= big5_code && big5_code <= 0xa3bf) ||
		( 0xa440 <= big5_code && big5_code <= 0xc67e))
	{
		if( mkf_map_ucs4_to_cns11643_1992_1( &ch , ucs4_code) == 0)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG ,
				" u%.4x is not mapped to cns11643_1992.\n" , ucs4_code) ;
		#endif
		
			return  0 ;
		}
	}
	else if( 0xc940 <= big5_code && big5_code <= 0xf9d5)
	{
		if( mkf_map_ucs4_to_cns11643_1992_2( &ch , ucs4_code) == 0)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG ,
				" u%.4x is not mapped to cns11643_1992.\n" , ucs4_code) ;
		#endif
		
			return  0 ;
		}
	}
	else
	{
		/*
		 * XXX
		 * hmmm , how to do with c6a1 - c8fe(vendor dependent chars) ?
		 */

		return  0 ;
	}

	*cns = ch ;

	return  1 ;
}

int
mkf_map_cns11643_1992_1_to_big5(
	mkf_char_t *  big5 ,
	mkf_char_t *  cns
	)
{
	u_int16_t  cns_code ;
	u_int32_t  ucs4_code ;
	mkf_char_t  ch ;

	cns_code = (u_int16_t) mkf_char_to_int( cns) ;

	if( mkf_map_cns11643_1992_1_to_ucs4( &ch , cns_code) == 0)
	{
		return  0 ;
	}

	ucs4_code = mkf_char_to_int( &ch) ;

	if( mkf_map_ucs4_to_big5( &ch , ucs4_code) == 0)
	{
		return  0 ;
	}

	*big5 = ch ;

	return  1 ;
}

int
mkf_map_cns11643_1992_2_to_big5(
	mkf_char_t *  big5 ,
	mkf_char_t *  cns
	)
{
	u_int16_t  cns_code ;
	u_int32_t  ucs4_code ;
	mkf_char_t  ch ;

	cns_code = (u_int16_t) mkf_char_to_int( cns) ;

	if( mkf_map_cns11643_1992_2_to_ucs4( &ch , cns_code) == 0)
	{
		return  0 ;
	}

	ucs4_code = mkf_char_to_int( &ch) ;

	if( mkf_map_ucs4_to_big5( &ch , ucs4_code) == 0)
	{
		return  0 ;
	}

	*big5 = ch ;

	return  1 ;
}

int
mkf_map_big5hkscs_to_big5(
	mkf_char_t *  big5 ,
	mkf_char_t *  big5hkscs
	)
{
	u_int16_t  big5_code ;

	big5_code = mkf_bytes_to_int( big5->ch , big5->size) ;

	if( IS_BASIC_BIG5(big5_code))
	{
		return  0 ;
	}

	big5 = big5hkscs ;
	big5->cs = BIG5 ;

	return  1 ;
}

int
mkf_map_big5_to_big5hkscs(
	mkf_char_t *  big5hkscs ,
	mkf_char_t *  big5
	)
{
	big5hkscs = big5 ;
	big5hkscs->cs = BIG5HKSCS ;

	return  1 ;
}
