/*
 *	$Id$
 */

#include  "mkf_ucs4_gbk.h"

#include  "table/mkf_gbk_to_ucs4.table"
#include  "table/mkf_ucs4_alphabet_to_gbk.table"
#include  "table/mkf_ucs4_cjk_to_gbk.table"
#include  "table/mkf_ucs4_compat_to_gbk.table"


/* --- global functions --- */

int
mkf_map_gbk_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  gb
	)
{
	u_char *  c ;
	
	if( ( c = CONV_GBK_TO_UCS4(gb)))
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
mkf_map_ucs4_to_gbk(
	mkf_char_t *  gb ,
	u_int32_t  ucs4_code
	)
{
	u_char *  c ;

	if( ( c = CONV_UCS4_ALPHABET_TO_GBK(ucs4_code)) ||
		( c = CONV_UCS4_CJK_TO_GBK(ucs4_code)) ||
		( c = CONV_UCS4_COMPAT_TO_GBK(ucs4_code)))
	{
		memcpy( gb->ch , c , 2) ;
		gb->size = 2 ;
		gb->cs = GBK ;
		gb->property = 0 ;
		
		return  1 ;
	}

	return  0 ;
}
