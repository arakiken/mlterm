/*
 *	$Id$
 */

#include  "mkf_ko_kr_map.h"

#include  "mkf_iso2022_intern.h"
#include  "mkf_ucs4_map.h"
#include  "mkf_ucs4_usascii.h"
#include  "mkf_ucs4_ksc5601.h"
#include  "mkf_ucs4_uhc.h"
#include  "mkf_ucs4_johab.h"
#include  "mkf_tblfunc_loader.h"


/*
 * johab <=> ucs4 conversion is so simple without conversion table that
 * ucs4 -> johab conversion is done first of all.
 */
static mkf_map_ucs4_to_func_t  map_ucs4_to_funcs[] =
{
	mkf_map_ucs4_to_us_ascii ,
	mkf_map_ucs4_to_johab ,
	mkf_map_ucs4_to_ksc5601_1987 ,
	mkf_map_ucs4_to_uhc ,

} ;


/* --- global functions --- */

int
mkf_map_ucs4_to_ko_kr(
	mkf_char_t *  kokr ,
	mkf_char_t *  ucs4
	)
{
	return  mkf_map_ucs4_to_with_funcs( kokr , ucs4 , map_ucs4_to_funcs ,
		sizeof( map_ucs4_to_funcs) / sizeof( map_ucs4_to_funcs[0])) ;
}
	
int
mkf_map_uhc_to_ksc5601_1987(
	mkf_char_t *  ksc ,
	mkf_char_t *  uhc
	)
{
	if( 0xa1 <= uhc->ch[0] && uhc->ch[0] <= 0xfe && 0xa1 <= uhc->ch[1] && uhc->ch[1] <= 0xfe)
	{
		ksc->ch[0] = UNMAP_FROM_GR( uhc->ch[0]) ;
		ksc->ch[1] = UNMAP_FROM_GR( uhc->ch[1]) ;
		ksc->size = 2 ;
		ksc->cs = KSC5601_1987 ;

		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

int
mkf_map_ksc5601_1987_to_uhc(
	mkf_char_t *  uhc ,
	mkf_char_t *  ksc
	)
{
	uhc->ch[0] = MAP_TO_GR( ksc->ch[0]) ;
	uhc->ch[1] = MAP_TO_GR( ksc->ch[1]) ;
	uhc->size = 2 ;
	uhc->cs = UHC ;

	return  1 ;
}

#ifdef  NO_DYNAMIC_LOAD_TABLE

#include  "../libtbl/mkf_uhc_johab.c"

#else

mkf_map_func2( kokr, mkf_map_johab_to_uhc)

mkf_map_func2( kokr, mkf_map_uhc_to_johab)

#endif
