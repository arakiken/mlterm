/*
 *	$Id$
 */

#include  "mkf_ucs4_usascii.h"


/* --- global functions --- */

int
mkf_map_us_ascii_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  ascii_code
	)
{
	if( ! (/* 0x00 <= ascii_code && */ ascii_code <= 0x7f))
	{
		return  0 ;
	}
	
	ucs4->ch[0] = '\x00' ;
	ucs4->ch[1] = '\x00' ;
	ucs4->ch[2] = '\x00' ;
	ucs4->ch[3] = ascii_code ;
	ucs4->size = 4 ;
	ucs4->cs = ISO10646_UCS4_1 ;
	ucs4->property = 0 ;

	return  1 ;
}

int
mkf_map_ucs4_to_us_ascii(
	mkf_char_t *  non_ucs ,
	u_int32_t  ucs4_code
	)
{
	if( ! (0x00 <= ucs4_code && ucs4_code <= 0x7f))
	{
		return  0 ;
	}

	non_ucs->ch[0] = ucs4_code ;
	non_ucs->size = 1 ;
	non_ucs->cs = US_ASCII ;
	non_ucs->property = 0 ;

	return  1 ;
}
