/*
 *	$Id$
 */

#include  "mkf_ucs4_jisx0208.h"

#include  "table/mkf_jisx0208_1983_to_ucs4.table"
#include  "table/mkf_jisx0208_ext_to_ucs4.table"

#include  "table/mkf_ucs4_alphabet_to_jisx0208_1983.table"
#include  "table/mkf_ucs4_cjk_to_jisx0208_1983.table"
#include  "table/mkf_ucs4_compat_to_jisx0208_1983.table"
#include  "table/mkf_ucs4_to_jisx0208_ext.table"


/* --- global functions --- */

int
mkf_map_jisx0208_1983_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  jis
	)
{
	u_int32_t  c ;
	
	if( ( c = CONV_JISX0208_1983_TO_UCS4(jis)))
	{
		mkf_int_to_bytes( ucs4->ch , 4 , c) ;
		ucs4->size = 4 ;
		ucs4->cs = ISO10646_UCS4_1 ;
		ucs4->property = 0 ;

		return  1 ;
	}

	return  0 ;
}

int
mkf_map_nec_ext_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  nec_ext
	)
{
	u_int16_t  c ;

	if( nec_ext < 0x2d21 || 0x2d7c < nec_ext)
	{
		return  0 ;
	}

	if( ( c = nec_ext_to_ucs4_table[ nec_ext - 0x2d21]) == 0x0)
	{
		return  0 ;
	}

	mkf_int_to_bytes( ucs4->ch , 4 , c) ;
	ucs4->size = 4 ;
	ucs4->cs = ISO10646_UCS4_1 ;
	ucs4->property = 0 ;

	return  1 ;
}

int
mkf_map_ucs4_to_jisx0208_1983(
	mkf_char_t *  jis ,
	u_int32_t  ucs4_code
	)
{
	u_int16_t  c ;

	if( ( c = CONV_UCS4_ALPHABET_TO_JISX0208_1983(ucs4_code)) ||
		( c = CONV_UCS4_CJK_TO_JISX0208_1983(ucs4_code)) ||
		( c = CONV_UCS4_COMPAT_TO_JISX0208_1983(ucs4_code)))
	{
		mkf_int_to_bytes( jis->ch , 2 , c) ;
		jis->size = 2 ;
		jis->cs = JISX0208_1983 ;
		jis->property = 0 ;

		return  1 ;
	}

	return  0 ;
}

int
mkf_map_ucs4_to_nec_ext(
	mkf_char_t *  non_ucs ,
	u_int32_t  ucs4_code
	)
{
	u_int16_t  c ;
	
	if( 0x2460 <= ucs4_code && ucs4_code <= 0x2473)
	{
		non_ucs->ch[0] = 0x2d ;
		non_ucs->ch[1] = (ucs4_code - 0x3f) & 0xff ;
	}
	else if( 0x2160 <= ucs4_code && ucs4_code <= 0x2169)
	{
		non_ucs->ch[0] = 0x2d ;
		non_ucs->ch[1] = (ucs4_code - 0x2b) & 0xff ;
	}
	else if( ucs4_code == 0x2116)
	{
		memcpy( non_ucs->ch , "\x2d\x62" , 2) ;
	}
	else if( ucs4_code == 0x2121)
	{
		memcpy( non_ucs->ch , "\x2d\x64" , 2) ;
	}
	else if( 0x2211 <= ucs4_code && ucs4_code <= 0x2235)
	{
		if( ( c = ucs4_sign_to_nec_ext_table1[ ucs4_code - 0x2211]) == 0x0)
		{
			return  0 ;
		}

		mkf_int_to_bytes( non_ucs->ch , 2 , c) ;
	}
	else if( ucs4_code == 0x2252)
	{
		memcpy( non_ucs->ch , "\x2d\x70" , 2) ;
	}
	else if( ucs4_code == 0x2261)
	{
		memcpy( non_ucs->ch , "\x2d\x71" , 2) ;
	}
	else if( ucs4_code == 0x22a5)
	{
		memcpy( non_ucs->ch , "\x2d\x76" , 2) ;
	}
	else if( ucs4_code == 0x22bf)
	{
		memcpy( non_ucs->ch , "\x2d\x79" , 2) ;
	}
	else if( 0x3303 <= ucs4_code && ucs4_code <= 0x339e)
	{
		if( ( c = ucs4_sign_to_nec_ext_table2[ ucs4_code - 0x3303]) == 0x0)
		{
			return  0 ;
		}

		mkf_int_to_bytes( non_ucs->ch , 2 , c) ;
	}
	else
	{
		return  0 ;
	}

	non_ucs->cs = JISC6226_1978_NEC_EXT ;
	non_ucs->size = 2 ;
	non_ucs->property = 0 ;

	return  1 ;
}
