/*
 *	$Id$
 */

#include  "mkf_ucs_property.h"

#include  <kiklib/kik_types.h>

/*
 * !! Notice !!
 * this must be size enough for all mkf_ucs_property_t enums.
 */
typedef u_int16_t  mkf_ucs_property_intern_t ;


/*
 * mkf_ucs_property_t => mkf_ucs_property_intern_t in these tables.
 * (for shrinking memory)
 */
#define  mkf_ucs_property_t  mkf_ucs_property_intern_t

#include  "table/mkf_ucs_alphabet_property.table"
#include  "table/mkf_ucs_extension_a_property.table"
#include  "table/mkf_ucs_cjk_property.table"
#include  "table/mkf_ucs_hangul_property.table"
#include  "table/mkf_ucs_compat_property.table"

#undef  mkf_ucs_property_t


/* --- global functions --- */

mkf_ucs_property_t
mkf_get_raw_ucs_property(
	u_char *  ch ,
	size_t  size
	)
{
	u_int32_t  ucs4 ;
	mkf_ucs_property_intern_t  prop ;
	
	if( size != 4)
	{
		return  0 ;
	}

	ucs4 = ((ch[0] << 24) & 0xff000000) | ((ch[1] << 16) & 0xff0000) |
		((ch[2] << 8) & 0xff00) | ch[3] ;

	if( (prop = GET_UCS_ALPHABET_PROPERTY( ucs4)) ||
		(prop = GET_UCS_EXTENSION_A_PROPERTY( ucs4)) ||
		(prop = GET_UCS_CJK_PROPERTY(ucs4)) ||
		(prop = GET_UCS_HANGUL_PROPERTY(ucs4)) ||
		(prop = GET_UCS_COMPAT_PROPERTY(ucs4)) )
	{
		return  (mkf_ucs_property_t)prop ;
	}

	return  0 ;
}

mkf_property_t
mkf_get_ucs_property(
	u_char *  ch ,
	size_t  size
	)
{
	mkf_ucs_property_t  ucs_prop ;

	if( ( ucs_prop = mkf_get_raw_ucs_property( ch , size)))
	{
		mkf_property_t  prop ;

		prop = 0 ;
		
		if( UCSPROP_GENERAL(ucs_prop) == MKF_UCSPROP_MC ||
			UCSPROP_GENERAL(ucs_prop) == MKF_UCSPROP_ME ||
			UCSPROP_GENERAL(ucs_prop) == MKF_UCSPROP_MN)
		{
			prop |= MKF_COMBINING ;
		}

		if( UCSPROP_EAST_ASIAN_WIDTH(ucs_prop) == MKF_UCSPROP_EAW_W ||
			UCSPROP_EAST_ASIAN_WIDTH(ucs_prop) == MKF_UCSPROP_EAW_F)
		{
			prop |= MKF_BIWIDTH ;
		}
		else if( UCSPROP_EAST_ASIAN_WIDTH(ucs_prop) == MKF_UCSPROP_EAW_A)
		{
			prop |= MKF_AWIDTH ;
		}

		return  prop ;
	}

	return  0 ;
}
