/*
 *	$Id$
 */

#include  "mkf_viet_map.h"

#include  <kiklib/kik_debug.h>

#include  "mkf_ucs4_map.h"
#include  "mkf_ucs4_usascii.h"
#include  "mkf_ucs4_viscii.h"
#include  "mkf_ucs4_iso8859.h"


/* --- static variables --- */

static mkf_map_ucs4_to_func_t  map_ucs4_to_funcs[] =
{
	mkf_map_ucs4_to_us_ascii ,
	mkf_map_ucs4_to_tcvn5712_3_1993 ,
	mkf_map_ucs4_to_viscii ,

} ;


/* --- global functions --- */

int
mkf_map_ucs4_to_viet(
	mkf_char_t *  viet ,
	mkf_char_t *  ucs4
	)
{
	return  mkf_map_ucs4_to_with_funcs( viet , ucs4 , map_ucs4_to_funcs ,
		sizeof( map_ucs4_to_funcs) / sizeof( map_ucs4_to_funcs[0])) ;
}

int
mkf_map_viscii_to_tcvn5712_3_1993(
	mkf_char_t *  tcvn ,
	mkf_char_t *  viscii
	)
{
	u_int16_t  viscii_code ;
	u_int32_t  ucs4_code ;

	mkf_char_t  ch ;

	viscii_code = mkf_char_to_int( viscii) ;

	if( mkf_map_viscii_to_ucs4( &ch , viscii_code) == 0)
	{
		return  0 ;
	}

	ucs4_code = mkf_char_to_int( &ch) ;

	if( mkf_map_ucs4_to_tcvn5712_3_1993( &ch , ucs4_code) == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG ,
			" u%.4x is not mapped to tcvn5712.\n" , ucs4_code) ;
	#endif

		return  0 ;
	}

	*tcvn = ch ;

	return  1 ;
}

int
mkf_map_tcvn5712_3_1993_to_viscii(
	mkf_char_t *  viscii ,
	mkf_char_t *  tcvn
	)
{
	u_int16_t  tcvn_code ;
	u_int32_t  ucs4_code ;
	mkf_char_t  ch ;

	tcvn_code = (u_int16_t) mkf_char_to_int( tcvn) ;

	if( mkf_map_tcvn5712_3_1993_to_ucs4( &ch , tcvn_code) == 0)
	{
		return  0 ;
	}

	ucs4_code = mkf_char_to_int( &ch) ;

	if( mkf_map_ucs4_to_viscii( &ch , ucs4_code) == 0)
	{
		return  0 ;
	}

	*viscii = ch ;

	return  1 ;
}
