/*
 *	$Id$
 */

#include  "mkf_ucs4_big5.h"

/*
 * these conversion tables are for BIG5 + HKSCS.
 */

#include  "table/mkf_big5_to_ucs4.table"

#include  "table/mkf_ucs4_alphabet_to_big5.table"
#include  "table/mkf_ucs4_cjk_to_big5.table"
#include  "table/mkf_ucs4_compat_to_big5.table"
#include  "table/mkf_ucs4_pua_to_big5.table"


/*
 * the same macro is defined in mkf_big5_parser.c
 */
#define  IS_HKSCS(code) \
	( (0x8140 <= (code) && (code) <= 0xa0fe) || \
	  (0xc6a1 <= (code) && (code) <= 0xc8fe) || \
	  (0xf9d6 <= (code) && (code) <= 0xfefe) )

#define  IS_HKSCS_ONLY(code) \
	( (0x8140 <= (code) && (code) <= 0xa0fe) || \
	  (0xf9d6 <= (code) && (code) <= 0xfefe) )


/* --- global functions --- */

int
mkf_map_big5_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  big5
	)
{
	u_char *  c ;

	if( IS_HKSCS_ONLY(big5))
	{
		return  0 ;
	}
	
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
mkf_map_hkscs_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  hkscs
	)
{
	u_char *  c ;

	if( ( c = CONV_BIG5_TO_UCS4(hkscs)))
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
		( c = CONV_UCS4_COMPAT_TO_BIG5(ucs4_code)) ||
		( c = CONV_UCS4_PUA_TO_BIG5(ucs4_code)))
	{
		u_int16_t  big5_code ;

		big5_code = mkf_bytes_to_int( c , 2) ;

		if( IS_HKSCS_ONLY(big5_code))
		{
			return  0 ;
		}
		
		memcpy( big5->ch , c , 2) ;
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
	u_char *  c ;

	if( ( c = CONV_UCS4_ALPHABET_TO_BIG5(ucs4_code)) ||
		( c = CONV_UCS4_CJK_TO_BIG5(ucs4_code)) ||
		( c = CONV_UCS4_COMPAT_TO_BIG5(ucs4_code)) ||
		( c = CONV_UCS4_PUA_TO_BIG5(ucs4_code)))
	{
		u_int16_t  hkscs_code ;

		hkscs_code = mkf_bytes_to_int( c , 2) ;

		if( ! IS_HKSCS(hkscs_code))
		{
			return  0 ;
		}
		
		memcpy( hkscs->ch , c , 2) ;
		hkscs->size = 2 ;
		hkscs->cs = HKSCS ;
		hkscs->property = 0 ;
		
		return  1 ;
	}

	return  0 ;
}
