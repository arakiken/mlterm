/*
 *	$Id$
 */

#include  "mkf_ucs4_ksc5601.h"

#include  "mkf_ucs4_uhc.h"


/* --- global functions --- */

int
mkf_map_ksc5601_1987_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  ks
	)
{
	/* converting to UHC */
	ks |= 0x8080 ;
	
	if( mkf_map_uhc_to_ucs4( ucs4 , ks))
	{
		return  1 ;
	}

	return  0 ;
}

int
mkf_map_ucs4_to_ksc5601_1987(
	mkf_char_t *  ks ,
	u_int32_t  ucs4_code
	)
{
	if( mkf_map_ucs4_to_uhc( ks , ucs4_code))
	{
		if( ks->ch[0] >= 0xa1 && ks->ch[1] >= 0xa1)
		{
			/* converting to KSC5601_1987 */
			ks->ch[0] &= 0x7f ;
			ks->ch[1] &= 0x7f ;
			
			ks->cs = KSC5601_1987 ;
			
			return  1 ;
		}
	}

	return  0 ;
}
