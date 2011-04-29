/*
 *	$Id$
 */

#include  "mkf_ucs4_iscii.h"


/* --- static functions --- */

static mkf_iscii_lang_t  iscii_lang = ISCIILANG_MALAYALAM ;


/* --- global functions --- */

int
mkf_set_iscii_lang_for_ucs_mapping(
	mkf_iscii_lang_t  lang
	)
{
	iscii_lang = lang ;

	return  1 ;
}

int
mkf_map_iscii_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  iscii_code
	)
{
	if( iscii_code <= 0x7e)
	{
		ucs4->ch[0] = 0x0 ;
		ucs4->ch[1] = 0x0 ;
		ucs4->ch[2] = 0x0 ;
		ucs4->ch[3] = iscii_code ;
		ucs4->size = 4 ;
		ucs4->cs = ISO10646_UCS4_1 ;
		ucs4->property = 0 ;

		return  1 ;
	}
	else
	{
		u_int32_t  ucs_code ;

		switch( iscii_lang)
		{
		case ISCIILANG_BENGALI:
			ucs_code = iscii_code + 0x900 ;
			break ;

		case ISCIILANG_GUJARATI:
			ucs_code = iscii_code + 0xa00 ;
			break ;

		case ISCIILANG_HINDI:
			ucs_code = iscii_code + 0x880 ;
			break ;

		case ISCIILANG_KANNADA:
			ucs_code = iscii_code + 0xc00 ;
			break ;

		default:
		case ISCIILANG_MALAYALAM:
			ucs_code = iscii_code + 0xc80 ;
			break ;

		case ISCIILANG_ORIYA:
			ucs_code = iscii_code + 0xa80 ;
			break ;

		case ISCIILANG_TAMIL:
			ucs_code = iscii_code + 0xb00 ;
			break ;

		case ISCIILANG_TELUGU:
			ucs_code = iscii_code + 0xb80 ;
			break ;
		}

		mkf_int_to_bytes( ucs4->ch , 4 , ucs_code) ;
		ucs4->size = 4 ;
		ucs4->cs = ISO10646_UCS4_1 ;
		ucs4->property = 0 ;

		return  1 ;
	}
}

int
mkf_map_ucs4_to_iscii(
	mkf_char_t *  non_ucs ,
	u_int32_t  ucs4_code
	)
{
	if( 0x900 <= ucs4_code && ucs4_code <= 0x97f)
	{
		non_ucs->ch[0] = ucs4_code - 0x880 ;
		non_ucs->ch[1] = ISCIILANG_HINDI ;		/* Hidden info */
	}
	else if( 0x980 <= ucs4_code && ucs4_code <= 0x9ff)
	{
		non_ucs->ch[0] = ucs4_code - 0x900 ;
		non_ucs->ch[1] = ISCIILANG_BENGALI ;	/* Hidden info */
	}
	else if( 0xa80 <= ucs4_code && ucs4_code <= 0xaff)
	{
		non_ucs->ch[0] = ucs4_code - 0xa00 ;
		non_ucs->ch[1] = ISCIILANG_GUJARATI ;	/* Hidden info */
	}
	else if( 0xb00 <= ucs4_code && ucs4_code <= 0xb7f)
	{
		non_ucs->ch[0] = ucs4_code - 0xa80 ;
		non_ucs->ch[1] = ISCIILANG_ORIYA ;		/* Hidden info */
	}
	else if( 0xb80 <= ucs4_code && ucs4_code <= 0xbff)
	{
		non_ucs->ch[0] = ucs4_code - 0xb00 ;
		non_ucs->ch[1] = ISCIILANG_TAMIL ;		/* Hidden info */
	}
	else if( 0xc00 <= ucs4_code && ucs4_code <= 0xc7f)
	{
		non_ucs->ch[0] = ucs4_code - 0xb80 ;
		non_ucs->ch[1] = ISCIILANG_TELUGU ;	/* Hidden info */
	}
	else if( 0xc80 <= ucs4_code && ucs4_code <= 0xcff)
	{
		non_ucs->ch[0] = ucs4_code - 0xc00 ;
		non_ucs->ch[1] = ISCIILANG_KANNADA ;	/* Hidden info */
	}
	else if( 0xd00 <= ucs4_code && ucs4_code <= 0xd7f)
	{
		non_ucs->ch[0] = ucs4_code - 0xc80 ;
		non_ucs->ch[1] = ISCIILANG_MALAYALAM ;	/* Hidden info */
	}
	else
	{
		return  0 ;
	}

	non_ucs->size = 1 ;
	non_ucs->cs = ISCII ;
	non_ucs->property = 0 ;

	return  1 ;
}
