/*
 *	update: <2001/11/26(02:44:28)>
 *	$Id$
 */

#include  "mkf_ucs4_big5.h"

#include  "table/mkf_big5_to_ucs4.table"

#include  "table/mkf_ucs4_alphabet_to_big5.table"
#include  "table/mkf_ucs4_cjk_to_big5.table"
#include  "table/mkf_ucs4_compat_to_big5.table"


/* --- global functions --- */

int
mkf_map_big5_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  big5
	)
{
	u_char *  c ;
	
	if( ( c = CONV_BIG5_TO_UCS4(big5)))
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
mkf_map_ucs4_to_big5(
	mkf_char_t *  big5 ,
	u_int32_t  ucs4_code
	)
{
	u_char *  c ;

	if( ( c = CONV_UCS4_ALPHABET_TO_BIG5(ucs4_code)) ||
		( c = CONV_UCS4_CJK_TO_BIG5(ucs4_code)) ||
		( c = CONV_UCS4_COMPAT_TO_BIG5(ucs4_code)))
	{
		memcpy( big5->ch , c , 2) ;
		big5->size = 2 ;
		big5->cs = BIG5 ;
		big5->property = 0 ;
		
		return  1 ;
	}

	return  0 ;
}
