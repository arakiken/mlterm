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
	u_int32_t  ucs4_code ;
	u_int16_t  iso8859_code ;

	mkf_char_t  ch ;

	iso8859_code = mkf_char_to_int( iso8859) ;

	if( mkf_map_iso8859_5_r_to_ucs4( &ch , iso8859_code) == 0)
	{
		return  0 ;
	}

	ucs4_code = mkf_char_to_int( &ch) ;

	if( mkf_map_ucs4_to_koi8_r( &ch , ucs4_code) == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG ,
			" u%.4x is not mapped to tcvn5712.\n" , ucs4_code) ;
	#endif

		return  0 ;
	}

	*ru = ch ;

	return  1 ;
}

int
mkf_map_koi8_r_to_iso8859_5_r(
	mkf_char_t *  iso8859 ,
	mkf_char_t *  ru
	)
{
	u_int16_t  ru_code ;
	u_int32_t  ucs4_code ;

	mkf_char_t  ch ;

	ru_code = mkf_char_to_int( ru) ;

	if( mkf_map_koi8_r_to_ucs4( &ch , ru_code) == 0)
	{
		return  0 ;
	}

	ucs4_code = mkf_char_to_int( &ch) ;

	if( mkf_map_ucs4_to_iso8859_5_r( &ch , ucs4_code) == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG ,
			" u%.4x is not mapped to koi8-r.\n" , ucs4_code) ;
	#endif

		return  0 ;
	}

	*iso8859 = ch ;

	return  1 ;
}

int
mkf_map_iso8859_5_r_to_koi8_u(
	mkf_char_t *  uk ,
	mkf_char_t *  iso8859
	)
{
	u_int32_t  ucs4_code ;
	u_int16_t  iso8859_code ;

	mkf_char_t  ch ;

	iso8859_code = mkf_char_to_int( iso8859) ;

	if( mkf_map_iso8859_5_r_to_ucs4( &ch , iso8859_code) == 0)
	{
		return  0 ;
	}

	ucs4_code = mkf_char_to_int( &ch) ;

	if( mkf_map_ucs4_to_koi8_u( &ch , ucs4_code) == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG ,
			" u%.4x is not mapped to tcvn5712.\n" , ucs4_code) ;
	#endif

		return  0 ;
	}

	*uk = ch ;

	return  1 ;
}

int
mkf_map_koi8_u_to_iso8859_5_r(
	mkf_char_t *  iso8859 ,
	mkf_char_t *  uk
	)
{
	u_int16_t  uk_code ;
	u_int32_t  ucs4_code ;

	mkf_char_t  ch ;

	uk_code = mkf_char_to_int( uk) ;

	if( mkf_map_koi8_u_to_ucs4( &ch , uk_code) == 0)
	{
		return  0 ;
	}

	ucs4_code = mkf_char_to_int( &ch) ;

	if( mkf_map_ucs4_to_iso8859_5_r( &ch , ucs4_code) == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG ,
			" u%.4x is not mapped to tcvn5712.\n" , ucs4_code) ;
	#endif

		return  0 ;
	}

	*iso8859 = ch ;

	return  1 ;
}
