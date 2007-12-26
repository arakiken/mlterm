/*
 *	$Id$
 */

#include  "mkf_ucs4_jisx0213.h"

#include  "table/mkf_ucs4_alphabet_to_jisx0213_2000_1.table"
#include  "table/mkf_ucs4_cjk_to_jisx0213_2000_1.table"
#include  "table/mkf_ucs4_compat_to_jisx0213_2000_1.table"
#include  "table/mkf_ucs4_extension_a_to_jisx0213_2000_1.table"
#include  "table/mkf_ucs4_cjk_to_jisx0213_2000_2.table"
#include  "table/mkf_ucs4_compat_to_jisx0213_2000_2.table"
#include  "table/mkf_ucs4_extension_a_to_jisx0213_2000_2.table"

#include  "table/mkf_jisx0213_2000_1_to_ucs4.table"
#include  "table/mkf_jisx0213_2000_2_to_ucs4.table"


/* --- global functions --- */

int
mkf_map_jisx0213_2000_1_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  jis
	)
{
	u_int32_t  c ;

	if( ( c = CONV_JISX0213_2000_1_TO_UCS4( jis)))
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
mkf_map_jisx0213_2000_2_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  jis
	)
{
	u_int32_t  c ;

	if( ( c = CONV_JISX0213_2000_2_TO_UCS4( jis)))
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
mkf_map_ucs4_to_jisx0213_2000_1(
	mkf_char_t *  jis ,
	u_int32_t  ucs4_code
	)
{
	u_int16_t  c ;

	if( ( c = CONV_UCS4_ALPHABET_TO_JISX0213_2000_1(ucs4_code)) ||
		( c = CONV_UCS4_CJK_TO_JISX0213_2000_1(ucs4_code)) ||
		( c = CONV_UCS4_COMPAT_TO_JISX0213_2000_1(ucs4_code)) ||
		( c = CONV_UCS4_EXTENSION_A_TO_JISX0213_2000_1(ucs4_code)))
	{
		mkf_int_to_bytes( jis->ch , 2 , c) ;
		jis->size = 2 ;
		jis->cs = JISX0213_2000_1 ;
		jis->property = 0 ;
		
		return  1 ;
	}

	return  0 ;
}

int
mkf_map_ucs4_to_jisx0213_2000_2(
	mkf_char_t *  jis ,
	u_int32_t  ucs4_code
	)
{
	u_int16_t  c ;

	if( ( c = CONV_UCS4_CJK_TO_JISX0213_2000_2(ucs4_code)) ||
		( c = CONV_UCS4_COMPAT_TO_JISX0213_2000_2(ucs4_code)) ||
		( c = CONV_UCS4_EXTENSION_A_TO_JISX0213_2000_2(ucs4_code)))
	{
		mkf_int_to_bytes( jis->ch , 2 , c) ;
		jis->size = 2 ;
		jis->cs = JISX0213_2000_2 ;
		jis->property = 0 ;
		
		return  1 ;
	}

	return  0 ;
}
