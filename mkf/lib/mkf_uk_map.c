/*
 *	$Id$
 */

#include  "mkf_uk_map.h"

#include  <kiklib/kik_debug.h>

#include  "mkf_ucs4_map.h"
#include  "mkf_ucs4_usascii.h"
#include  "mkf_ucs4_iso8859.h"
#include  "mkf_ucs4_koi8.h"


static mkf_map_ucs4_to_func_t  map_ucs4_to_funcs[] =
{
	mkf_map_ucs4_to_us_ascii ,
	mkf_map_ucs4_to_koi8_u ,
	mkf_map_ucs4_to_iso8859_5_r ,
} ;


/* --- global functions --- */

int
mkf_map_ucs4_to_uk(
	mkf_char_t *  uk ,
	mkf_char_t *  ucs4
	)
{
	return  mkf_map_ucs4_to_with_funcs( uk , ucs4 , map_ucs4_to_funcs ,
		sizeof( map_ucs4_to_funcs) / sizeof( map_ucs4_to_funcs[0])) ;
}

int
mkf_map_koi8_u_to_iso8859_5_r(
	mkf_char_t *  iso8859 ,
	mkf_char_t *  uk
	)
{
	return  mkf_map_via_ucs( iso8859 , uk , ISO8859_5_R) ;
}
