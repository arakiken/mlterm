/*
 *	$Id$
 */

#include  "mkf_ucs4_jisx0212.h"

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
	u_char *  c ;

	if( ( c = CONV_JISX0213_2000_1_TO_UCS4( jis)))
	{
		memcpy( ucs4->ch , c , 4) ;
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
	u_char *  c ;

	if( ( c = CONV_JISX0213_2000_2_TO_UCS4( jis)))
	{
		memcpy( ucs4->ch , c , 4) ;
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
	u_char *  c ;

	if( ( c = CONV_UCS4_ALPHABET_TO_JISX0213_2000_1(ucs4_code)) ||
		( c = CONV_UCS4_CJK_TO_JISX0213_2000_1(ucs4_code)) ||
		( c = CONV_UCS4_COMPAT_TO_JISX0213_2000_1(ucs4_code)) ||
		( c = CONV_UCS4_EXTENSION_A_TO_JISX0213_2000_1(ucs4_code)))
	{
		memcpy( jis->ch , c , 2) ;
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
	u_char *  c ;

	if( ( c = CONV_UCS4_CJK_TO_JISX0213_2000_2(ucs4_code)) ||
		( c = CONV_UCS4_COMPAT_TO_JISX0213_2000_2(ucs4_code)) ||
		( c = CONV_UCS4_EXTENSION_A_TO_JISX0213_2000_2(ucs4_code)))
	{
		memcpy( jis->ch , c , 2) ;
		jis->size = 2 ;
		jis->cs = JISX0213_2000_2 ;
		jis->property = 0 ;
		
		return  1 ;
	}

	return  0 ;
}
