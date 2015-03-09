/*
 *	$Id$
 */

#include  "mkf_tg_map.h"

#include  <kiklib/kik_debug.h>

#include  "mkf_ucs4_map.h"
#include  "mkf_ucs4_iso8859.h"
#include  "mkf_ucs4_koi8.h"


static mkf_map_ucs4_to_func_t  map_ucs4_to_funcs[] =
{
	mkf_map_ucs4_to_koi8_t ,
	mkf_map_ucs4_to_iso8859_5_r ,
} ;


/* --- global functions --- */

int
mkf_map_ucs4_to_tg(
	mkf_char_t *  tg ,
	mkf_char_t *  ucs4
	)
{
	return  mkf_map_ucs4_to_with_funcs( tg , ucs4 , map_ucs4_to_funcs ,
		sizeof( map_ucs4_to_funcs) / sizeof( map_ucs4_to_funcs[0])) ;
}

int
mkf_map_koi8_t_to_iso8859_5_r(
	mkf_char_t *  iso8859 ,
	mkf_char_t *  tg
	)
{
	return  mkf_map_via_ucs( iso8859 , tg , ISO8859_5_R) ;
}
