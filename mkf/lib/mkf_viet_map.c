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
	return  mkf_map_via_ucs( tcvn , viscii , TCVN5712_3_1993) ;
}

int
mkf_map_tcvn5712_3_1993_to_viscii(
	mkf_char_t *  viscii ,
	mkf_char_t *  tcvn
	)
{
	return  mkf_map_via_ucs( viscii , tcvn , VISCII) ;
}
