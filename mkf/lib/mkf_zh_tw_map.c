/*
 *	$Id$
 */

#include  "mkf_zh_tw_map.h"

#include  <kiklib/kik_debug.h>

#include  "mkf_ucs4_map.h"
#include  "mkf_ucs4_cns11643.h"
#include  "mkf_ucs4_big5.h"


/* --- static variables --- */

static mkf_map_ucs4_to_func_t  map_ucs4_to_funcs[] =
{
	mkf_map_ucs4_to_big5 ,
	mkf_map_ucs4_to_cns11643_1992_1 ,
	mkf_map_ucs4_to_cns11643_1992_2 ,
	mkf_map_ucs4_to_cns11643_1992_3 ,
} ;


/* --- global functions --- */

int
mkf_map_ucs4_to_zh_tw(
	mkf_char_t *  zhtw ,
	mkf_char_t *  ucs4
	)
{
	return  mkf_map_ucs4_to_with_funcs( zhtw , ucs4 , map_ucs4_to_funcs ,
		sizeof( map_ucs4_to_funcs) / sizeof( map_ucs4_to_funcs[0])) ;
}

/*
 * BIG5 <=> CNS11643_1992_[1-2]
 */
 
int
mkf_map_big5_to_cns11643_1992(
	mkf_char_t *  cns ,
	mkf_char_t *  big5
	)
{
	mkf_char_t  ucs4 ;

	if( ! mkf_map_to_ucs4( &ucs4 , big5))
	{
		return  0 ;
	}
	
	if( ! mkf_map_ucs4_to_cs( cns , &ucs4 , CNS11643_1992_1) &&
		! mkf_map_ucs4_to_cs( cns , &ucs4 , CNS11643_1992_2))
	{
		return  0 ;
	}
	
	return  1 ;
}

int
mkf_map_cns11643_1992_1_to_big5(
	mkf_char_t *  big5 ,
	mkf_char_t *  cns
	)
{
	return  mkf_map_via_ucs( big5 , cns , BIG5) ;
}

int
mkf_map_cns11643_1992_2_to_big5(
	mkf_char_t *  big5 ,
	mkf_char_t *  cns
	)
{
	return  mkf_map_via_ucs( big5 , cns , BIG5) ;
}
