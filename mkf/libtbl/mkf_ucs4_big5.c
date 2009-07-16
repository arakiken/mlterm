/*
 *	$Id$
 */

#include  "../lib/mkf_ucs4_big5.h"

#include  "table/mkf_big5_to_ucs4.table"
#include  "table/mkf_hkscs_to_ucs4.table"

#include  "table/mkf_ucs4_alphabet_to_big5.table"
#include  "table/mkf_ucs4_cjk_to_big5.table"
#include  "table/mkf_ucs4_compat_to_big5.table"
#include  "table/mkf_ucs4_pua_to_big5.table"
#include  "table/mkf_ucs4_alphabet_to_hkscs.table"
#include  "table/mkf_ucs4_cjk_to_hkscs.table"
#include  "table/mkf_ucs4_compat_to_hkscs.table"
#include  "table/mkf_ucs4_extension_a_to_hkscs.table"
#include  "table/mkf_ucs4_pua_to_hkscs.table"


/* --- global functions --- */

int
mkf_map_big5_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  big5
	)
{
	u_int32_t  c ;
	
	if( ( c = CONV_BIG5_TO_UCS4(big5)))
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
mkf_map_hkscs_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  hkscs
	)
{
	u_int32_t  c ;

	if( ( c = CONV_HKSCS_TO_UCS4(hkscs)))
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
mkf_map_ucs4_to_big5(
	mkf_char_t *  big5 ,
	u_int32_t  ucs4_code
	)
{
	u_int16_t  c ;

	if( ( c = CONV_UCS4_ALPHABET_TO_BIG5(ucs4_code)) ||
		( c = CONV_UCS4_CJK_TO_BIG5(ucs4_code)) ||
		( c = CONV_UCS4_COMPAT_TO_BIG5(ucs4_code)) ||
		( c = CONV_UCS4_PUA_TO_BIG5(ucs4_code)))
	{
		mkf_int_to_bytes( big5->ch , 2 , c) ;
		big5->size = 2 ;
		big5->cs = BIG5 ;
		big5->property = 0 ;
		
		return  1 ;
	}

	return  0 ;
}

int
mkf_map_ucs4_to_hkscs(
	mkf_char_t *  hkscs ,
	u_int32_t  ucs4_code
	)
{
	u_int16_t  c ;

	if( ( c = CONV_UCS4_ALPHABET_TO_HKSCS(ucs4_code)) ||
		( c = CONV_UCS4_CJK_TO_HKSCS(ucs4_code)) ||
		( c = CONV_UCS4_COMPAT_TO_HKSCS(ucs4_code)) ||
		( c = CONV_UCS4_EXTENSION_A_TO_HKSCS(ucs4_code)) ||
		( c = CONV_UCS4_PUA_TO_HKSCS(ucs4_code)))
	{
		mkf_int_to_bytes( hkscs->ch , 2 , c) ;
		hkscs->size = 2 ;
		hkscs->cs = HKSCS ;
		hkscs->property = 0 ;
		
		return  1 ;
	}

	return  0 ;
}
