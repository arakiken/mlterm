/*
 *	$Id$
 */

#include  "mkf_ucs4_cp1251.h"

#include  "table/mkf_cp1251_to_ucs4.table"
#include  "table/mkf_ucs4_alphabet_to_cp1251.table"


/* --- global functions --- */

int
mkf_map_cp1251_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  gp_code
	)
{
	u_int32_t  c ;

	if( ( c = CONV_CP1251_TO_UCS4(gp_code)))
	{
		mkf_int_to_bytes( ucs4->ch , 4 , c) ;
		ucs4->size = 4 ;
		ucs4->cs = ISO10646_UCS4_1 ;
		ucs4->property = 0 ;

		return  1 ;
	}
	else if( 0x20 <= gp_code && gp_code <= 0x7e)
	{
		ucs4->ch[0] = 0x0 ;
		ucs4->ch[1] = 0x0 ;
		ucs4->ch[2] = 0x0 ;
		ucs4->ch[3] = gp_code ;
		ucs4->size = 4 ;
		ucs4->cs = ISO10646_UCS4_1 ;
		ucs4->property = 0 ;

		return  1 ;
	}

	return  0 ;
}

int
mkf_map_ucs4_to_cp1251(
	mkf_char_t *  non_ucs ,
	u_int32_t  ucs4_code
	)
{
	u_int8_t  c ;

	if( ( c = CONV_UCS4_ALPHABET_TO_CP1251(ucs4_code)))
	{
		non_ucs->ch[0] = c ;
		non_ucs->size = 1 ;
		non_ucs->cs = CP1251 ;
		non_ucs->property = 0 ;

		return  1 ;
	}
	
	return  0 ;
}
