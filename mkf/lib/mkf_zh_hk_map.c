/*
 *	$Id$
 */

#include  "mkf_zh_hk_map.h"

#include  "mkf_ucs4_map.h"
#include  "mkf_ucs4_big5.h"


/* --- static variables --- */

static mkf_map_ucs4_to_func_t  map_ucs4_to_funcs[] =
{
	mkf_map_ucs4_to_hkscs ,
	mkf_map_ucs4_to_big5 ,
} ;


/* --- global functions --- */

int
mkf_map_ucs4_to_zh_hk(
	mkf_char_t *  zhhk ,
	mkf_char_t *  ucs4
	)
{
	return  mkf_map_ucs4_to_with_funcs( zhhk , ucs4 , map_ucs4_to_funcs ,
		sizeof( map_ucs4_to_funcs) / sizeof( map_ucs4_to_funcs[0])) ;
}
