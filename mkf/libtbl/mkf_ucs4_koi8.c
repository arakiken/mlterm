/*
 *	$Id$
 */

#include  "../lib/mkf_ucs4_koi8.h"

#include  "table/mkf_koi8_r_to_ucs4.table"
#if  0
/* Not implemented yet */
#include  "table/mkf_ucs4_to_koi8_r.table"
#endif

#include  "table/mkf_koi8_t_to_ucs4.table"
#include  "table/mkf_ucs4_to_koi8_t.table"


/* --- global functions --- */

int
mkf_map_koi8_r_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  koi8_code
	)
{
	u_int32_t  c ;

	if( ( c = CONV_KOI8_R_TO_UCS4(koi8_code)))
	{
		mkf_int_to_bytes( ucs4->ch , 4 , c) ;
		ucs4->size = 4 ;
		ucs4->cs = ISO10646_UCS4_1 ;
		ucs4->property = 0 ;

		return  1 ;
	}
	else if( /* 0x00 <= koi8_code && */ koi8_code <= 0x7f)
	{
		ucs4->ch[0] = 0x0 ;
		ucs4->ch[1] = 0x0 ;
		ucs4->ch[2] = 0x0 ;
		ucs4->ch[3] = koi8_code ;
		ucs4->size = 4 ;
		ucs4->cs = ISO10646_UCS4_1 ;
		ucs4->property = 0 ;

		return  1 ;
	}

	return  0 ;
}

int
mkf_map_koi8_u_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  koi8_code
	)
{
	/*
	 * about KOI8-R <-> KOI8-U incompatibility , see rfc2319.
	 * note that the appendix A of rfc2319 is broken.  
	 */
	if( koi8_code == 0xa4 || koi8_code == 0xa6 || koi8_code == 0xa7)
	{
		ucs4->ch[3] = 0x54 + (koi8_code - 0xa4) ;
	}
	else if( koi8_code == 0xb6 || koi8_code == 0xb7)
	{
		ucs4->ch[3] = 0x06 + (koi8_code - 0xb6) ;
	}
	else if( koi8_code == 0xad)
	{
		ucs4->ch[3] = 0x91 ;
	}
	else if( koi8_code == 0xb4)
	{
		ucs4->ch[3] = 0x04 ;
	}
	else if( koi8_code == 0xbd)
	{
		ucs4->ch[3] = 0x90 ;
	}
	else if( mkf_map_koi8_r_to_ucs4( ucs4 , koi8_code))
	{
		return  1 ;
	}
	else
	{
		return  0 ;
	}

	ucs4->ch[0] = 0x0 ;
	ucs4->ch[1] = 0x0 ;
	ucs4->ch[2] = 0x04 ;

	ucs4->size = 4 ;
	ucs4->cs = ISO10646_UCS4_1 ;
	ucs4->property = 0 ;

	return  1 ;
}

int
mkf_map_koi8_t_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  koi8_code
	)
{
	u_int32_t  c ;

	if( ( c = CONV_KOI8_T_TO_UCS4(koi8_code)))
	{
		mkf_int_to_bytes( ucs4->ch , 4, c) ;
		ucs4->size = 4 ;
		ucs4->cs = ISO10646_UCS4_1 ;
		ucs4->property = 0 ;

		return  1 ;
	}
	else if( /* 0x00 <= koi8_code && */ koi8_code <= 0x7f)
	{
		ucs4->ch[0] = 0x0 ;
		ucs4->ch[1] = 0x0 ;
		ucs4->ch[2] = 0x0 ;
		ucs4->ch[3] = koi8_code ;
		ucs4->size = 4 ;
		ucs4->cs = ISO10646_UCS4_1 ;
		ucs4->property = 0 ;

		return  1 ;
	}

	return  0 ;
}

int
mkf_map_ucs4_to_koi8_r(
	mkf_char_t *  non_ucs ,
	u_int32_t  ucs4_code
	)
{
	u_int8_t  offset ;
	
	for( offset = 0 ; offset <= koi8_r_to_ucs4_end - koi8_r_to_ucs4_beg ; offset ++)
	{
		if( koi8_r_to_ucs4_table[offset] == (u_int16_t)ucs4_code)
		{
			non_ucs->ch[0] = offset + koi8_r_to_ucs4_beg ;
			non_ucs->size = 1 ;
			non_ucs->cs = KOI8_R ;
			non_ucs->property = 0 ;

			return  1 ;
		}
	}
	
	return  0 ;
}

int
mkf_map_ucs4_to_koi8_u(
	mkf_char_t *  non_ucs ,
	u_int32_t  ucs4_code
	)
{
	/*
	 * about KOI8-R <-> KOI8-U incompatibility , see rfc2319.
	 * note that the appendix A of rfc2319 is broken. 
	 */
	if( ucs4_code == 0x454 || ucs4_code == 0x456 || ucs4_code == 0x457)
	{
		non_ucs->ch[0] = 0xa4 + ucs4_code - 0x454 ;
	}
	else if( ucs4_code == 0x406 || ucs4_code == 0x407)
	{
		non_ucs->ch[0] = 0xb6 + ucs4_code - 0x406 ;
	}
	else if( ucs4_code == 0x491)
	{
		non_ucs->ch[0] = 0xad ;
	}
	else if( ucs4_code == 0x404)
	{
		non_ucs->ch[0] = 0xb4 ;
	}
	else if( ucs4_code == 0x490)
	{
		non_ucs->ch[0] = 0xbd ;
	}
	else if( mkf_map_ucs4_to_koi8_r( non_ucs , ucs4_code))
	{
		non_ucs->cs = KOI8_U ;
	
		return  1 ;
	}
	else
	{
		return  0 ;
	}

	non_ucs->size = 1 ;
	non_ucs->cs = KOI8_U ;
	non_ucs->property = 0 ;

	return  1 ;
}

int
mkf_map_ucs4_to_koi8_t(
	mkf_char_t *  non_ucs ,
	u_int32_t  ucs4_code
	)
{
	u_int8_t  c ;

	if( ( c = CONV_UCS4_TO_KOI8_T(ucs4_code)))
	{
		non_ucs->ch[0] = c ;
		non_ucs->size = 1 ;
		non_ucs->cs = KOI8_T ;
		non_ucs->property = 0 ;

		return  1 ;
	}
	
	return  0 ;
}
