/*
 *	$Id$
 */

/*
 * about KOI8-R <-> KOI8-U uncompatibility , see rfc2319.
 */

#include  "mkf_ru_map.h"

#include  <kiklib/kik_debug.h>

#include  "mkf_ucs4_map.h"
#include  "mkf_ucs4_usascii.h"
#include  "mkf_ucs4_iso8859.h"
#include  "mkf_ucs4_koi8.h"


#define  IS_NOT_COMPAT_AREA_OF_KOI8_R_U(ch) \
	(ch == 0xa4 || ch == 0xa6 || ch == 0xa7 || ch == 0xad || ch == 0xb4 || ch == 0xb6 || \
		ch == 0xb7 || ch == 0xbd)


static mkf_map_ucs4_to_func_t  map_ucs4_to_funcs[] =
{
	mkf_map_ucs4_to_us_ascii ,
	mkf_map_ucs4_to_koi8_r ,
	mkf_map_ucs4_to_koi8_u ,
	mkf_map_ucs4_to_iso8859_5_r ,
} ;


/* --- global functions --- */

int
mkf_map_ucs4_to_ru(
	mkf_char_t *  ru ,
	mkf_char_t *  ucs4
	)
{
	return  mkf_map_ucs4_to_with_funcs( ru , ucs4 , map_ucs4_to_funcs ,
		sizeof( map_ucs4_to_funcs) / sizeof( map_ucs4_to_funcs[0])) ;
}

int
mkf_map_koi8_u_to_koi8_r(
	mkf_char_t *  ru ,
	mkf_char_t *  uk
	)
{
	if( IS_NOT_COMPAT_AREA_OF_KOI8_R_U(uk->ch[0]))
	{
		return  0 ;
	}

	*ru = *uk ;
	ru->cs = KOI8_R ;

	return  1 ;
}

int
mkf_map_koi8_r_to_koi8_u(
	mkf_char_t *  uk ,
	mkf_char_t *  ru
	)
{
	if( IS_NOT_COMPAT_AREA_OF_KOI8_R_U(uk->ch[0]))
	{
		return  0 ;
	}

	*uk = *ru ;
	ru->cs = KOI8_U ;

	return  1 ;
}

int
mkf_map_iso8859_5_r_to_koi8_r(
	mkf_char_t *  ru ,
	mkf_char_t *  iso8859
	)
{
	return  mkf_map_via_ucs( ru , iso8859 , KOI8_R) ;
}

int
mkf_map_koi8_r_to_iso8859_5_r(
	mkf_char_t *  iso8859 ,
	mkf_char_t *  ru
	)
{
	return  mkf_map_via_ucs( iso8859 , ru , ISO8859_5_R) ;
}

int
mkf_map_iso8859_5_r_to_koi8_u(
	mkf_char_t *  uk ,
	mkf_char_t *  iso8859
	)
{
	return  mkf_map_via_ucs( uk , iso8859 , KOI8_U) ;
}

int
mkf_map_koi8_u_to_iso8859_5_r(
	mkf_char_t *  iso8859 ,
	mkf_char_t *  uk
	)
{
	return  mkf_map_via_ucs( iso8859 , uk , KOI8_U) ;
}
