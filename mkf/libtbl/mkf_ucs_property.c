/*
 *	$Id$
 */

#include  "../lib/mkf_ucs_property.h"

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
	u_int32_t  ucs
	)
{
	mkf_ucs_property_intern_t  prop ;
	
	if( (prop = GET_UCS_ALPHABET_PROPERTY( ucs)) ||
		(prop = GET_UCS_EXTENSION_A_PROPERTY( ucs)) ||
		(prop = GET_UCS_CJK_PROPERTY(ucs)) ||
		(prop = GET_UCS_HANGUL_PROPERTY(ucs)) ||
		(prop = GET_UCS_COMPAT_PROPERTY(ucs)) )
	{
		return  (mkf_ucs_property_t)prop ;
	}

	return  0 ;
}
