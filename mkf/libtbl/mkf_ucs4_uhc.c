/*
 *	$Id$
 */

#include  "../lib/mkf_ucs4_uhc.h"

#include  "table/mkf_uhc_to_ucs4.table"
#include  "table/mkf_ucs4_to_uhc.table"


/* --- global functions --- */

int
mkf_map_uhc_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  ks
	)
{
	u_int32_t  c ;
	
	if( ( c = CONV_UHC_TO_UCS4(ks)))
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
mkf_map_ucs4_to_uhc(
	mkf_char_t *  ks ,
	u_int32_t  ucs4_code
	)
{
	u_int16_t  c ;

	if( ( c = CONV_UCS4_TO_UHC(ucs4_code)))
	{
		mkf_int_to_bytes( ks->ch , 2 , c) ;
		ks->size = 2 ;
		ks->cs = UHC ;
		ks->property = 0 ;
		
		return  1 ;
	}

	return  0 ;
}
