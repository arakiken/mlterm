/*
 *	update: <2001/11/26(02:45:47)>
 *	$Id$
 */

#include  "mkf_ucs_property.h"

#include  "table/mkf_ucs_alphabet_property.table"
#include  "table/mkf_ucs_extension_a_property.table"
#include  "table/mkf_ucs_cjk_property.table"
#include  "table/mkf_ucs_hangul_property.table"
#include  "table/mkf_ucs_compat_property.table"


/* --- global functions --- */

mkf_ucs_property_t
mkf_get_ucs_property(
	u_char *  ch ,
	size_t  size
	)
{
	u_int32_t  ucs4 ;
	mkf_ucs_property_t  val ;

	if( size != 4)
	{
		return  0 ;
	}

	ucs4 = ((ch[0] << 24) & 0xff000000) | ((ch[1] << 16) & 0xff0000) |
		((ch[2] << 8) & 0xff00) | ch[3] ;

	if( (val = GET_UCS_ALPHABET_PROPERTY( ucs4)) ||
		(val = GET_UCS_EXTENSION_A_PROPERTY( ucs4)) ||
		(val = GET_UCS_CJK_PROPERTY(ucs4)) ||
		(val = GET_UCS_HANGUL_PROPERTY(ucs4)) ||
		(val = GET_UCS_COMPAT_PROPERTY(ucs4)))
	{
		return  val ;
	}

	return  0 ;
}
